---
layout: default
title: North Star
---

# TriePack North Star

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This document states what TriePack is for, what it optimises, what it
deliberately does not do, and the rules any format change has to obey. It is
the document to argue with before arguing about a format.

It exists because v1 drifted from its own plan. Two of six control codes
(`SUFFIX`, `ESCAPE`) and a header field (`suffix_table_offset`) were reserved
for features that were never built, and `src/core/suffix.c` is fourteen lines
of which the implementation is `/* TODO */`. Those unbuilt reservations are
what capped the alphabet at 249 byte values — the format paid rent on vapour,
and binary keys were the tenant evicted. Writing the goals down first is how
that stops happening again.

## 1. What TriePack is

**A static, immutable, byte-identical file format for string-keyed
dictionaries with typed values, readable directly from ROM by ten independent
implementations.**

Each clause is load-bearing:

- **Static / immutable** — a `.trp` is built once and never modified. No
  insertion, no deletion, no rebalancing. This is not a limitation we
  tolerate; it is the licence that makes everything else possible.
- **Byte-identical** — every implementation, given the same input, emits the
  same bytes. This is the crown jewel and the hardest constraint.
- **Readable directly from ROM** — the file is the data structure. No parse
  step, no load-time fixups, no relocation, no allocation required to read.
- **Ten implementations** — C, C++, Python, JavaScript, TypeScript, Go, Rust,
  Swift, Kotlin, Java. All native. None is a binding over another.

## 2. What we optimise, in order

When two goals conflict, the earlier one wins. This ordering is the whole
point of the document.

1. **Size on disk / in flash.** The primary metric. A dictionary that does not
   fit is not slow, it is absent.
2. **ROM-ability.** Readable in place from flash with no RAM copy, no
   allocation, no load-time work, and no unaligned access on parts that
   cannot do it.
3. **Cross-implementation byte-identity.** Non-negotiable, but listed third
   because it constrains *how* we achieve 1 and 2 rather than competing
   with them.
4. **Lookup speed.** Fast enough that size is the thing you notice. We will
   trade constant factors for bytes; we will not trade asymptotics.
5. **Implementation simplicity.** Every feature costs ten implementations.
   A feature that cannot be specified precisely enough to implement ten times
   is not a feature, it is a wish.

## 3. What we explicitly do not do

Naming these is as important as naming the goals. Each is a real capability
that a reasonable person might want and that we are choosing to decline.

- **Mutation.** No insert, update or delete on a built dictionary. Rebuild.
- **General-purpose serialisation.** If you want to store arbitrary object
  graphs, use CBOR, FlatBuffers or Cap'n Proto. TriePack stores a *map from
  byte strings to values*, and the JSON layer is a flattening convention on
  top of that, not a document database.
- **Beating specialist structures at their specialty.** `marisa-trie` and
  `xcdat` will win on compression of very large dictionaries. `darts-clone`
  will win on raw lookup throughput. `fst` will win on fuzzy search. We are
  not trying to take those crowns, and a proposal that costs portability to
  chase one should be rejected.
- **Approximate or fuzzy matching, for now.** `tp_dict_find_fuzzy` returns
  `TP_ERR_UNSUPPORTED` and will keep doing so until someone is prepared to
  implement a Levenshtein automaton ten times, identically. Saying so is
  better than shipping something that looks like fuzzy search and is not.

## 4. Design rules

These are the rules a format change has to survive.

### 4.1 Global optimisation is legal

The encoder may examine the entire input before emitting a byte. Whole-corpus
analysis, multi-pass layout, suffix deduplication, optimal-ish packing — all
permitted, all encouraged. The encoder may be slow and may allocate freely.

The **decoder** may not. That asymmetry is the deal: we push every cost we
can to build time, because build happens once on a workstation and read
happens forever on something small.

### 4.2 Every optimisation must be an algorithm, not a goal

This is the rule that v1 lacked and the one that byte-identity depends on.

"The encoder deduplicates common suffixes" is a *goal*, and ten
implementations pursuing a goal will produce ten different files. "The
encoder deduplicates suffixes by the exact procedure in §N, processing tails
in the specified order, with ties broken as specified" is an *algorithm*, and
ten implementations of an algorithm produce one file.

Any optimisation admitted to the format must be specified to the point where
two competent implementers cannot disagree about the output. If it cannot be
pinned down that tightly, it does not go in the format — it goes in a
build-time tool that emits a conforming file.

### 4.3 The file is the data structure

No load-time fixups. No pointer swizzling. No "parse into a runtime
representation". Offsets are file-relative so the blob is position
independent; a reader adds its base pointer and walks. Acceleration
structures that a runtime would normally build — rank indices, in particular
— are **precomputed by the encoder and stored in the file**, because build
time is free and load time is not.

### 4.4 Sections are aligned; bit-packing is local

Sections begin on 4-byte boundaries. Within a section, bit-level packing is
fine. The reason is ROM: a Cortex-M0 cannot do unaligned word access, and the
rank tables are exactly the thing you want to read a word at a time.

### 4.5 Structure and labels are separate

v1 put control codes and alphabet symbols in one code space, sized by an
8-bit field. That coupling produced the 249-byte-value ceiling: structural
vocabulary stole from data vocabulary.

In v2 they never touch. Structure lives in a bit vector; labels are raw
bytes. There is no shared code space to run out of, and therefore no
alphabet limit, no symbol table, no escape mechanism, and no class of bug
where a wide alphabet silently corrupts a file. **All 256 byte values are
legal in keys.**

### 4.6 One format, not a family of profiles

It is tempting to define profiles so that a small reader can implement a
subset. We decline. A `.trp` that only some conforming readers can open
destroys the property the whole project exists for. Features may be *unused*
in a given file — flagged absent in the header — but every conforming reader
must handle every feature.

The way we keep embedded readers small is by keeping the *format* small, not
by fragmenting it.

### 4.7 Reserve nothing you are not building

v1's reserved control codes are the reason this document exists. If a future
feature needs format space, it takes a minor version and claims the space
then. Reserved fields must be `0` and readers must reject non-zero, so the
space is genuinely recoverable later rather than accidentally load-bearing.

## 5. The constraint behind every decision

**Ten implementations.** Every feature is written ten times, tested ten times,
and kept byte-identical forever. This is why:

- the format prefers one mechanism used three times over three mechanisms
  (v2 uses a single rank primitive for the tree, the terminal map and the
  tail map);
- rank/select uses a simple blocked-popcount scheme with binary search rather
  than a broadword select structure — the latter is faster and nobody wants
  to debug ten subtly different versions of it;
- the conformance corpus is written *before* the implementations, not after.

A change that is elegant in one language and miserable in the other nine is a
bad change, however good the benchmark looks.

## 6. How we will know v2 is better

Success criteria, to be measured against v1 on the existing corpora
(`tests/data/common_words_10k.txt`, `tests/data/benchmark_100k.json`):

| Property | v1 | v2 target |
|---|---|---|
| Distinct byte values in keys | ≤ 249 | **256** (no limit) |
| Structural overhead per node | `bps` bits + branch varints | ~2 bits + index |
| Shared suffix storage | not implemented | required |
| Value index per key | explicit varint | derived by rank, **not stored** |
| Load-time work | none | none |
| Allocation to open | one struct | **zero** (caller-provided storage) |
| Subtree extents | explicit `SKIP` distances | implicit in structure |

The last row is a correctness goal as much as a size goal. Issue #1 was the
decoder mis-deriving subtree extents from `SKIP` distances; a structure where
extents cannot be mis-derived retires that entire bug class.

## 7. Related documents

- [Format Specification v2](internals/format-spec-v2.md) — the proposed
  binary format
- [v2 Implementation Plan](internals/v2-implementation-plan.md) — sequencing
  across ten implementations
- [Format Specification v1](internals/format-spec.md) — what ships today
