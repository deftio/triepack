---
layout: default
title: Format v2 — decoder grammar (draft 2)
---

# TriePack format v2 — decoder grammar

**Draft 2, for review.** Supersedes nothing yet; `format-spec-v2.md` is the
current proposal and §14 lists what this changes about it.

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## 0. What this document is

**This document is the format.** Implementations conform to it; it does not
describe them. Where an implementation and this document disagree, the
implementation is wrong — including the C one.

That inversion is the point. TriePack's reason to exist is that ten
independent implementations produce identical bytes, and the standard for
that is not "ten things that agree with `src/core/`" but "ten things that
agree with a written grammar." JSON has RFC 8259 and hundreds of
implementations; the RFC is why the hundred-and-first is possible. Today
TriePack has C and nine ports of it, and nobody outside this repository can
write the eleventh. This document exists to remove that ceiling.

It is written **decoder-first**, carried forward from the original draft:
the format is defined by what a reader must do with a byte sequence.
Encoders may vary in sophistication as long as what they emit parses to the
same dictionary — with one exception, §12, where canonical output is
required.

---

## 1. Principles

From [the North Star](../triepack-northstar.md), in its priority order —
size, ROM-ability, byte-identity, speed, simplicity — plus two rules this
draft adds.

### 1.1 Nothing is padded because bytes exist

The lineage is `dio_BitFile` (2000): `PutNBitQty(stream, value, n_bits)` and
`GetNBitQty(stream, &value, n_bits)` — write a 5-bit symbol, a 12-bit offset
and a 7-bit value into one continuous stream and read them back as integers.
Arbitrary widths are the native unit here, not an optimisation.

v1 kept that for trie symbols and then contradicted it in the header:
`symbol_count` is a fixed 8-bit field. **That single hardcoded width is the
249-byte-value limit.** Nothing else causes it.

> **Rule 1. Every count, length and offset is either a varint or has its
> width declared by a field that appears earlier in the stream. No format
> constant is a number the reader is expected to already know.**

Applying it removes four hardcoded limits at once, with no new machinery:

| v1 limit | cause | under Rule 1 |
|---|---|---|
| 249 distinct byte values | `symbol_count` fixed at 8 bits | gone |
| 512 MB | 32-bit bit-offsets | gone |
| `bits_per_symbol` 1–8 | 256-entry lookup arrays | not applicable (§6) |
| iteration depth 256 | fixed reader stack | declared in the file (§5.4) |

### 1.2 Alignment is a property of access, not of the file

North Star §6.7 asks for 4-byte section alignment because a Cortex-M0 cannot
do unaligned word access and the rank tables are read a word at a time. Rule
1 asks for bit-packed metadata. These only appear to conflict:

> **Rule 2. Data that is *randomly accessed as machine words* is aligned.
> Data that is *parsed sequentially* is bit-packed.**

Rank/select tables and the LOUDS bit vector are word-addressed: aligned, §7.
Header fields, counts and offsets are read once at open: varints, §5. A
reader parses ~20 varints at open and then does word-aligned access forever.

This is the rule the current v2 proposal breaks: its 80-byte header spends
eight bytes on `num_keys` and eight more on each of six offsets, all 8-byte
aligned, in a format whose premise is that padding is waste. For a 10,000-key
dictionary that header is **80 bytes where 24 would do**, and it is read once.

### 1.3 One format, not a family

North Star §6.10 (marked **[proposed]**, not decided): **there are no profiles and no
optional features.** Every conforming reader implements everything. A `.trp`
that only some readers can open destroys the property the project exists for.

A *feature may be unused in a given file* — a section absent, flagged in the
header — but "absent" is not "optional to implement."

Small embedded readers are served by keeping the **format** small, not by
fragmenting it. §13 makes that a testable requirement rather than a hope.

---

## 2. Notation

```
u(n)        n-bit unsigned integer, most-significant bit first.
            The dio_BitFile GetNBitQty(n) primitive.
varint      LEB128: 7 payload bits per byte, high bit = continuation.
            Byte-granular but NOT byte-aligned: it begins at the current
            bit position. At most 10 groups; an 11th is malformed.
svarint     zigzag then varint: (v << 1) ^ (v >> 63).
bit[k]      k raw bits.
byte[k]     k bytes, starting at the current bit position.
align(n)    skip 0..n-1 zero bits to reach the next n-byte boundary.
            Readers MUST verify the skipped bits are zero.
```

All multi-bit quantities are big-endian within the stream: the first bit read
is the most significant. This matches `dio_BitFile` and is not negotiable
later — it is the one convention that, if ambiguous, makes every
implementation disagree.

---

## 3. Object

A TriePack object is a map from **byte strings** to **typed values**. Keys
may contain any of the 256 byte values, including NUL, in any position.
There is no alphabet, no symbol table and no escape mechanism (§6).

Duplicate keys are not representable. An encoder given duplicates is
encouraged to warn; the format cannot express them.

---

## 4. File

```
file  =  magic  version  chunk*  crc
```

| | |
|---|---|
| `magic` | `byte[4]` = `54 52 50 00` (`"TRP\0"`) |
| `version` | `u(8)` major, `u(8)` minor |
| `crc` | `u(32)`, CRC-32 (IEEE 802.3, reflected, init `0xFFFFFFFF`, final xor `0xFFFFFFFF`) over every byte from `magic` through the last chunk |

A reader rejects a file whose `major` it does not implement. It accepts any
`minor` of a major it implements — see §4.2.

A valid CRC means the file is **intact**, never that it is **trustworthy**.
Anyone who can supply a file can supply a matching checksum.

### 4.1 Chunks

```
chunk  =  type:varint  flags:u(8)  length:varint  payload:byte[length]  align(4)
```

`length` is the payload in bytes, excluding padding. Each chunk's payload
begins 4-byte aligned, satisfying Rule 2 for the tables inside it.

| `flags` bit | Name | Meaning |
|---|---|---|
| 0 | `MUST_UNDERSTAND` | A reader that does not know `type` must reject the file |
| 1–7 | reserved | Must be 0; readers reject non-zero |

Chunk types defined by this version:

| Type | Name | Required | §|
|---|---|---|---|
| 1 | `META` | yes | 5 |
| 2 | `LOUDS` | yes | 7 |
| 3 | `LABELS` | yes | 8 |
| 4 | `TAILS` | when `META.has_tails` | 9 |
| 5 | `TERMINALS` | yes | 10 |
| 6 | `VALUES` | when `META.has_values` | 11 |

`META` is first. The rest may appear in any order; a reader indexes them
before walking. A duplicate chunk type is malformed. A missing required chunk
is malformed.

### 4.2 Why chunks, and what they are not for

This is the change from the current proposal, which uses a fixed 80-byte
header with a 16-bit flags word and ten reserved bits.

A fixed header means **every future feature is a format break.** That is
already observable: v1 reserved `has_suffix_table`, `has_nested_dicts` and
`compact_mode` as header bits, and four years later all three are still
unbuilt, because there was no way to add the machinery incrementally. North
Star §6.9 — *reserve nothing you are not building* — is the right rule, and
chunks are what make it affordable: you do not need to reserve space when you
can add a chunk.

**Chunks do not create optional features.** Within one major.minor, every
chunk type above is mandatory to implement (§1.3). `MUST_UNDERSTAND` exists
for *version evolution*: a later minor version adding a chunk marks it
must-understand if ignoring it would silently change meaning, and clears it
if the chunk is a pure accelerator an older reader can safely skip. That is
how a format stays extensible without ever splitting its reader population
inside a version.

---

## 5. META chunk

```
num_keys        varint
num_nodes       varint
max_depth       varint     longest key in bytes
flags           u(8)
```

| bit | name |
|---|---|
| 0 | `has_tails` |
| 1 | `has_values` |
| 2–7 | reserved, must be 0, readers reject non-zero |

Then, present only when the corresponding flag is set, in this order:

```
tail_pool_bits  varint     when has_tails    -- bit length of the tail pool
value_bits      varint     when has_values   -- see §11
```

### 5.1 No offsets

The current proposal stores six 8-byte section offsets. Chunk framing already
locates every section, so they are redundant — and a redundant offset is a
second source of truth that can disagree with the first. Removed.

### 5.2 num_keys and num_nodes are varints

At 10,000 keys, `num_keys` is 2 bytes rather than 8. At 10^12 keys it is 6.
It is never truncated, which is Rule 1 doing its job: the 512 MB ceiling and
the 249-value ceiling were both fixed-width fields meeting a larger world.

### 5.3 Deriving widths instead of storing them

A reader computes what it needs:

```
node_index_bits = ceil(log2(num_nodes + 1))
```

Storing that would let a file declare a width inconsistent with its own node
count — a malformed state that has to be detected and rejected. Deriving it
makes the state unrepresentable. **Prefer derivation to declaration wherever
the value is a function of something already present.**

### 5.4 max_depth

Bounds the reader's traversal stack, so a bound exists and is stated rather
than hardcoded at 256 as in v1. A reader may refuse a file whose `max_depth`
exceeds what it can allocate — that is a resource decision, not a conformance
failure, and it must be an explicit error rather than a truncated walk.

---

## 6. Keys, labels and the absence of an alphabet

**All 256 byte values are legal in keys, everywhere.** There is no symbol
table, no `bits_per_symbol`, no control-code space, no ESCAPE.

v1 put control codes and data symbols in one code space sized by one 8-bit
field, so structural vocabulary stole from data vocabulary: 255 − 6 = 249.
Measured on the shipped library, 249 distinct byte values encodes and reads
back; 250 returns `TP_ERR_ALPHABET`.

Two of those six stolen codes — `SUFFIX` and `ESCAPE` — **are never emitted
by any encoder and never read by any decoder.** They appear only as
`#define`s. The format has been paying two alphabet slots for four years for
features that do not exist.

v2 separates the two concerns completely (North Star §6.8): **structure lives
in a bit vector (§7), labels are raw bytes (§8).** They share no code space,
so there is nothing to run out of. ESCAPE is not unimplemented here; it is
unnecessary, and this draft deletes it rather than carrying the reservation
forward.

---

## 7. LOUDS chunk — structure

The trie is a **radix trie**: each edge carries a first byte (its label) and
an optional tail of further bytes (§9).

Structure is a LOUDS bit vector. Each node contributes `1` per child followed
by a single `0`, in breadth-first order, preceded by a `10` super-root.

```
rank_params     u(16) superblock_bits, u(16) block_bits
num_bits        varint
align(4)
bits            bit[num_bits]
align(4)
superblocks     u(64) x ceil(num_bits / superblock_bits)
blocks          u(32) x ceil(num_bits / block_bits)
```

`superblock_bits` and `block_bits` are **declared, not assumed** — Rule 1.
This version's encoders emit 2048 and 256; a reader must honour whatever the
file says. Both must be powers of two, `block_bits` must divide
`superblock_bits`, and `block_bits` ≥ 64.

The tables are word-addressed and therefore aligned, Rule 2.

Navigation, with `rank1(i)` = number of `1`s below bit `i`:

```
child_begin(v) = (v == 0) ? 0 : select0(v - 1) + 1
child_count(v) = select0(v) - child_begin(v)
child(v, k)    = rank1(child_begin(v) + k) + 1
```

`select0` is a binary search over the rank tables; it is not stored.

> **Editorial note for review.** LOUDS is the hardest thing in this document
> to implement identically ten times — an off-by-one in `select0` yields a
> file that decodes to plausible garbage rather than an error. That is a
> reason to specify it *this* precisely and to test it adversarially (§13),
> not a reason to avoid it: a design that cannot be written down well enough
> to implement ten times is telling you it is too complicated, and that is
> feedback worth having at design time.

---

## 8. LABELS chunk

```
labels  byte[num_nodes - 1]
```

One byte per node except the root, in the same breadth-first order as LOUDS,
so `labels[v - 1]` is the label of node `v`. Children of a node are ordered
by ascending label, which makes the search among siblings a binary search and
makes iteration order lexicographic by construction.

No transformation, no symbol table, no packing. This is the chunk that makes
§6 true.

---

## 9. TAILS chunk

Present when `META.has_tails`. An edge whose key segment is longer than one
byte stores the remainder here.

```
pool_bits    varint
align(4)
pool         bit[pool_bits]
tail_count   varint
refs         (varint offset, varint length) x tail_count
```

Tails are stored **suffix-merged**: a tail that is a suffix of another is a
reference into it at a different offset, not a second copy. This is the
suffix sharing the original draft specified and v1 never built
(`src/core/suffix.c` is fourteen lines ending in `/* TODO */`).

Which node owns which tail is derived from the terminal bitmap and node
order; it is not stored separately, per §5.3.

---

## 10. TERMINALS chunk

```
rank_params  u(16) superblock_bits, u(16) block_bits
align(4)
bitmap       bit[num_nodes]
align(4)
superblocks  u(64) x ...
blocks       u(32) x ...
```

Bit `v` is 1 when node `v` ends a key. The value ordinal of a terminal node
is `rank1(bitmap, v)`.

**This is the fix for v1's worst defect.** v1 stored a varint index at each
terminal pointing into the value store, so resolving one value meant decoding
every preceding one: measured 328 µs at 10k keys, 7,524 µs at 175k, 30,500 µs
at 699k, with full decode O(n²). The prototype measures 2.06 / 2.59 / 3.17 µs
at those same three sizes.

The defect came from carrying a 2001 idea forward while changing what it
held: the original stored a *fixed-width integer* at each terminal, which is
random-access. Replacing it with a varint index kept the shape and lost the
property. **Values are addressed by rank, never by a stored index.**

---

## 11. VALUES chunk

Present when `META.has_values`.

```
type_bits    varint
align(4)
types        bit[num_keys * type_bits]
payload      ...
```

Values are in value-ordinal order (§10). `type_bits` is declared because a
dictionary using only `null` and `uint` needs one bit per value, not eight.

| Code | Type | Payload |
|---|---|---|
| 0 | null | none |
| 1 | bool | `u(1)` |
| 2 | uint | varint |
| 3 | int | svarint |
| 4 | float32 | `u(32)` IEEE-754 |
| 5 | float64 | `u(64)` IEEE-754 |
| 6 | string | varint length, then bytes |
| 7 | blob | varint length, then bytes |

Codes above `2^type_bits - 1` cannot occur; a file declaring `type_bits` too
small for a type it uses is malformed.

### 11.1 Open question for review

Values are currently a flat store with **no sharing at all** — the reason
JSON does so badly, where on enwik9-as-JSON 99.8% of output was value store.
The original draft specified a **second trie** for values with an optionally
shared symbol table. That is the largest remaining compression win in the
format and this draft does not yet specify it.

Three options, in ascending order of work:

1. **Flat store** (above). Simple; no sharing.
2. **Suffix-share the value store.** Values are byte strings; pool them the
   way §9 pools tails.
3. **A second trie**, as the original draft specified. Values become keys of
   their own trie; terminals point by rank into it.

A measured result bearing on this: concatenating key + separator + value into
one trie beat a value dictionary, 70,665 against 84,166 bytes, because tries
share *suffixes* while dictionaries only dedupe exact matches. That argues
against (1) and suggests (3) is worth its cost, but the comparison was not
against (2). **This needs a measurement before it is specified.**

---

## 12. Canonical encoding

Decoder-first everywhere else; here encoders are constrained, because
byte-identity across implementations is the property the project exists for
and it is not achievable if two encoders may legally disagree.

1. Children are ordered by ascending label.
2. Nodes are numbered breadth-first from the root.
3. Every edge segment longer than one byte goes in the tail pool; segments of
   exactly one byte never do.
4. The tail pool is suffix-merged maximally: no tail in the pool is a proper
   suffix of another at a different offset.
5. `type_bits` is the smallest value admitting every type used.
6. `rank_params` are the encoder's choice but must satisfy §7's constraints.
7. Reserved bits are zero.

Two conforming encoders given the same key-value set emit identical bytes.
This is testable and §13 requires that it be tested.

---

## 13. Conformance

### 13.1 The corpus is generated from this document

Not from any implementation. An implementation passes by matching the corpus;
the corpus does not record what an implementation happens to do.

### 13.2 Differential testing is required

Round-tripping your own output proves very little. Measured, in this
repository: a wire-format bug in a sibling project survived **3,000
randomised round-trips in each of two implementations independently**,
because each decoded its own output the way it had encoded it. It surfaced in
minutes once one implementation's bytes were fed to another's parser.

**Every implementation must be tested against bytes produced by a different
implementation.** Self-round-trip is necessary and not sufficient.

### 13.3 Reader size budget

North Star §6.10 says embedded readers stay small by keeping the format small.
That is only meaningful if measured:

> A complete reader — open, validate, lookup, iterate — should be under
> ~1,500 lines in a typical language.

For calibration, the existing v1 implementations are 940 (Python), 1,150
(Kotlin), 1,152 (JavaScript), 1,156 (Swift), 1,307 (Go), 1,585 (Java) and
1,798 (Rust) lines. **This is the number that makes the format portable, and
a proposed feature that pushes it up is spending the project's core asset.**

### 13.4 Encoder memory budget

> An encoder must build a dictionary in **≤ 8× the input size** in peak
> resident memory, and this must be tested at 1 GB.

Measured on the current v2 prototype: 420 MB for 5 MB of input, 834 MB for
10 MB, 1,654 MB for 20 MB — **~87×, linear**, extrapolating to roughly 87 GB
for the 1 GB input the North Star requires support for.

The cause is a build strategy, not the format: the prototype builds a
pointer-based trie with one heap-allocated node per *byte* of every key
(~56 bytes of struct plus allocator overhead plus a children array), and
compresses to a radix trie afterwards. The fix is to build the radix trie
directly, or to sort keys and build incrementally so only the path from the
last key is resident.

It is stated here because "embedded-grade" applied only to the reader in the
original draft, and nothing constrained the encoder. A format usable only by
a machine with 87 GB of RAM has a portability problem in the other direction.

---

## 14. What this changes about `format-spec-v2.md`

| | Current proposal | Here | Why |
|---|---|---|---|
| Header | fixed 80 bytes, 8-byte fields | magic + version + chunks | Rule 1; extensibility without reserving |
| Section offsets | six `u64` | none | chunk framing already locates them; §5.1 |
| `num_keys`, `num_nodes` | `u64` | varint | Rule 1 |
| Rank parameters | fixed 2048 / 256 | declared per file | Rule 1 |
| Future features | ten reserved flag bits | new chunk types | North Star §6.9 |
| `type_bits` | implied 8 | declared | Rule 1 |
| Reader size | unstated | ≤ ~1,500 lines, tested | §13.3 |
| Encoder memory | noted as a Phase 3 blocker | ≤ 8× input, tested, measured today at 87× | §13.4 |
| Differential testing | not required | required | §13.2 |
| Value sharing | flat store | open question, needs measurement | §11.1 |

Unchanged from the current proposal: LOUDS with rank/select, rank-addressed
values, suffix-merged tails, all 256 byte values legal, CRC-32, and the North
Star priority order.

---

## 15. Open questions

1. **§11.1 — value sharing.** Flat, suffix-shared, or a second trie. Needs a
   measurement of option 2, which has never been run.
2. **Rule 2 against North Star §6.7.** This draft reads §6.7 as "align what
   is word-addressed," which permits varint metadata. If §6.7 means all
   section *contents* are 4-byte aligned, §5 needs rewriting.
3. **Huffman labels.** The current proposal has flags for it. This draft
   omits it: it is a size-vs-simplicity trade that costs ten implementations,
   and the North Star ranks size first but simplicity last. Worth measuring
   before deciding.
4. **`max_depth` as refusal.** §5.4 lets a reader refuse a file it cannot
   allocate for. That is arguably a conformance hole — a file some readers
   cannot open — and §1.3 says those are forbidden. Alternative: a mandatory
   minimum depth all readers must support.
