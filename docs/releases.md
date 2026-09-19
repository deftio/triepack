---
layout: default
title: Releases
---

<!-- GENERATED from CHANGELOG.md by scripts/sync_changelog.sh.
     Do not edit: edit CHANGELOG.md and rerun the script. -->

# Releases

What changed in each version. Downloads are on the
[GitHub Releases page](https://github.com/deftio/triepack/releases);
the full history lives in
[CHANGELOG.md](https://github.com/deftio/triepack/blob/main/CHANGELOG.md).

## v1.3.2 — 2026-09-16

### Added
- **Sanitizer build and CI job.** `-DENABLE_SANITIZERS=ON` builds with
  AddressSanitizer and UndefinedBehaviorSanitizer, and CI runs the whole suite
  under both on every push, with LeakSanitizer on. The format parses untrusted
  bytes and several tests deliberately walk corrupted dictionaries; those only
  prove anything when a bad read aborts instead of quietly returning garbage.
  The option refuses to combine with `ENABLE_COVERAGE`, whose instrumentation
  it fights.
- `scripts/test-ci-linux.sh` grew `sanitizers` and `coverage` targets, so both
  new CI jobs can be run locally. LeakSanitizer is Linux-only, which makes the
  container the only place the leak half runs at all.
- **`tests/test_scale.c`** — large dictionaries, tiered by `TRIEPACK_SCALE_MB`
  so the default run stays quick and CI can raise it. Covers round-trip,
  ordered iteration and lookup latency at size, and asserts that passing the
  v1 512 MB ceiling is a clean refusal rather than a corrupt file. Measuring
  lookup at scale is what exposed how steep the O(n) value scan is: 328 µs/key
  at 10k keys, 7,524 µs at 175k, 30,500 µs at 699k.
- **`tools/v2_prototype.c`** — a working implementation of the proposed v2
  core (radix trie, LOUDS with rank/select, suffix-merged tail pool, terminal
  bitmap) that verifies every key through its own serialised bytes. Registered
  as the `v2_prototype_roundtrip` test.

  It falsified the spec on its first run: at 57,507 bytes it was 28% *worse*
  than v1, because storing a `(pool offset, length)` pair per tail occurrence
  spent 35,280 bytes pointing into a 2,064-byte pool. Indexing the distinct
  tail instead, and coding labels at the alphabet width rather than a byte
  each, brings it to **27,610 bytes — 39% smaller than v1** on the same
  corpus, with all 10,000 keys verified. Both corrections are now in the spec.

  With values implemented (§9: ordinal derived by rank over the terminal
  bitmap, located through a sampled offset index) it closes v1's worst defect
  outright. On the same corpus: **48,734 bytes against v1's 90,451 (−46%) and
  2.06 µs/key against 328 µs (159× faster)**. The value fetch itself costs
  0.04 µs. At 699k keys, where v1 takes 30,500 µs per lookup, v2 takes
  **3.17 µs** — v1 grows 93× across that range, v2 grows 1.5×.

  It also answers the Huffman question with data rather than assumption: a
  second pass over the byte sections saves 7.6% on the word corpus, 29.0% on
  path-like keys and 0.1% on random binary keys. It is therefore specified as
  optional and per-section, with the encoder required to measure and keep the
  smaller result.
- **`src/v2/v2_bitvec.c`, `include/triepack/v2_bitvec.h`** — the first piece of
  v2 built properly: the ranked bit vector and LOUDS navigation that the tree,
  the terminal map and the tail map all stand on. Written test-first, with
  `tests/test_v2_bitvec.c` and `tests/test_v2_louds.c` derived from the
  specification rather than from the code, so the tests are the authority.

  The discipline paid immediately. `test_rank_matches_brute_force_across_block_boundaries`
  probes 255/256/257 and 2047/2048/2049, and found that where a superblock
  boundary lands exactly at the end of the vector the final superblock and
  block entries were written as separate "terminators" holding the same count
  — `rank1(2048)` returned 1366 instead of 683. The prototype had the same
  bug and never hit it, because no corpus happened to end on that boundary.
- **`tools/compare_formats.py`** — one table covering every corpus and every
  format: raw, triepack v1 and v2, v2 recompressed with gzip and with
  byte-level Huffman, MessagePack, BSON and gzip, in bytes and as a ratio,
  with key and value counts and a refusals section naming what each format
  declined and why. `--full` adds the whole 1 GB enwik9.

  It immediately exposed a real defect: **v2 was larger than v1 on path-like
  keys** (361,567 against 306,178). Labels were alphabet-coded but the tail
  pool was left as raw bytes, and both hold the same alphabet — on that corpus
  the pool is 56% of the file, so 8 bits where 5 would do cost 85,141 bytes.
  Coding the pool brings it to **276,426**, ahead of v1. Per-corpus tables had
  never put those two numbers side by side.

  Stacking a general compressor was measured rather than assumed: gzip on the
  `.trp` beats gzip on the original **only for the word list** (15,863 against
  27,973); everywhere else gzipping the original wins, because the trie has
  already taken the redundancy LZ77 would have found. Byte-level Huffman never
  beats gzip and barely beats the uncompressed file.
- **`docs/comparisons.md`** — measured against BSON, MessagePack and gzip on
  `benchmark_100k.json`, and against the trie libraries architecturally.
  gzip wins outright at 13.13x for a document read whole, and the page says
  so; where TriePack wins is repeated keys and random access. BSON spends
  **31% of its bytes (45,476 of 144,545) on field names**, storing each in
  full once per occurrence; v2 stores the whole flattened key set in 13,116
  bytes — 9.87x against raw key text, 3.47x against BSON's names — because
  5,974 tail occurrences collapse to 41 distinct tails.
- **`tests/test_robustness.c`** — runs the library over whatever real file is
  in the gitignored `data/`, currently `enwik9`. The assertion is that it must
  not die: every outcome is acceptable except a crash, a hang or a silently
  wrong answer, and refusing the input is a fine answer. Skips cleanly when
  `data/` is empty, as in CI.
- **`src/v2/v2_tails.c`** — the §8.1 suffix-merge algorithm, written
  test-first. `tests/test_v2_tails.c` pins the spec's worked example byte for
  byte and asserts the property byte-identity actually rests on: that output
  does not depend on the order tails arrive in.
- **`docs/status.md`** — an audit of the gap between what TriePack documents
  and what it implements. The architecture page described a two-trie design,
  three addressing modes and Huffman symbol selection, none of which exist;
  two of three checksum algorithms in a public enum have no implementation.
  Documented features that were never built are now listed as such, with the
  known defects and hard limits beside them.
- **Direction documents** — `docs/triepack-northstar.md` (goals, non-goals and
  the rules a format change must obey), `docs/internals/format-spec-v2.md` (a
  proposed successor format with no alphabet limit, suffix sharing, structural
  value indexing and 64-bit addressing) and
  `docs/internals/v2-implementation-plan.md` (sequencing across ten
  implementations, with measured baselines to beat). `architecture.md`,
  `triepack-technical-doc.md` and `index.md` now mark design intent as design
  intent rather than asserting it in the present tense.
- **`tests/test_core_iterate.c`** — iteration and prefix descent, the code
  issue #1 got wrong. Covers dictionaries with no value store, keys and
  prefixes longer than the iterator's 256-byte buffer, a prefix that is itself
  a stored key, nesting past the frame stack, a header that disagrees with its
  own trie, and a sweep that flips every bit of the body in turn.
- Corruption sweeps in `test_json_decode.c`, repairing the CRC after each flip
  so the trie walk is what has to catch the damage rather than the checksum.
- C++ wrapper tests for moved-from handles, the length-taking `add` overload,
  blob equality, and the alphabet limit surfacing through `build()`.
- Python tests for the alphabet limit — reachable there because the Python
  encoder accepts raw `bytes` keys — and for the BRANCH-after-terminal check.
- JavaScript tests pinning the invariant that makes its alphabet guard
  unreachable: keys are strings, so the bytes are always UTF-8, which spans at
  most 243 distinct values against a ceiling of 249.

### Changed
- The coverage workflow now prints a per-file table into the job summary and
  fails below 97% lines or 80% branches. It previously generated a report and
  uploaded it as an artifact that nothing read.
- Documented the iterator's 256-frame depth limit, which stops a pathologically
  nested dictionary with `TP_ERR_OVERFLOW`; lookup is not frame-bound and still
  reaches those keys.

### Fixed
- **Encoder options that were accepted and ignored.** `tp_encoder_options`
  has six fields; the encoder read one (`bits_per_symbol`) and silently
  dropped the rest, so a caller requesting SHA-256 got CRC-32, and one
  enabling the suffix table got a plain trie, with no way to find out. Each
  unimplemented option now returns `TP_ERR_UNSUPPORTED`. A test had codified
  the old behaviour as correct and has been rewritten. The fields and enum
  values stay until 2.0, since removing them breaks the API.
- **Silent corruption past 512 MB.** `trie_data_offset`, `value_store_offset`
  and `total_data_bits` are computed as 64-bit quantities and stored as 32-bit
  *bit* counts, so a data stream over 2^32 bits truncated them, wrote a valid
  CRC over the wrong offsets, and produced a file that opened cleanly and
  decoded to garbage — the same failure mode as the alphabet overflow, from
  the same cause. The encoder now returns `TP_ERR_OVERFLOW` instead. Lifting
  the ceiling (rather than reporting it) needs the wider header of format v2.
- **Unbounded allocation from a corrupt length field (Rust, Python,
  JavaScript).** String and blob lengths are read from the file, and
  `read_bytes` allocated that many bytes before checking the stream had them.
  A corrupt length could claim gigabytes: Rust's `vec![0; n]` **aborted the
  process** (SIGABRT), Python raised `MemoryError` and JavaScript a
  `RangeError` — none of them the format's own error, and all of them a
  denial of service on untrusted input. All three now check the declared
  length against what is left before allocating. The C implementation was
  never affected: it hands back a pointer into the caller's buffer through
  `tp_bs_reader_direct_ptr`, which bounds-checks and never allocates.
  Found by CI on Linux; macOS had absorbed the oversized allocation silently.
- **Memory leak decoding a truncated or corrupt JSON dictionary.**
  `extract_entries` allocates each key separately; its error path freed the
  entry array but not the keys, so every key the walk got through before the
  input went bad was leaked. Found by the new sanitizer job — the existing
  `test_json_decode_progressive_truncation` had been exercising the leak since
  it was written, with nothing watching for it.
- `tools/check-coverage.sh` did not clear gcov counters between runs, so a
  second run reported the union of both and gave a different number from the
  first. It also defaulted to a 100% threshold it could no longer meet, called
  the project "TXZ", and used GNU-only `grep -oP`, which fails on macOS.
- `scripts/test-ci-linux.sh` piped `ctest` into `tail`, so the pipeline
  reported `tail`'s exit status and a failing job looked like a passing one.
  That is why the leak above went unnoticed on the first sanitizer run. All
  four container jobs now set `pipefail`.
- The testing guide claimed 100% line coverage for C/C++, Python and
  JavaScript. Measured, it was 97.9%, 99% and 99.6%. Python and JavaScript are
  now genuinely at 100%; C/C++ is at 99.5% lines and 100% of functions, and the
  guide says what the remainder is and why.
- Removed `package-manager-cache` from `setup-node@v4` in `publish.yml`. It is
  a v5 input, ignored with a warning on v4, and there is no lockfile to cache.

## v1.3.1 — 2026-09-16

### Fixed
- **The release could never publish.** `check_versions.sh` refused whenever a
  tag for the declared version existed — but `publish.yml` is *triggered* by
  that tag, so it always did. v1.3.0 was tagged and every publishing job was
  skipped. The check now takes `--tagged` for the case where the tag is
  expected, confirming it points at the commit being built instead of
  demanding it be absent, and still refusing if a registry already has the
  version.
- The GitHub release check reported "gh unavailable, skipped" on runners,
  because `gh` needs a token. The workflow now passes one.

### Added
- **PyPI publishing** from `pypi.yml`, by OIDC trusted publishing. PyPI matches
  its trusted publisher on the workflow filename, as npm does, so the two
  registries need separate entry points. The gate they both have to pass —
  version consistency plus the whole cross-language matrix — moved into
  `_release-tests.yml` and is called by both, so it stays defined once and
  neither registry can be published to on a red build.
- `check_versions.sh` now holds PyPI to the same rule as npm rather than just
  reporting it, including yanked releases, which still occupy a version.
- npm and PyPI version badges in the README, linking to each package.
- Badge colours chosen against WCAG AA rather than by eye. Shields' named
  colours are light, and the coverage badge was white text on `brightgreen` at
  2.12:1 — well under the 4.5:1 AA needs for normal text. The four coverage
  steps keep their meaning at 5.1:1 to 6.5:1, and the licence badge moves from
  4.38:1 to 6.91:1.
- Branch protection on `main`: force pushes and deletion blocked, and the
  sixteen checks that run on every pull request required before merging. The
  release-only checks are deliberately not required, since they never run on a
  pull request and would block every merge.

## v1.3.0 — 2026-09-16

### Added
- **`scripts/check_versions.sh`** — reconciles the declared version against
  everything already published: git tags, GitHub releases, npm, and the
  registries not published to yet. `sync_version.sh` only checks that the
  repository agrees with itself, which cannot see a version published by hand,
  a tag that was never pushed, or a release cut from a different tree. 1.2.0
  reached npm while the latest tag and GitHub release were still v1.1.0, and
  nothing in the tooling noticed. The release gate and `publish.yml` now refuse
  a version that is not strictly newer than everything published; CI reports
  the same picture on every pull request without failing on it.

### Changed
- `scripts/make-release.sh` takes `--merge squash|merge|rebase`. Squash suits a
  release PR that is only a version bump, and flattens a branch carrying real
  work; squashing more than three commits now warns and asks first.

### Note
- npm `triepack@1.2.0` was published by hand from a tree that predates the
  `version()` API, so it is the one artefact that does not match the 1.2.0
  source. 1.3.0 is the first version published through CI.

## v1.2.0 — 2026-09-16

### Fixed
- **Decode could throw EOF for some key sets** (#1) -- the trie walk in the
  native bindings decided whether a BRANCH followed a terminal by peeking at
  the next `bits_per_symbol` bits. Past the trie's last terminal those bits
  are the byte padding and the CRC, which for some key sets happen to equal
  the BRANCH code; the walker then followed a branch that isn't there and ran
  off the end of the buffer. The walk is now bounded by the trie extent the
  header already declares (`value_store_offset`, and each child's SKIP
  distance), so a terminal is followed by a BRANCH exactly when the subtree
  has not reached its end. Affected the JavaScript, Python, Go, Rust, Swift,
  Java and Kotlin bindings; the C core walks by key count and was unaffected.
  Header fields were correct throughout -- the 2-bit "slack" in the report is
  the data section's byte-alignment padding.
- **Alphabets of 250+ distinct bytes produced unreadable dictionaries** --
  `symbol_count` is an 8-bit header field holding the alphabet plus the six
  control codes, so an alphabet past 249 overflowed it. The encoder reported
  success, the CRC validated, and every lookup then failed silently. All
  implementations now refuse to build such a dictionary (`TP_ERR_ALPHABET`,
  `TP_MAX_ALPHABET_SIZE`), and readers reject a trie config whose
  `bits_per_symbol` or `symbol_count` is out of range. Reachable from C and Go,
  whose keys are arbitrary bytes; UTF-8-only bindings cap out around 243.
- **Zero-length blob values freed the caller's memory** (C) -- `value_deep_copy`
  skipped blobs of length 0 while `value_free_copy` still freed the pointer, so
  `tp_encoder_add` with `tp_value_blob(ptr, 0)` left the encoder owning memory
  it never copied.
- **The full 64-bit integer range did not survive a round trip** in four
  bindings: Go held varints in `int` (`uint64` values above `MaxInt64` went
  negative), Swift trapped negating `Int64.min` for the zigzag, and Java and
  Kotlin both overflowed the same negation and rejected `uint64` values above
  `Long.MAX_VALUE`. All four now zigzag on the bit pattern and treat the
  unsigned range as unsigned. JavaScript, which holds integers in a double,
  now raises a `RangeError` instead of writing bytes that decode to a
  different number.
- **`tp_iter_next` was a stub that always returned EOF**, so iteration and
  prefix search returned nothing in C and, through it, in the C++ wrapper.
  Both are now implemented: a resumable bounded trie walk yielding keys in
  lexicographic order, and a prefix search that descends to the subtree
  instead of scanning.
- **Kotlin did not compile** with kotlinc 1.9.24: inside `TpValue`, the bare
  `Int` in `Blob.hashCode()` resolved to the nested `TpValue.Int`.

### Changed
- **Binding CI can now fail.** Every job in `bindings.yml` carried
  `continue-on-error: true`, so a broken binding never turned the build red;
  Java and Kotlin had no jobs at all. Both are fixed, which is what makes
  "publish only on green CI" mean anything.
- Every binding README said "Not yet implemented". Rewritten with real
  install and usage instructions — the JavaScript and Python ones are the npm
  and PyPI landing pages.
- **The C++ wrapper is a complete API rather than an int32-only stub.** New
  `triepack::Value` (an owning tagged value for all eight format types),
  `Status` mirroring `tp_result`, `Encoder::add`/`build` (which writes into a
  caller-owned `std::vector` instead of handing back memory to `free`), and a
  working `Iterator` with prefix support. The old `Encoder::insert` and
  `Encoder::encode` are gone; `insert` always used the signed tag, so its
  output differed from every other implementation for non-negative values.
- `tp_dict_find_fuzzy` returns `TP_ERR_UNSUPPORTED` instead of an iterator
  over every key. It was never implemented; now that iteration works, the old
  behaviour would have looked like a successful fuzzy match for anything.

### Added
- **`triepack-version.txt`, one source of truth for the release version.**
  CMake reads it directly and the CLI prints the header CMake generates from
  it; `scripts/sync_version.sh` propagates it to every package manifest, every
  binding's `VERSION` constant, the docs site header and the README, with
  `--check` failing on drift. CI runs `--check` on every push, and each
  binding's tests read the file and assert that what they report matches, so a
  stale constant fails in that language rather than shipping. All bindings move
  from 0.1.0 to match the C library. The on-disk *format* version is
  deliberately not synced.
- **`version()` in every implementation** — C (`tp_version`), C++, JavaScript,
  Python, Go, Rust, Swift, Java and Kotlin all report the same metadata: name,
  which implementation answered, the library version and its parts, the `.trp`
  format version they write, and the alphabet ceiling. A polyglot system can
  ask each one what it is and compare.
- **`scripts/sync_changelog.sh`** — `docs/releases.md` was a hand-written
  second copy of the changelog, saying the same things in different words.
  It is now generated from `CHANGELOG.md`, with `--check` in CI.
- **`scripts/test-jvm.sh`** — builds and tests the Java and Kotlin bindings
  with `javac`/`kotlinc` and the JUnit console launcher, fetching a JDK, JUnit
  and the Kotlin compiler into a gitignored cache rather than installing
  anything. The local release gate now covers all nine targets on a machine
  with no JVM toolchain.
- **`scripts/make-release.sh`** — builds and tests every target, and only if
  all of it is green drives the release: PR if the version bump has not landed
  yet, wait for CI, squash-merge, tag. It *reads* the version and never sets
  it; passing `--version` is an error that says so. A version bump is an
  ordinary reviewed change, which keeps the shipped version the one that was
  reviewed and makes the script safe to run as a check. `--check` runs the gate
  alone; `--dry-run` prints the git and gh commands. See `RELEASE.md`.
- **npm publishing.** The `triepack` package is built from
  `bindings/javascript`, ships bundled TypeScript declarations
  (`src/index.d.ts`), and publishes from `publish.yml` only after the whole
  cross-language test matrix passes. The job requests `id-token: write` and
  publishes with `--provenance`, so switching to OIDC trusted publishing is a
  registry-side setting; it is idempotent if the version already exists.
- Complete package metadata for npm and PyPI: repository, homepage, author,
  keywords, classifiers, project URLs, bundled licences, and an `files` /
  `MANIFEST.in` pair so neither package ships tests or fixtures that cannot
  run outside a checkout.
- GoatCounter analytics on the documentation site.
- Project boilerplate: `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1),
  `SECURITY.md` with a disclosure process and a scope aimed at the decoder,
  issue and pull-request templates, and an `.editorconfig` matching
  `.clang-format`. `CONTRIBUTING.md` was boilerplate from another project --
  it welcomed you to "TXZ" and told you to clone `txz` -- and has been
  rewritten around this repository's actual build, test and conformance
  workflow.
- **A cross-language conformance corpus** (`tests/conformance/`): one case
  list that the C library and all eight bindings run, checking that each
  decodes the C-generated fixtures to the same values and re-encodes them byte
  for byte. 50 cases covering trie shapes, `bits_per_symbol` boundaries,
  Unicode keys, the full numeric range, blobs and scale, plus 11 malformed
  buffers every reader must reject. See `tests/conformance/README.md`.
- `TP_ERR_ALPHABET`, `TP_ERR_UNSUPPORTED` and `TP_MAX_ALPHABET_SIZE` in the
  public C API; `MAX_ALPHABET_SIZE` exported by each binding
- `try_encode` in the Rust binding, for callers who would rather handle the
  alphabet limit than have `encode` panic
- Regression tests for issue #1 in all seven native bindings, plus a
  deterministic sweep over 2,000 generated key sets in JavaScript and Python
- `tests/test_core_limits.c` and `tests/test_conformance.c`
- docs/triepack-technical-doc.md section 5.4 "Subtree Extent", describing how
  a reader determines where a subtree ends

## v1.1.0 — 2026-03-04

### Added
- **Go binding** -- native Go implementation with ~38 tests (roundtrip + fixture)
- **Rust binding** -- native Rust implementation with 75 tests (44 unit + 31 integration)
- **Swift binding** -- native Swift implementation with 27 tests via SPM
- **Kotlin binding** -- native Kotlin/JVM implementation with ~41 tests via Gradle
- **Java binding** -- native Java implementation with ~42 tests via Gradle
- All five bindings validate against 7 C-generated fixture files for byte-level
  binary compatibility (both decode and byte-identical encode)
- Usage examples for all five new languages in docs/guide/examples.md

### Changed
- README: all 8 bindings marked "Implemented", project status updated, v1.1
  roadmap items checked off
- bindings/README.md: updated status and test counts for all bindings
- docs/guide/testing.md: added Go, Rust, Swift, Kotlin, Java test organization
  sections; grand total now ~749 tests across 51 test files
- docs/guide/examples.md: added contents table entries and usage examples for
  all five new languages

## v1.0.7 — 2026-03-04

### Added
- 100% line coverage across all three languages (C/C++, Python, JavaScript)
- C/C++: 2,395 lines covered across 16 source files, 27 test programs
- Python: 590 statements covered, 97 tests across 5 test files
- JavaScript: 99 tests across 6 test suites, all files at 100%
- New C test: keys-only dictionary lookup returns null value
- New JS tests: values encode/decode, bitstream edge cases, crafted trie
  error paths, trie prefix/branch coverage
- New Python tests: encoder edge cases, decoder error paths, bitstream
  bounds checking, varint overflow

### Fixed
- Encoder: removed 4 unreachable code paths (start>=end guards and
  single-child branches proven impossible by common-prefix loop invariant)
- Python encoder: removed unreachable dedup loop (dict keys are unique)
  and single-child branch (same invariant as C)
- JavaScript encoder: removed unreachable dedup loop and single-child branch

### Changed
- Added LCOV_EXCL markers to allocation failure paths across 14 C source
  files (malloc/realloc NULL returns require custom allocator injection to
  test, excluded from coverage measurement)
- Updated testing documentation with JavaScript test inventory
- Updated README project status and test counts

## v1.0.6 — 2026-03-02

### Fixed
- Encoder: deep-copy string/blob value data to prevent use-after-return when
  JSON encoder passes stack-allocated buffers (root cause of CI flaky test)
- JSON decoder: fix heap-buffer-overflow in segment dedup that used entry index
  as byte offset into key string
- Bitstream: fix undefined behavior in zigzag encode (left-shift of negative
  value); cast to uint64_t before shifting

### Added
- README: CI build, coverage, and BSD-2-Clause license badges
- README: roadmap section with planned milestones for language bindings,
  format enhancements, and tooling
- CLI tool: `trp encode`, `trp decode`, `trp validate` commands with stdin
  support and `-o`/`--pretty` flags

### Changed
- README: updated language bindings table (Python, JavaScript now implemented)
- README: updated project status to v1.0.5 test counts
- CLI tool: removed `trp json` command (replaced by `trp decode`)

## v1.0.5 — 2026-03-02

### Added
- Native Python binding: pure-Python `.trp` encoder/decoder with byte-for-byte compatibility
- Python test suite: 70 tests across 5 files (crc32, bitstream, varint, roundtrip, fixtures)
- Complex JSON example (`json_complex.c`): nested objects, arrays, DOM lookups, pretty-print
- Test data files: `common_words_10k.txt` (10K words) and `benchmark_100k.json` (202 KB)
- Generator script: `tools/generate_benchmark_json.py`
- 3 new C test files: `test_json_decode.c`, `test_core_internal.c`, `test_bitstream_errors.c`
- Expanded existing test files with error-path and edge-case coverage

### Changed
- C test suite: 16 test programs -> 20 test programs, ~330 individual tests
- Total tests across all languages: ~400 (C/C++ + Python)
- Updated testing documentation (`docs/guide/testing.md`)
- Updated bindings README: Python and JavaScript marked as implemented

## v1.0.4 — 2026-03-02

### Fixed
- Bitstream guide: clarify signed bit-field extraction with worked example

## v1.0.3 — 2026-03-02

### Fixed
- CSS: move stylesheet to `assets/main.scss` so minima theme loads correctly
- Remove hardcoded top bar above nav, add whitespace around section dividers
- Adjust version label size and color for readability

## v1.0.2 — 2026-03-01

### Added
- Native JavaScript `.trp` implementation: pure-JS encoder/decoder
- Cross-language fixture files (7 `.trp` files) for interop testing
- Cross-language test (`test_cross_language.c`) validating fixture files

### Fixed
- 32-bit CI: enable Unity 64-bit type support, switch Pages to workflow build
- Site margins: use 75% viewport width, fix `!important` overrides
- clang-tidy: suppress `misc-no-recursion` false positive, fix dead store bug
- GitHub Pages: fix lcov report overwriting docs site, fix broken links

## v1.0.1 — 2026-03-01

### Fixed
- GitHub Pages deployment: docs site was showing lcov coverage report instead of Jekyll site
- Fix broken navigation links on docs site

## v1.0.0 — 2026-02-28

### Added

**Bitstream Library (`triepack_bitstream`)**
- Arbitrary-width bit field read/write (1-64 bits per field)
- MSB-first and LSB-first bit ordering
- Fixed-width byte reads: u8, u16, u32, u64 (big-endian)
- VarInt encoding: unsigned (LEB128) and signed (zigzag + LEB128)
- Fixed-width symbol read/write (configurable bits-per-symbol)
- UTF-8 codepoint read/write with full validation
- Stateless ROM functions (`_at` variants) for zero-allocation reads
- Growable writer with configurable buffer growth policy
- Zero-copy reader over `const` buffers (ROM-safe)
- Direct pointer access for zero-copy string/blob reads
- Bulk copy (reader-to-writer)
- 64-bit cursor addressing

**Core Trie Codec (`triepack_core`)**
- Encoder: batch insert key-value pairs, build compressed trie
- Automatic symbol analysis (optimal bits-per-symbol selection)
- Two-pass trie encoding with skip pointers for O(key-length) lookups
- Full typed value system: null, bool, int, uint, float32, float64, string, blob
- Value construction helpers (`tp_value_int()`, `tp_value_string()`, etc.)
- Dictionary reader with CRC-32 integrity verification
- Unchecked open for trusted/ROM data
- Key lookup, existence check, key count, dictionary metadata
- 32-byte binary header with magic bytes (`TRP\0`), version, flags, offsets
- CRC-32 footer (reflected polynomial, matches zlib)

**JSON Library (`triepack_json`)**
- One-shot JSON encode: parse JSON string, produce `.trp` blob
- One-shot JSON decode: reconstruct JSON from `.trp` blob
- Pretty-printed decode with configurable indentation
- DOM-style access: open, path lookup, root type, count
- Flattened dot-path key scheme (`a.b.c`, `items[0]`)
- Supports objects, arrays, strings, numbers, booleans, null
- Unicode escape handling (`\uXXXX`) with surrogate pair support

**C++ Wrappers (`triepack_wrapper`)**
- `triepack::Encoder` -- RAII encoder with move semantics
- `triepack::Dict` -- RAII dictionary reader with move semantics
- `triepack::Iterator` -- RAII iterator with move semantics
- `triepack::BitstreamReader` / `BitstreamWriter` -- RAII bitstream wrappers
- `triepack::Json` -- RAII JSON DOM wrapper

**Build System**
- CMake 3.16+ build system
- C99 (no extensions) + C++11 (no extensions) enforced
- Strict compiler warnings: -Wall -Wextra -Wpedantic -Werror
- Unity test framework (v2.6.0) via FetchContent
- Code coverage support (gcov/lcov)
- Doxygen documentation generation

**CI/CD**
- GitHub Actions: Ubuntu GCC, Ubuntu Clang, macOS Clang, 32-bit
- clang-tidy linting and clang-format style checking
- Coverage report generation and GitHub Pages deploy

**Tests**
- 16 test suites covering bitstream, core, JSON, and C++ wrappers
- 6 example programs registered as integration tests

**Examples**
- `basic_encode_decode` -- encode/decode with multiple value types
- `compaction_benchmark` -- 10k-word compression ratio measurement
- `rom_lookup` -- ROM-style zero-allocation dictionary access
- `prefix_search` -- membership checking with shared-prefix keys
- `json_roundtrip` -- JSON encode/decode/DOM round-trip
- `cpp_usage` -- C++ RAII wrapper demonstration

**Documentation**
- Getting started guide, build instructions, API reference
- Binary format specification with worked examples
- Bitstream specification with cross-byte field diagrams
- Technical deep dive document
- Jekyll-based GitHub Pages site

## v0.1.0 — 2026-02-27

### Added
- Initial project scaffolding
- Directory structure, CMake build system, public API headers
- Stub implementations for all libraries
- CI/CD pipeline configuration
- Language binding scaffolding (Python, TypeScript, JavaScript, Go, Swift, Rust)
