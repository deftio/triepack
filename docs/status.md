---
layout: default
title: Status
---

# What TriePack Actually Does

<!-- Copyright (c) 2026 M. A. Chatterjee -->

An audit of the gap between what TriePack documents and what it implements,
as of 2.0.0.

This document exists because that gap was large and undocumented. The
architecture page describes a two-trie design, three addressing modes and
Huffman symbol selection; none of those are implemented. The public encoder
options struct has six fields, of which the encoder read one and silently
ignored five. Two of three checksum algorithms named in a public enum do not
exist. Someone reading the docs would reasonably conclude they were getting
a system that was never built.

Nothing here is a plan. [The North Star](triepack-northstar.md) says where
this is going and [the v2 spec](internals/format-spec-v2.md) says how. This
page is only concerned with what is true today.

It covers the TriePack library only. [`terseml/`](pages/terseml.md) is a
separate subproject that happens to live in the same repository; it links
nothing from TriePack, is not published anywhere, and is not audited here.

## 1. Core: implemented and tested

| Capability | State |
|---|---|
| Bit-packed prefix trie, fixed-width symbols | works |
| Typed values: null, bool, int, uint, float32/64, string, blob | works |
| Lookup, `contains`, `get_info` | works |
| Full iteration in lexicographic order | works |
| Prefix iteration (`tp_dict_find_prefix`) | works |
| CRC-32 integrity, malformed-input rejection | works |
| Zero-copy strings and blobs | works |
| Ten implementations, byte-identical, conformance-enforced | works |
| JSON flattening layer | works |

This is a real capability set and the cross-implementation parity is real.
The rest of this page is what surrounds it.

## 2. Named but not implemented

Each of these is reachable from a public header or a published document.

| Thing | Where it appears | Reality |
|---|---|---|
| Suffix table / two-trie design | `architecture.md`, `TP_CTRL_SUFFIX`, `suffix_table_offset`, `has_suffix_table` | `src/core/suffix.c` is fourteen lines ending in `/* TODO */`. No suffix sharing happens. |
| `ESCAPE` control code | `core_internal.h`, format spec | Reserved. Never emitted, never read. Consumes alphabet space. |
| Addressing modes (byte, symbol-fixed, symbol-UTF8) | `tp_addr_mode`, technical doc §"three addressing modes" | Only `TP_ADDR_BIT`/`TP_ADDR_BYTE` occur, both hardcoded. The technical doc's claim that they are "stored in header flags" is false. |
| SHA-256, xxHash64 checksums | `tp_checksum_type` | Only CRC-32 exists. |
| Huffman / multi-width symbol encoding | `architecture.md` pipeline diagram | Fixed-width only. |
| `array`, `dict` value types | `tp_value_type`, format spec §5.1 | Encoding returns `TP_ERR_INVALID_PARAM`. |
| `has_nested_dicts`, `compact_mode` header flags | format spec §2.3 | Never set, never read. |
| Fuzzy search | `tp_dict_find_fuzzy` | Returns `TP_ERR_UNSUPPORTED`. Honest, at least. |

As of 1.3.2 the encoder **rejects** options naming unimplemented features
(`TP_ERR_UNSUPPORTED`) instead of accepting and ignoring them. The enum
values and struct fields are still there. 2.0.0 did **not** remove them —
that release was a version bump over the v2 groundwork, not an API break, and
57 checked-in fixtures are byte-identical to 1.3.2's. Deleting them is still
the right end state and still a break, so it waits for a major that actually
means to be one.

## 3. Known defects

### Value lookup is O(n)

`END_VAL` stores a varint index into the value store, and resolving it means
decoding every preceding value. Measured, uniform integer values:

| Keys | Lookup |
|---|---|
| 10,000 | 328 µs |
| 174,762 | 7,524 µs |
| 699,050 | **30,500 µs** |

Latency tracks key count almost exactly. A full decode is O(n²) — decoding
the 5,044-key JSON benchmark takes 1.3 seconds. **This is the most serious
defect in the library.** It cannot be tuned away; the stored index has to go.

The v2 prototype does exactly that — ordinal by `rank1` over the terminal
bitmap instead of a stored index — and measures **2.06 µs, 2.59 µs and
3.17 µs** at those same three sizes: 159×, 2,905× and 9,621× faster, growing
logarithmically where v1 grows linearly. See
[the v2 plan](internals/v2-implementation-plan.md).

Keys-only dictionaries are unaffected (2.73 µs/key).

### Dictionaries with values can be larger than their input

`common_words_10k.txt` with integer values encodes to 90,451 bytes from
71,824 bytes of keys — 0.79×. Every key pays a varint index on top of its
value. Keys-only is 1.59×.

### Iteration depth is capped at 256

The walk keeps one frame per open branch. Deeper nesting stops with
`TP_ERR_OVERFLOW` after yielding what fits. Lookup is unaffected. Pathological
in practice, but real.

### The read path allocates

`architecture.md` claims no heap allocation in the read path. Actually:
`tp_dict_open` allocates one control structure, and `tp_dict_iterate`
allocates an iterator plus a 256-byte key buffer that grows. Lookup itself
allocates nothing, which is the claim that matters for ROM, but the blanket
statement is wrong.

## 4. Hard limits

| Limit | Value | Consequence |
|---|---|---|
| Distinct byte values in keys | 249 | Binary keys using more are refused (`TP_ERR_ALPHABET`) |
| Data stream size | 2³² bits ≈ 512 MB | Larger inputs are refused (`TP_ERR_OVERFLOW`) |
| Iteration branch depth | 256 | Deeper dictionaries iterate partially |
| JSON nesting depth | 32 | |
| Value store random access | O(n) | See §3 |

The first two were **silent corruption** until this release: exceeding either
produced a file with a valid CRC that decoded to garbage. Both now refuse.
Both are lifted by v2.

## 5. Why this happened, and what prevents a repeat

Three causes, all structural rather than careless:

**Reserving space for unbuilt features.** `SUFFIX` and `ESCAPE` took two of
six control codes, and those two reservations are what capped the alphabet at
249 rather than 251. The format paid for features it never got.
[North Star §4.7](triepack-northstar.md) now forbids reserving what you are
not building.

**Documenting intent as fact.** `architecture.md` describes the design as
planned, in the present tense, with no marker distinguishing built from
intended. A reader cannot tell. That page now carries a status banner and
this document exists as the authority.

**Narrowing without checking.** The alphabet ceiling, the 512 MB ceiling and
the JavaScript/Python/Rust allocation bugs are all the same shape: a wide
value placed into a narrow field, or a length trusted before validation, with
no guard. All now check. The conformance corpus and the sanitizer job exist
to catch the next one.

## 6. Honest summary

TriePack today is a **working, well-tested, genuinely portable static
dictionary format with a serious performance defect in its value path and a
set of documented features that do not exist.**

The portability is the real achievement and it is not overstated: ten native
implementations producing identical bytes, enforced on every build, is rare
and it works. The compression is modest, the value lookup is unusable at
scale, and the surrounding documentation described a more ambitious system
than was ever written.

Version 2 exists to close that gap, and it is no longer only on paper:
`tools/v2_prototype.c` builds the proposed format, verifies every key through
its own serialised bytes, and measures **27,610 bytes against v1's 45,054 on
the same corpus — 39% smaller** — while encoding a 253-byte-value corpus that
v1 refuses outright. See [the v2 plan](internals/v2-implementation-plan.md).

This page will shrink as that lands.
