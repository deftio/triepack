# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-09-16

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
  it; `scripts/sync_version.sh` propagates it to every package manifest, the
  Kotlin `VERSION` constant, the Python `__version__`, the docs site header and
  the README, with `--check` failing on drift. CI runs `--check` on every push.
  All bindings move from 0.1.0 to 1.1.0 to match the C library. The on-disk
  *format* version is deliberately not synced.
- **`scripts/make-release.sh`** — builds and tests every target, and only if
  all of it is green drives the release: version bump on a release branch, PR,
  wait for CI, squash-merge, tag. `--check` runs the gate alone; `--dry-run`
  prints the git and gh commands. A missing toolchain fails rather than
  silently skipping. See `RELEASE.md`.
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

## [1.1.0] - 2026-03-04

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

## [1.0.7] - 2026-03-04

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

## [1.0.6] - 2026-03-02

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

## [1.0.5] - 2026-03-02

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

## [1.0.4] - 2026-03-02

### Fixed
- Bitstream guide: clarify signed bit-field extraction with worked example

## [1.0.3] - 2026-03-02

### Fixed
- CSS: move stylesheet to `assets/main.scss` so minima theme loads correctly
- Remove hardcoded top bar above nav, add whitespace around section dividers
- Adjust version label size and color for readability

## [1.0.2] - 2026-03-01

### Added
- Native JavaScript `.trp` implementation: pure-JS encoder/decoder
- Cross-language fixture files (7 `.trp` files) for interop testing
- Cross-language test (`test_cross_language.c`) validating fixture files

### Fixed
- 32-bit CI: enable Unity 64-bit type support, switch Pages to workflow build
- Site margins: use 75% viewport width, fix `!important` overrides
- clang-tidy: suppress `misc-no-recursion` false positive, fix dead store bug
- GitHub Pages: fix lcov report overwriting docs site, fix broken links

## [1.0.1] - 2026-03-01

### Fixed
- GitHub Pages deployment: docs site was showing lcov coverage report instead of Jekyll site
- Fix broken navigation links on docs site

## [1.0.0] - 2026-02-28

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

## [0.1.0] - 2026-02-27

### Added
- Initial project scaffolding
- Directory structure, CMake build system, public API headers
- Stub implementations for all libraries
- CI/CD pipeline configuration
- Language binding scaffolding (Python, TypeScript, JavaScript, Go, Swift, Rust)
