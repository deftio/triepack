---
layout: default
title: v2 Implementation Plan
---

# TriePack v2 — Implementation Plan

<!-- Copyright (c) 2026 M. A. Chatterjee -->

How to get from [v1](format-spec.md) to
[v2](format-spec-v2.md) across ten implementations without losing the
byte-identity property along the way.

Read [the North Star](../triepack-northstar.md) first; this document assumes
its priority ordering.

## 1. The baseline we have to beat

Measured on this machine with the v1 encoder, `tools/run_benchmarks`:

### `common_words_10k.txt` — 10,000 words, 71,824 key bytes

| Metric | Keys only | Keys + int values |
|---|---|---|
| Encoded size | 45,054 B | 90,451 B |
| Compression vs input | 1.59× | **0.79× (larger than input)** |
| Bits per key | 36.0 | 72.4 |
| Encode time | 5.0 ms | 7.1 ms |
| Lookup | 2.73 µs/key | **328.06 µs/key** |

### `benchmark_100k.json` — 202,408 B JSON, 5,044 flattened keys

| Metric | Value |
|---|---|
| Encoded `.trp` | 134,287 B (66.3% of JSON) |
| Encode time | 9.4 ms |
| **Decode time** | **1,314 ms** |
| Single lookup | 0.66 µs |

Two numbers should be alarming, and they are the same bug wearing different
hats:

**328 µs/key with values, against 2.73 µs/key without** — a 120× cliff.
Resolving `END_VAL`'s varint index means seeking to the value store and
decoding every preceding value, so lookup is O(n) and a full pass is O(n²).
Decoding 5,044 keys taking 1.3 seconds is the same defect.

**0.79× compression with values** — the file is *larger than its input*,
because every key pays a varint index on top of its value.

v2 removes both structurally: the value ordinal is derived by rank rather
than stored (§9.1), and location is O(1) or bounded at 32 skips (§9.2).

### The O(n) value scan, measured

`tests/test_scale.c`, uniform keys with integer values:

| Keys | Lookup | vs 10k keys |
|---|---|---|
| 10,000 | 328 µs | 1.0× |
| 174,762 | 7,524 µs | 22.9× (keys 17.5×) |
| 699,050 | 30,500 µs | 93.0× (keys 69.9×) |

Latency tracks key count almost exactly: the scan is O(n) per lookup and a
full pass is O(n²). At 699k keys **a single lookup takes 30 milliseconds**.
Extrapolated to the 1 GB inputs this format is meant to serve — on the order
of 45M keys — one lookup would take roughly two seconds.

This is not a tuning problem. No amount of optimisation rescues a linear scan
per lookup; the index has to go, which is what §9.1 does.

### Targets, and what the prototype actually did

`tools/v2_prototype.c` implements the v2 core — radix trie, LOUDS with
rank/select, tails with suffix merging, terminal bitmap — serialises it, and
performs every lookup *through the serialised bytes*. It is registered as a
test (`v2_prototype_roundtrip`) and exits non-zero on any key mismatch.

| Metric | v1 | Target | **Measured** |
|---|---|---|---|
| Keys-only size, `common_words_10k` | 45,054 B | ≤ 31,500 B | **27,610 B (−39%)** |
| …with optional Huffman | — | — | **25,518 B (−43%)** |
| Keys verified through serialised bytes | — | all | **10,000 / 10,000** |
| Distinct key bytes | 249 | 256 | **253 corpus encodes; v1 refuses it** |

Across corpora, as a fraction of raw key bytes:

| Corpus | Raw keys | v2 | +Huffman |
|---|---:|---:|---:|
| `common_words_10k.txt` | 71,824 | 27,610 (38%) | 25,518 (36%) |
| 20k path-like keys | 682,642 | 361,567 (53%) | 256,579 (38%) |
| 20k random binary keys | 160,523 | 237,923 (**148%**) | 237,697 (148%) |

The last row is not a defect to fix. Random binary keys share no structure, so
a trie stores per-key overhead on incompressible data and the result exceeds
its input. Worth stating plainly in the docs: TriePack is for keys with shared
structure. v1 could not represent that corpus at all.

### Values: the O(n) scan, closed

The prototype implements [spec §9](format-spec-v2.md) — value ordinal derived
by `rank1` over the terminal bitmap, located through a sampled offset index —
and fetches every key's value the way a reader would.

Directly against v1 on `common_words_10k.txt`:

| | v1 | v2 | Change |
|---|---:|---:|---|
| Keys only | 45,054 B | 27,610 B | **−39%** |
| Keys + integer values | 90,451 B | 48,734 B | **−46%** |
| Lookup, keys only | 2.73 µs | 2.02 µs | −26% |
| Lookup + value | 328 µs | **2.06 µs** | **159× faster** |

The value fetch costs 0.04 µs — 2.06 against 2.02 keys-only. In v1 it cost
325 µs. That is the whole difference between a stored index that must be
scanned to and an ordinal derived by rank.

It holds at scale, which is the part that matters:

| Keys | v1 lookup+value | v2 lookup+value | Speedup |
|---|---:|---:|---:|
| 10,000 | 328 µs | 2.06 µs | 159× |
| 174,762 | 7,524 µs | 2.59 µs | 2,905× |
| 699,050 | 30,500 µs | **3.17 µs** | **9,621×** |

v1 grows 93× across that range; v2 grows 1.5×. The residual is the binary
search in `select0`, which is O(log n) exactly as [spec §5.3](format-spec-v2.md)
says it should be.

### Full baseline

All keys and values verified through the serialised bytes in every row:

| Corpus | Keys | Raw keys | Keys only | +values | lookup+value | Distinct tails |
|---|---:|---:|---:|---:|---:|---|
| `common_words_10k.txt` | 10,000 | 71,824 | 27,610 (38%) | 48,734 | 2.06 µs | 696 of 7,056 |
| path-like keys | 20,000 | 682,642 | 361,567 (53%) | 407,555 | 2.84 µs | 16,640 of 26,589 |
| random binary keys | 20,000 | 160,523 | 237,923 (**148%**) | 283,911 | 1.05 µs | 19,568 of 20,010 |
| synthetic, 175k | 174,762 | 4,194,288 | 3,626,484 (86%) | 4,156,106 | 2.59 µs | 174,763 of 174,763 |
| synthetic, 699k | 699,050 | 16,777,200 | 14,854,981 (89%) | 17,023,003 | 3.17 µs | 699,051 of 699,051 |

**Compression tracks shared structure, and nothing else.** English words share
suffixes heavily (696 distinct tails behind 7,056 occurrences) and compress to
38%. The synthetic corpora have hash-derived middles, so *every* tail is
unique — no sharing is available and the result is 86-89%. Random binary keys
exceed their input. None of these is a defect; they are what a trie does, and
the docs should say so rather than quote the 38% alone.

### One gigabyte, end to end

`data/enwik9` — 1,000,000,000 bytes of Wikipedia XML from the Large Text
Compression Benchmark, treated as a newline-separated key list:

| | |
|---|---|
| Distinct keys | 10,920,486 |
| Trie nodes after radix compression | 10,352,071 |
| Distinct tails | 5,166,976 of 8,480,394 occurrences |
| Keys only | 797,098,514 B (1.25× the input) |
| With values | 824,904,195 B |
| Lookup + value | **10.02 µs/key** |
| Keys and values verified | **10,920,486 / 10,920,486** |
| Wall clock | 315 s |
| **Encoder peak RSS** | **60.2 GB** |

Three things to take from this.

**It works.** A gigabyte encodes, serialises and verifies every key and every
value through its own bytes. v1 cannot represent this input at all — the data
stream exceeds its 512 MB ceiling — so this is the first evidence the 1 GB
requirement is actually met.

**Lookup still barely moves.** 2.06 µs at 10k keys, 10.02 µs at 10.9M: a
1,000× increase in dictionary size costs 5× in latency, which is the
`log n` of `select0` and nothing else.

**The encoder blew its memory budget by 7.5×.** [Spec §12.2](format-spec-v2.md)
requires building a 1 GB dictionary in ≤ 8× input; this took **60×**. The
cause is the prototype's build strategy, not the format: it inserts one node
per input *byte* and compresses afterwards, so a 987 MB key corpus
materialises ~987M nodes before collapsing to 10M. A conforming encoder must
build the radix trie directly, or build it externally in passes.

This is a Phase 3 blocker and belongs there rather than in the format: the
serialised output is unaffected. **It is also exactly what Phase 3 is for** —
a memory strategy that fails at a gigabyte is much cheaper to find in one
implementation than in ten.

### XML is a bad case, and that is informative

1.25× on enwik9, against 2.96× on an English word list. Wikipedia markup is
mostly prose lines that share very little: 5.2M distinct tails behind 8.5M
occurrences, so suffix merging has almost nothing to work with. The same
story as the synthetic corpora — **compression tracks shared structure and
nothing else** — and a reason to quote a range rather than a headline number.

### The prototype earned its keep before the first port

The first run came out at **57,507 bytes — 28% worse than v1** — and showed
why in one line: tail references were 61.3% of the file. The spec stored a
`(pool offset, length)` pair per tail *occurrence*, so 7,056 occurrences of
696 distinct tails spent **35,280 bytes pointing into a 2,064-byte pool**.
The deduplication was working perfectly; the references to it outweighed the
data seventeen to one.

Two corrections, both now in the spec:

- reference the distinct tail by index (10 bits) with a small dictionary,
  rather than repeating the pair per occurrence — 35,280 B → 10,212 B
- code labels at `⌈log2(alphabet)⌉` rather than a byte each — 12,949 B →
  8,120 B

That is the difference between the format losing to v1 by 28% and beating it
by 39%, and it was found for the cost of one prototype instead of ten ports.
**Phase 2 stays a hard gate for exactly this reason.**

## 2. Sequencing principle

The v1 lesson is that the conformance corpus found five of six real bugs — so
in v2 it comes **first**, before any implementation. Ten implementations
built against fixtures that already exist cannot drift; ten implementations
that agree a corpus afterwards will have already drifted.

The rank primitive gets vectors of its own, independent of any dictionary,
because LOUDS, tails and terminals all stand on it. A rank bug is ten
mysterious failures at once.

## 3. Phases

### Phase 0 — Specification freeze

Resolve the open questions in [spec §14](format-spec-v2.md):

- Sample interval (32) and superblock/block sizes (2048/256): measure in
  Phase 2, freeze before Phase 3.
- DAG merging: confirm deferred to v3.

None of these change the addressable size of a dictionary, so none blocks
Phase 1.

*Exit:* spec has no open questions that change the wire format.

### Phase 1 — Conformance corpus

Before any encoder exists.

1. **Rank/select vectors** — bit vectors up to 2³²+1 bits (the boundary that
   catches u32 truncation), with expected `rank1`/`select0`/`select1` at
   chosen positions. Generated by an independent reference in Python, checked
   against a brute-force implementation.
2. **LOUDS vectors** — hand-computed trees including spec §6.3.
3. **Dictionary corpus** — v1's 50 cases, re-expressed for v2, plus new ones
   that v1 could not represent: all 256 byte values, keys with embedded NULs,
   tails over 255 bytes, keys sharing 10 KB prefixes.
4. **Malformed corpus** — v1's 11 plus v2-specific: rank index inconsistent
   with its bit vector, `popcount(terminals) ≠ num_keys`, tail reference past
   the pool, non-zero reserved flag bits, section offsets overlapping or
   unaligned.

*Exit:* corpus generates and self-checks; no implementation exists yet.

### Phase 2 — C reference implementation

The C core is the reference. Build in dependency order, each stage green
against its Phase 1 vectors before the next starts:

1. Ranked bit vector (build + `rank1` + `select0`/`select1`)
2. LOUDS construction and navigation
3. Labels and binary-search descent
4. Tail extraction and the §8.1 dedup algorithm
5. Terminal bitmap and rank-derived value ordinals
6. Value store with both indexing modes
7. Header, section layout, alignment, CRC

Then measure against §1 targets. **This is the falsification gate.** If tails
plus LOUDS do not reach ≤ 31,500 B on `common_words_10k.txt`, stop and revise
the format before porting anything.

*Exit:* C passes the full corpus; benchmarks meet or beat §1 targets, or the
spec is amended and Phase 1 regenerated.

### Phase 3 — Large-input hardening

Before porting — a 512 MB ceiling found in nine other languages is nine times
the work.

Implement the [spec §12.2](format-spec-v2.md) fixtures as generators, run the
C implementation against all five up to 1 GB, and assert:

- offsets past 2³² address correctly
- rank/select correct past 2³² bits
- decoder peak RSS within a small constant of the mapped file
- lookup latency flat as dictionary size grows
- encoder peak RSS ≤ 8× input

*Exit:* 1 GB inputs encode, decode and look up correctly in C.

### Phase 4 — Port to nine implementations

Only now. Each port is: rank primitive → LOUDS → labels → tails → terminals →
values, green against the shared corpus at each step.

Suggested order — Rust and Go first (strong tooling, catch spec ambiguity
early), then Python and JavaScript (reference-readable, likely where
ambiguities surface as bugs), then TypeScript, Java, Kotlin, Swift, and the
C++ wrapper last since it sits on the C core.

Each port must run the large-input fixtures at a reduced size (~64 MB) in CI,
with the full 1 GB set nightly or on demand.

*Exit:* all ten byte-identical on the full corpus.

### Phase 5 — Cut over

- Delete the v1 encoder and decoder. No converter, no compatibility mode:
  nothing is deployed, and carrying v1 forward doubles the parity tax
  permanently.
- **Delete the API surface that names things that do not exist.** 1.3.2 made
  these refuse rather than silently ignore. 2.0.0 did not remove them — it was
  a version bump over this groundwork, not a break. Removal is an API
  break and therefore belongs here:
  - `tp_addr_mode` — collapse to whatever v2 actually uses
  - `tp_checksum_type` — `SHA256` and `XXHASH64` have no implementation
  - `tp_encoder_options.enable_suffix` — v2 always shares tails; there is
    nothing to switch
  - `tp_encoder_options.compact_mode`, `trie_mode`, `value_mode`
  - `TP_ARRAY` / `TP_DICT` value tags
  - `has_nested_dicts` / `compact_mode` header flags
  See [Status §2](../status.md) for the full inventory.
- Update `format-spec.md` to point at v2 and keep v1 as a historical note.
- Publish 2.0.0 across npm and PyPI.

## 4. Benchmarks

`tools/run_benchmarks` currently reports size and time for two corpora. v2
needs it to answer harder questions, and to run in CI so regressions surface
as diffs rather than surprises.

**Corpora to add:**

| Corpus | Why |
|---|---|
| `common_words_10k.txt` | existing, keeps the v1 comparison honest |
| `benchmark_100k.json` | existing, exercises the JSON layer |
| Full English wordlist (~370k) | where suffix sharing should pay most |
| URL/path list | long shared prefixes, deep tails |
| Random binary keys | the case v1 could not represent at all |
| Synthetic 1 GB (generated) | scale, not committed to git |

**Metrics to add:** decode throughput (MB/s), peak encoder and decoder RSS,
bytes per key split into structure / labels / tails / values, and lookup
latency at p50 and p99 rather than a mean — the v1 mean hid the O(n) value
scan behind an average.

**CI:** the small corpora run per-PR with results in the job summary and a
hard failure if size regresses more than 2% or lookup more than 10%. The 1 GB
set runs nightly.

## 5. Risks

| Risk | Mitigation |
|---|---|
| Size targets missed | Phase 2 is an explicit falsification gate before porting |
| `select` binary search too slow | measure in Phase 2; a sampled select index is additive and does not change the wire format |
| Tail dedup too slow or too hungry at 1 GB | §8.1 sorts distinct tails, not occurrences; Phase 3 verifies the 8× budget |
| Spec ambiguity found during porting | fix spec, regenerate corpus, re-verify C — never patch one port to match another |
| Ten-way port stalls | Phases 0-3 leave a shippable C implementation; v1 keeps working meanwhile |
| Declared widths mis-selected by an encoder | widths are a pure function of pool size and longest tail; the conformance corpus includes a fixture at each width boundary |

## 6. What this does not change

The JSON layer, the public API shape (`encode`/`decode`, `Dict`/`Encoder`/
`Iterator`), the CRC-32 footer, the value type tags, and the ten-implementation
parity discipline all carry forward. v2 is a change to how bytes are laid out,
not to what TriePack is.
