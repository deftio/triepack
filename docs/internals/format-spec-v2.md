---
layout: default
title: Binary Format Specification v2
---

# TriePack Format Specification — Version 2 (Proposed)

<!-- Copyright (c) 2026 M. A. Chatterjee -->

**Status: proposal.** Nothing implements this yet. v1 is what ships; see
[Format Specification v1](format-spec.md). The goals this serves are in the
[North Star](../triepack-northstar.md).

Version 2 is a **breaking change**. `version_major` becomes `2`, so v1
readers reject v2 files by their existing version check and v2 readers reject
v1 files. There is no compatibility shim and no converter: nothing is
deployed on v1, so nothing needs migrating.

## 1. Why a new format

Five problems in v1, four of them structural:

**The alphabet ceiling.** Control codes and data symbols shared one code
space sized by an 8-bit `symbol_count`, capping keys at 249 distinct byte
values. Binary keys were unrepresentable, and exceeding the cap silently
produced a valid-CRC file whose every lookup failed.

**Explicit subtree extents.** Every child but the last carried `SKIP` plus a
varint bit-distance so the walk could find subtree boundaries. That is pure
navigational overhead, and mis-deriving it was issue #1.

**No suffix sharing.** `TP_CTRL_SUFFIX`, `suffix_table_offset` and
`src/core/suffix.c` were all reserved or stubbed and never implemented, so
every occurrence of a common ending was stored in full.

**O(n) value lookup.** `END_VAL` stored a varint index, and resolving it
meant seeking to the value store and decoding every preceding value. Measured
on `common_words_10k.txt`: **2.73 µs/key without values, 328 µs/key with
them** — a 120× cliff, quadratic in dictionary size.

**A 512 MB ceiling.** `total_data_bits` and every section offset are `u32`
counting *bits*, so the data stream cannot exceed 2³² bits ≈ 512 MB. A 1 GB
input is not merely slow in v1; it is unrepresentable.

## 2. Design summary

| Concern | v1 | v2 |
|---|---|---|
| Structure | symbols + `SKIP` distances | LOUDS bit vector |
| Structure cost | `bps` bits/node + branch varints | ~2 bits/node + ~8% index |
| Labels | remapped codes, shared with controls | **raw bytes, all 256 legal** |
| Alphabet limit | 249 | **none** |
| Subtree extents | explicit varints | implicit |
| Suffix sharing | none | tail pool with dedup, indexed per occurrence |
| Label width | `bps`, shared with control codes | `⌈log2(alphabet)⌉`, shared with nothing |
| Entropy coding | none | optional, per section, encoder measures |
| Value index | varint per key | derived by rank, not stored |
| Value lookup | O(n) scan | O(1) or ≤32 skips |
| Addressing | u32 bits (512 MB) | **u64 bytes (≥ 1 GB)** |
| Load-time work | none | none |
| Allocation to open | one struct | zero |

The unifying idea: **structure and labels never share a namespace.** The tree
shape lives in a bit vector, labels are raw bytes, and there are no control
codes at all. No shared code space means no ceiling to overflow, no symbol
table, no escape mechanism, and no class of bug where a wide alphabet
corrupts a file.

## 3. File layout

```
Offset    Size       Section
──────    ────────   ─────────────────────────────────────
0         80 B       Header
80        variable   LOUDS bit vector
…         variable   LOUDS rank index
…         variable   Label array
…         variable   Tail presence bitmap + rank index
…         variable   Tail offset array + tail length array
…         variable   Tail pool
…         variable   Terminal bitmap + rank index
…         variable   Value store
…         variable   Value offset index (optional)
end-4     4 B        CRC-32 footer
```

Every section begins on a **4-byte boundary**; the encoder inserts zero
padding as needed. Sections absent from a file have offset `0`.

All multi-byte integers are **big-endian**. All offsets are **byte offsets
from the start of the file** — file-relative, so the blob is position
independent.

## 3.1 Conventions

Everything in this document obeys these four rules. They are stated once
because most cross-implementation disagreement comes from leaving them
implicit.

**Integers.** All multi-byte integers are **big-endian** (most significant
byte first), whether in the header, an index table or a packed array.

**Bit vectors.** Bit `i` of a bit vector is
`byte[i >> 3] & (0x80 >> (i & 7))` — **most significant bit first** within
each byte. Bits past the declared length in the final byte are zero and are
not part of the vector.

**Packed integer arrays.** Several sections store fixed-width values narrower
than a byte: the label array, the tail reference array, the tail dictionary.
Element `i` of a `w`-bit array occupies bits `[i × w, (i+1) × w)` of the
array, numbered by the bit-vector rule above, and the value is stored with
its **most significant bit first**. So a 5-bit array holding `0b10110,
0b00011` begins `10110 000 | 11...` — byte `0xB0`, then the next element
continues in the following byte. Arrays are padded to a whole number of bytes
with zero bits.

This is the rule that makes `label[i]` reachable by arithmetic rather than by
walking, and it must be identical everywhere or descent silently reads the
wrong symbol.

**Sections and padding.** Every top-level section begins on a **4-byte
boundary** relative to the start of the file. The encoder inserts zero bytes
as needed; a reader must not assume the padding is absent. Offsets in the
header are byte offsets **from the start of the file**, so the blob is
position independent: a reader adds its own base pointer.

## 4. Header (80 bytes)

```
Byte    Width   Field
────    ─────   ────────────────────────────────────────────
0-3     4       magic: "TRP\0" (0x54 0x52 0x50 0x00)
4       1       version_major = 2
5       1       version_minor = 0
6-7     2       flags
8-15    8       num_keys
16-23   8       num_nodes
24-31   8       louds_offset
32-39   8       labels_offset
40-47   8       tails_offset          (0 when HAS_TAILS is clear)
48-55   8       terminal_offset
56-63   8       value_store_offset    (0 when HAS_VALUES is clear)
64-71   8       total_size            (bytes, including CRC)
72-79   8       reserved              (must be 0)
```

Every field from byte 8 on is 8-byte aligned, so a reader may load the header
as `u64`s on parts that require aligned access.

### 4.1 Flags

| Bit | Name | Meaning |
|---|---|---|
| 0 | `HAS_VALUES` | Value store present |
| 1 | `HAS_TAILS` | Tail pool present |
| 2 | `VALUE_FIXED_WIDTH` | All values share one bit width (see §9.2) |
| 3 | `VALUE_SAMPLED_INDEX` | Sampled value offset index present |
| 4 | `HUFFMAN_LABELS` | Label array is Huffman-coded (§8.2) |
| 5 | `HUFFMAN_TAIL_POOL` | Tail pool is Huffman-coded (§8.2) |
| 6-15 | reserved | **must be 0**; readers must reject non-zero |

Bit 6-15 rejection is deliberate. v1 reserved bits that became load-bearing
by accident; here unused space stays genuinely recoverable.

### 4.2 Widths and the 1 GB requirement

`num_keys`, `num_nodes` and every offset are **u64 counting bytes**. This is
the direct fix for v1's 512 MB ceiling.

Worked limits:

| Quantity | Limit | Note |
|---|---|---|
| File size | 2⁶⁴ bytes | offsets are u64 |
| Keys | 2⁶⁴ | |
| Nodes | 2⁶⁴ | |
| Tail pool | 2⁶⁴ bytes | `off_width` declared per file |
| Single tail length | 2³² bytes | `len_width` declared per file |
| Key length | unbounded | limited only by node count |
| Distinct byte values | **256** | no alphabet table exists |

A 1 GB input is an ordinary case, not an edge case. §12 specifies the
large-input test obligations.

### 4.3 Sub-sections

The header names five top-level sections. The remaining structures are
**sub-sections of those**, not independently addressed:

| Sub-section | Lives inside | Located by |
|---|---|---|
| LOUDS rank index | LOUDS section | §5.1 layout, after the bit vector |
| Tail presence bitmap + rank | tail section | first sub-section at `tails_offset` |
| Tail reference arrays | tail section | follows the presence bitmap |
| Tail pool | tail section | follows the reference arrays |
| Terminal rank index | terminal section | §5.1 layout, after the bit vector |
| Value offset index | value section | follows the value store |

Each ranked bit vector carries its own `n` (§5.1), and each variable array is
sized by a count already known from the header (`num_nodes`, `num_keys`) or
by a `u64` length at the head of its sub-section. Nothing requires a separate
directory, and there is none: a reader walks each section from its declared
offset.

Adding a future top-level section takes a minor version and one of the
reserved bytes, per [North Star §6.9](../triepack-northstar.md).

## 5. The rank primitive

One primitive, used three times (LOUDS, tail presence, terminals). Specifying
it once and reusing it is the single biggest lever on implementation cost
across ten languages.

### 5.1 Layout

A ranked bit vector of `n` bits is stored as:

```
u64          n            (bit count)
⌈n/8⌉ B      bits         (big-endian within each byte: bit i is
                           byte[i>>3] & (0x80 >> (i&7)))
pad to 4     
⌈n/2048⌉×8   superblocks  u64: rank1 of all bits before this superblock
⌈n/256⌉×2    blocks       u16: rank1 within the superblock, before
                           this block
```

Overhead is `64/2048 + 16/256` ≈ **9.4%** of the bit vector.

### 5.2 rank1(i) — number of 1 bits strictly before position i

```
sb    = i / 2048
blk   = i / 256
count = superblocks[sb] + blocks[blk]
count += popcount of bits in [blk*256, i)
```

The tail is at most 256 bits: four 64-bit popcounts plus a masked partial.
Implementations must produce identical results; popcount may be a builtin or
a portable fallback.

`rank0(i) = i - rank1(i)`.

### 5.3 select0(k) / select1(k) — position of the k-th 0/1 bit, 0-indexed

Binary search over `superblocks`, then over `blocks`, then a linear scan
within the 256-bit block. O(log n) with **no additional stored structure**.

This is a deliberate trade. A broadword select index would be O(1) and
faster; it would also be ten subtly different implementations of the
trickiest code in the format. Size is priority 1 and lookup speed is
priority 4 — see [North Star §5](../triepack-northstar.md).

## 6. Tree structure (LOUDS)

### 6.1 Encoding

Nodes are numbered `0 … num_nodes-1` in **breadth-first order**; node `0` is
the root. The bit vector is the concatenation, in that order, of `1^d 0` for
each node of degree `d`.

No super-root prefix is used.

### 6.2 Navigation

For node `v`:

```
child_begin(v) = (v == 0) ? 0 : select0(v - 1) + 1
child_count(v) = select0(v) - child_begin(v)
child(v, k)    = rank1(child_begin(v) + k) + 1        for 0 ≤ k < child_count(v)
```

### 6.3 Worked example

Root with two children, both leaves:

```
degrees:  node0=2, node1=0, node2=0
bits:     1 1 0   0        0
index:    0 1 2   3        4
```

- `child_begin(0) = 0`, `child_count(0) = select0(0) - 0 = 2 - 0 = 2` ✓
- `child(0,0) = rank1(0) + 1 = 0 + 1 = 1` ✓
- `child(0,1) = rank1(1) + 1 = 1 + 1 = 2` ✓
- `child_begin(1) = select0(0) + 1 = 3`, `child_count(1) = select0(1) - 3 = 3 - 3 = 0` ✓
- `child_begin(2) = select0(1) + 1 = 4`, `child_count(2) = select0(2) - 4 = 0` ✓

Implementations must reproduce this example exactly; it is the first
conformance vector.

## 7. Labels

The **label array** holds one entry per node except the root, indexed by
`node - 1`. `label[c-1]` identifies the first byte of the edge entering node
`c`.

Labels are coded at `label_bits = ⌈log2(alphabet_size)⌉`, where
`alphabet_size` is the number of distinct byte values the keys actually use.
A **label alphabet table** of `alphabet_size` bytes precedes the array,
mapping code → byte value.

Because a label is at a computable bit offset (`(c-1) × label_bits`), random
access is preserved and descent can still binary-search a node's children.

> **This is v1's `bits_per_symbol` idea, without the defect that made it
> dangerous.** v1 packed control codes into the same code space and stored
> the total in an 8-bit `symbol_count`, so `alphabet + 6 ≤ 255` capped keys
> at 249 distinct byte values. Here **nothing else shares the space**:
> structure lives in the LOUDS bit vector and there are no control codes at
> all, so `alphabet_size` may be the full 256 and `label_bits` at most 8.
> The ceiling is gone because the coupling is gone, not because a field grew.

Measured on `common_words_10k.txt`: 26 distinct bytes, `label_bits = 5`, and
the label array falls from 12,949 bytes to 8,120 — **37% of the section, 8%
of the file**, for one multiply and shift per access.

**The tail pool uses the same code.** A tail is the bytes after an edge's
first byte, drawn from the same key text, so the alphabet is computed over
labels *and* pool bytes together and both are stored at `label_bits`. Leaving
the pool as raw bytes was measured and is a mistake worth naming: on a corpus
of 20,000 path-like keys the pool is 56% of the file, and storing it at 8
bits instead of 5 cost 85,141 bytes — enough to make v2 **larger than v1** on
that corpus (361,567 against 306,178). Coding it brings the same corpus to
276,426.

**Children are ordered by ascending label byte**, which is what makes descent
binary-searchable and encoder output deterministic.

## 8. Tails

A tail is the run of bytes on an edge after its first byte — the radix-trie
compression of non-branching chains, plus deduplication across the whole
dictionary.

Present when `HAS_TAILS` is set. The section begins with its widths, then
four parts in this order:

```
Offset within the tail section
────────────────────────────────────────────────────────────────
0      u8    index_bits   bits per reference   = ceil(log2(distinct))
1      u8    offset_bits  bits per pool offset = ceil(log2(pool_size))
2      u8    length_bits  bits per tail length = ceil(log2(max_len + 1))
3      u8    reserved (0)
4      u64   distinct     number of distinct tails
12     u64   pool_size    bytes in the pool
20     ...   presence bitmap, ranked per §5 (num_nodes - 1 bits)
...    ...   reference array   (distinct_occurrences x index_bits)
...    ...   tail dictionary   (distinct x (offset_bits + length_bits))
...    ...   tail pool         (pool_size bytes)
```

Each part is padded to a 4-byte boundary.

1. **Presence bitmap** — `num_nodes - 1` bits, ranked per §5. Bit `c-1` is
   set if the edge into node `c` carries a tail.
2. **Reference array** — one `index_bits`-wide entry per set bit, in bitmap
   order, indexed by `rank1(presence, c-1)`. The value is an index into the
   tail dictionary.
3. **Tail dictionary** — one entry per distinct tail: an `offset_bits` pool
   offset followed by a `length_bits` length, packed with no gap between
   them, so entry `d` starts at bit `d × (offset_bits + length_bits)`.
4. **Tail pool** — `pool_size` raw bytes.

A reader must reject a file whose three width fields are not the minimum for
the declared `distinct`, `pool_size` and maximum length: the widths are a
pure function of those counts, so any other value means the file was not
produced by a conforming encoder.

Resolving the tail on the edge into node `c`:

```
if not presence[c-1]: no tail
d      = reference[rank1(presence, c-1)]
offset = dictionary[d].offset
length = dictionary[d].length
bytes  = pool[offset .. offset+length)
```

**Why an index rather than a pair.** The obvious encoding — a `(pool offset,
length)` pair per tail *occurrence* — was measured and is badly wrong. On
`common_words_10k.txt` there are 7,056 occurrences of just 696 distinct
tails, so pairs cost **35,280 bytes to point into a 2,064-byte pool**: the
references outweigh the data seventeen to one, and the whole file came out
28% *larger* than v1. Indexing the distinct tail instead costs 10 bits per
occurrence plus a 696-entry dictionary — 10,212 bytes, a 3.5× reduction, and
the difference between the format losing to v1 and beating it by 39%.

Widths are declared rather than fixed so the format imposes no cap of its
own. Suffix sharing still requires `(offset, length)` in the *dictionary* — a
tail may point into the interior of a longer one, where no length prefix can
exist — but that pair is now stored once per distinct tail rather than once
per occurrence.

### 8.1 Deduplication algorithm (normative)

Byte-identity requires this be an algorithm, not an aspiration
([North Star §6.2](../triepack-northstar.md)). The encoder **must** perform
exactly these steps:

1. Collect the set of **distinct** tails.
2. Sort them by their **reversed** byte sequence, ascending — that is,
   compare last byte first, then second-to-last, and so on; a shorter tail
   that is a suffix of a longer one sorts before it.
3. Walk the sorted list from **last to first**, tracking `prev`, the most
   recently emitted tail:
   - If the current tail is a proper suffix of `prev`, emit no bytes and
     record `(offset(prev) + len(prev) − len(current), len(current))`.
     `prev` is unchanged.
   - Otherwise append its bytes to the pool, record `(offset, length)`, and
     set `prev` to it.

Worked example — tails `{ing, string, ring}`:

```
reversed:        gni      gnir     gnirts
sorted:          ing      ring     string
walk (last→first):
  string   emit at 0, length 6      pool = "string"
  ring     suffix of "string"       -> (2, 4)
  ing      suffix of "string"       -> (3, 3)
```

Three tails, six bytes, one allocation. This is the `-tion` / `-ing` /
`-ment` sharing the original two-trie design was for.

**Complexity.** Sorting dominates: O(t log t) comparisons over t distinct
tails, then a single linear pass. Suffix tests compare against `prev` only.
This matters — the [§12.2](#122-large-inputs) encoder budget has to hold at
1 GB, and an algorithm that searched the whole pool for each tail would be
quadratic and could not.

**This is not optimal packing.** A tail that is a suffix of some *earlier*
emitted tail, but not of `prev`, is stored again; interior substring sharing
(`ring` inside `strings`) is not attempted at all. Optimal packing is the
shortest-common-superstring problem, which is NP-hard. The spec claims
determinism and linear-after-sort cost, not minimality.

## 8.2 Optional second-pass Huffman

After the trie and tail decomposition, two sections are byte-oriented and
frequency-skewed: the label array and the tail pool. A Huffman pass over them
is permitted, flagged by `HUFFMAN_LABELS` and `HUFFMAN_TAIL_POOL`.

**The encoder must measure, not assume.** It builds both encodings, keeps the
smaller, and sets the flag accordingly. This is not a tuning knob for callers
— it is a property of the data, and the measurements below are why:

| Corpus | Raw keys | Without Huffman | With | Saving |
|---|---:|---:|---:|---:|
| `common_words_10k.txt` | 71,824 | 27,610 | 25,518 | **7.6%** |
| 20k path-like keys | 682,642 | 361,567 | 256,579 | **29.0%** |
| 20k random binary keys | 160,523 | 237,923 | 237,697 | **0.1%** |

Three corpora, three different right answers. A format that always applied it
would pay decode complexity for nothing on the third; one that never did
would leave 29% on the table on the second.

**Most of Huffman's apparent win is not Huffman.** Against *raw bytes* it
looks like a 46% saving on labels — but §7 already codes labels at
`label_bits`, and 26 symbols in 5 bits is most of that. Measured against the
alphabet-width baseline the further gain is 20.6% on those sections, which is
7.6% of the file. The honest comparison is the second one.

**Cost.** Huffman codes are variable-length, so a coded section loses O(1)
indexing. This is why the flags are per-section and why labels are the
riskier of the two: descent binary-searches the label array, so coding it
means decoding a node's whole child run instead of indexing into it. The tail
pool is compared sequentially once located, so coding it costs nothing
structural.

Canonical Huffman, code lengths limited to 15 bits, with the length table
stored as one byte per alphabet symbol. Ties in code assignment broken by
ascending symbol value, so the tree is reproducible across implementations.

## 9. Terminals and values

### 9.1 Terminal bitmap

`num_nodes` bits, ranked per §5. Bit `v` is set if a stored key ends at node
`v`. For a terminal node `v`, its value's ordinal is:

```
value_index(v) = rank1(terminal_bits, v)
```

**No index is stored per key.** v1's `END_VAL` varint is gone; the ordinal is
derived. This is both a size win and the mechanism that removes v1's O(n)
value scan.

`popcount(terminal_bits) == num_keys` — a cheap structural check.

### 9.2 Value store

Values are stored in ascending `value_index` order. The section begins with
its mode, then the values:

```
Offset within the value section
────────────────────────────────────────────────────────────────
0      u8    mode         0 = scan, 1 = fixed width, 2 = sampled
1      u8    reserved (0)
2      u16   sample_period  (mode 2 only; 0 otherwise)
4      u32   value_width    (mode 1 only, in bits; 0 otherwise)
8      u64   store_bits     total bits of value data
16     ...   value data
...    ...   offset index   (mode 2 only)
```

Each value is:

```
4 bits     type tag
variable   payload
```

| Tag | Type | Payload |
|---|---|---|
| 0 | null | none |
| 1 | bool | 1 bit |
| 2 | int | signed LEB128 (zigzag: `(v << 1) ^ (v >> 63)`) |
| 3 | uint | unsigned LEB128 |
| 4 | float32 | 32 bits, IEEE 754 binary32, big-endian |
| 5 | float64 | 64 bits, IEEE 754 binary64, big-endian |
| 6 | string | unsigned LEB128 byte length, pad to byte boundary, then raw UTF-8 |
| 7 | blob | unsigned LEB128 byte length, pad to byte boundary, then raw bytes |

Tags 8-15 are **invalid** and a reader must reject them. v1's `array` and
`dict` tags are removed rather than reserved — per
[North Star §6.9](../triepack-northstar.md), space is claimed when a feature
is built.

**LEB128**, normatively: seven bits of payload per byte, least significant
group first, high bit set on every byte except the last. At most 10 groups;
an eleventh means the value is malformed, not merely large.

Locating value `i` depends on the mode:

**Mode 1, `VALUE_FIXED_WIDTH`** — every value encodes to exactly
`value_width` bits, so value `i` starts at bit `i × value_width`. **O(1),
zero index.** The encoder **must** choose this whenever it holds; it is the
common case for uniformly typed dictionaries.

**Mode 2, `VALUE_SAMPLED_INDEX`** — a `u64` bit offset every
`sample_period` values (32 unless measured otherwise), stored after the value
data. Locate sample `i / sample_period`, then skip `i mod sample_period`
values. **Bounded by `sample_period` skips**, overhead
`8 / sample_period` bytes per value.

**Mode 0, scan** — from the start. Permitted only when
`num_keys ≤ sample_period`, so the bound is the same constant.

A file whose mode does not satisfy its own precondition is malformed. v1's
quadratic value path is closed here by construction rather than by
convention: there is no mode in which locating a value is O(n).

Strings and blobs are zero-copy — the decoder returns a pointer into the file
buffer, which is why they are byte-aligned.

### 9.3 Values do not share, and that is a decision to revisit

**As specified above, values get no sharing at all.** Keys are trie-compressed
and tails are suffix-merged; the value store is a flat sequence in terminal
order. Two keys with the same value store it twice. This is worth stating
because it is not obvious from the structure and it is where the remaining
size is.

Three ways to fix it were measured on `benchmark_100k.json` (5,043 keys,
2,280 distinct values — 55% repeat):

| Option | Bytes | |
|---|---:|---|
| A — flat store, no sharing (as specified) | 91,780 | +30% |
| B — `key` + separator + `value` as one trie | **70,665** | **best** |
| C — flat store plus a value dictionary | 84,166 | +19% |

**B is the surprise.** Concatenating the value onto the key and letting the
trie hold both beats a value dictionary by 19%, because a dictionary
deduplicates only *identical* values while a trie shares *suffixes* between
different ones: `Product A-123` and `Product B-123` share `-123` in a trie
and are two full entries in a dictionary.

**B is also the worst option on other data.** The same measurement over
10,000 keys with different value distributions:

| Values | Flat store | One trie | |
|---|---:|---:|---|
| All distinct (a row index) | 56,084 | 99,308 | flat store wins by 77% |
| Ten distinct | 44,340 | 37,034 | one trie wins by 16% |
| All identical | 109,340 | 34,329 | one trie wins by 69% |

Distinct values have nothing to share, and concatenation additionally throws
away the typed encoding: an integer that costs 12 bits as a tagged LEB128
becomes five or more characters of text.

**And the alphabets differ.** On the JSON corpus keys use 38 distinct byte
values and values use 66; their union needs 7 bits where keys alone need 6.
Merging the two regions therefore taxes every key byte ~17% to accommodate
value bytes. A region-aware code — the separator marks the boundary, so a
reader always knows which side it is on — would recover roughly 6% of the
whole file. Keys and values are simply different distributions and a single
model fits neither well.

**A second measurement changes the conclusion.** Converting 16 MB of enwik9
from XML to JSON and encoding that:

| | Bytes | Of the XML |
|---|---:|---:|
| Raw XML | 16,776,986 | 1.000 |
| Key structure alone | 25,335 | **0.002** |
| Value store (article text) | 15,800,399 | 0.942 |
| Total, values in a flat store | 15,825,734 | 0.943 |
| Total, key + separator + value in one trie | **13,952,685** | **0.832** |

The key structure is remarkable — 11,335 keys in 25 KB, because
`pages[N].revision.text` repeats 2,267 times — and **irrelevant**, because
99.8% of the file is the value store.

The one-trie form wins by 12%, but not for the reason it won on the JSON
benchmark. Its tail pool went from 15,796,978 bytes to 13,822,356, and
`15,796,978 × 7/8 = 13,822,356` exactly: **the whole gain is the 7-bit
alphabet code, with nothing left over for sharing.** Deduplication reached
only 26% of tails and its reference overhead consumed the rest.

So the useful conclusion is narrower than "put values in the trie". What
these values needed was §7's treatment — bytes stored at the width their
alphabet requires — and they can have that inside a flat value store,
keeping typed encoding and O(1) location. **Alphabet-coding string and blob
payloads is the cheaper half of the win and costs none of the structure.**

**Unresolved.** No option wins everywhere, so this is specified as A for now
and listed in §14. The likely answer follows the pattern §8.2 already sets
for Huffman: the encoder builds more than one and keeps the smaller, with a
header flag saying which. That needs a wider corpus before it is fixed in the
format.

## 10. CRC-32 footer

Unchanged from v1: the final 4 bytes are a big-endian CRC-32 (reflected
polynomial `0xEDB88320`, initial `0xFFFFFFFF`, final XOR `0xFFFFFFFF`) over
every preceding byte.

## 11. Lookup

```
node = 0
i    = 0
while i < key_len:
    find child c of node whose label == key[i]      # binary search
    if none: return NOT_FOUND
    i += 1
    if edge c has a tail:
        compare tail bytes against key[i …]
        if mismatch: return NOT_FOUND
        i += tail_len
    node = c
if terminal_bits[node]:
    return value_at(rank1(terminal_bits, node))
return NOT_FOUND
```

No allocation. No decoding of unrelated subtrees. No `SKIP` distances to
mis-derive.

## 11.1 A complete worked example

The whole format on one tiny dictionary, so an implementer has something to
check against before facing a real corpus.

**Input:** `{"he": 1, "hello": 2, "help": 3}`

### The radix trie

`he` is a stored key and also the road to two others, so it branches:

```
        (root, node 0)
           | 'h'  tail "e"
        (node 1)  terminal, value 1
           |
        BRANCH
         /      \
     'l'          (nothing else)
   tail "l"
   (node 2)
      |
   BRANCH
    /    \
 'o'      'p'
(node 3)  (node 4)
 term=2    term=3
```

Radix compression folds the single-child chains: the edge into node 1 carries
first byte `h` and tail `"e"`; the edge into node 2 carries `l` and tail
`"l"`; nodes 3 and 4 carry `o` and `p` with no tail.

BFS order is exactly that numbering: 0, 1, 2, 3, 4.

### LOUDS (§6)

Degrees in BFS order are 1, 1, 2, 0, 0:

```
node:    0     1     2      3   4
bits:   "10"  "10"  "110"  "0" "0"     ->  1010110 00
```

Nine bits: `101011000`. Check `child(0,0)`:
`child_begin(0) = 0`, `rank1(0) + 1 = 1` — node 1, correct.

### Labels (§7)

Four non-root nodes, so four labels: `h`, `l`, `o`, `p`. Four distinct byte
values, so `label_bits = 2` and the alphabet table is `h l o p` (sorted
ascending: `h`=0x68, `l`=0x6C, `o`=0x6F, `p`=0x70).

```
alphabet table: 68 6C 6F 70            (4 bytes)
codes:          h=0, l=1, o=2, p=3
label array:    00 01 10 11            (4 x 2 bits = 1 byte: 0x1B)
```

### Tails (§8)

Two edges carry tails: node 1 has `"e"`, node 2 has `"l"`.

```
presence bitmap (num_nodes - 1 = 4 bits):  1100
```

Distinct tails are `"e"` and `"l"`. Sorted by reversed bytes ascending:
`"e"` then `"l"`. Walking last to first: `"l"` is emitted at offset 0; `"e"`
is not a suffix of `"l"`, so it is emitted at offset 1.

The dictionary is indexed in that same sorted order, so `"e"` is entry 0 and
`"l"` is entry 1, each carrying the offset the walk assigned:

```
pool:            "le"          (2 bytes)
dictionary:      [0] = (offset 1, length 1)   <- "e"
                 [1] = (offset 0, length 1)   <- "l"
```

Widths:
`index_bits = ceil(log2(2)) = 1`, `offset_bits = ceil(log2(2)) = 1`,
`length_bits = ceil(log2(2)) = 1`.

```
reference array (2 entries x 1 bit):  0 1        node 1 -> "e", node 2 -> "l"
dictionary (2 entries x 2 bits):      1 1 | 0 1  (offset, length) each
```

### Terminals and values (§9)

Nodes 1, 3 and 4 are terminals; 0 and 2 are not:

```
terminal bitmap (5 bits):  01011
```

`popcount = 3 = num_keys`. Value ordinals come from `rank1`:

| Node | Key | `rank1(terminals, node)` | Value |
|---|---|---|---|
| 1 | `he` | 0 | 1 |
| 3 | `hello` | 1 | 2 |
| 4 | `help` | 2 | 3 |

Each value is `uint` (tag 3) with a one-byte LEB128 payload, so every value
is 12 bits — uniform. The encoder must therefore select
`VALUE_FIXED_WIDTH` with `value_width = 12`, and store no index at all.

```
values:  3|1  3|2  3|3        (4-bit tag + 8-bit LEB128, x3 = 36 bits)
```

### Looking up `hello`

```
node = 0, i = 0
  children of 0: [1], labels ['h'] -> match key[0]='h', node = 1, i = 1
  node 1 has tail "e" -> key[1..2) == "e", i = 2
node = 1, i = 2
  children of 1: [2], labels ['l'] -> match key[2]='l', node = 2, i = 3
  node 2 has tail "l" -> key[3..4) == "l", i = 4
node = 2, i = 4
  children of 2: [3,4], labels ['o','p'] -> binary search finds 'o', node = 3, i = 5
i == key_len, terminals[3] is set
  ordinal = rank1(terminals, 3) = 1
  value   = store[1 x 12 bits] = uint 2
```

No allocation, no subtree decoded that is not on the path, and no `SKIP`
distance to mis-derive.

## 11.2 What a reader must reject

A conforming reader rejects a file when any of the following holds. These are
checks a hostile file must not be able to walk past, and they are the
malformed corpus of §12.1.

| Check | Failure |
|---|---|
| Magic is not `TRP\0` | bad magic |
| `version_major != 2` | version |
| Any reserved bit or field is non-zero | corrupt |
| CRC-32 does not match | corrupt |
| A section offset is not 4-byte aligned, is 0 for a section the flags require, or lies outside the file | corrupt |
| Sections overlap | corrupt |
| `popcount(terminal_bits) != num_keys` | corrupt |
| A rank index disagrees with its own bit vector | corrupt |
| A declared width is not the minimum for its count | corrupt |
| A tail reference indexes past the dictionary, or a dictionary entry past the pool | corrupt |
| A value tag is 8-15 | corrupt |
| A LEB128 run exceeds 10 groups | corrupt |
| Value mode 0 with `num_keys > sample_period` | corrupt |

Rejection must be a returned error. The v1 failures that motivated v2 — a
wide alphabet and an oversized data stream — both produced files with a valid
CRC that decoded to garbage, which is the outcome this table exists to
prevent.

## 12. Conformance and scale obligations

### 12.1 Cross-implementation

The existing corpus discipline carries forward: every implementation must
decode each fixture to identical values **and** re-encode to identical bytes.
The rank primitive (§5) and the LOUDS example (§6.3) get their own vectors,
independent of any dictionary, because everything else is built on them.

### 12.2 Large inputs

The suite must cover inputs up to **1 GB**. These fixtures are generated, not
committed — a 1 GB file does not belong in git.

| Case | Shape | What it proves |
|---|---|---|
| `large-flat` | 50M short distinct keys | `num_keys`/`num_nodes` past u32 |
| `large-deep` | keys sharing 10 KB prefixes | node counts, tail splitting |
| `large-wide` | every node at max fan-out | label array, rank at scale |
| `large-binary` | keys over all 256 byte values | no alphabet path exists |
| `large-values` | 1 GB of blob values | value store offsets past 4 GB |

Required assertions: offsets beyond 2³² are addressed correctly; rank/select
are correct past 2³² bits; peak decoder RSS stays within a small constant of
the mapped file; lookup latency does not degrade with dictionary size.

**Encoder budget.** Global optimisation is legal
([North Star §6.1](../triepack-northstar.md)) but not unbounded. The encoder
must build a 1 GB dictionary in **≤ 8× input size** of RAM, which the tail
deduplication of §8.1 must respect — the sort is over distinct tails, not
over occurrences.

> **Measured, and currently violated.** `tools/v2_prototype.c` encodes
> `enwik9` (1 GB, 10.9M keys) correctly — every key and value verified — but
> peaks at **60.2 GB, 7.5× over budget**. It inserts one node per input byte
> and compresses afterwards, so ~987M nodes exist before collapsing to 10M.
> The serialised output is unaffected; this is a build-strategy defect, and a
> conforming encoder must construct the radix trie directly rather than
> compressing a byte trie. Tracked as a Phase 3 blocker.

## 13. Removed from v1

| Removed | Reason |
|---|---|
| `bits_per_symbol`, `symbol_count`, symbol table | labels are raw bytes |
| All six control codes | structure is the bit vector |
| `SKIP` distances | extents implicit |
| `ESCAPE` | no code space to escape from |
| `SUFFIX` code + `suffix_table_offset` | replaced by §8 |
| `END_VAL` value index | derived by rank (§9.1) |
| `has_nested_dicts`, `compact_mode` flags | reserved, never implemented |
| Value tags 8-9 (`array`, `dict`) | reserved, never implemented |
| Bit-granular section offsets | byte offsets, 4-byte aligned |

## 14. Open questions

Flagged rather than silently decided:

1. **DAG merging.** §8 shares suffix *bytes* but not branching subtrees. Full
   DAFSA merging would compress more, and is incompatible with LOUDS as
   specified, which encodes a tree. Deferred; would be v3 and a much larger
   change.
2. **Sample interval.** 32 is a guess. Measure before fixing.
3. **Superblock/block sizes** (2048/256) are conventional, not measured.
4. **Value sharing** (§9.3). Values currently get none. Concatenating them
   onto keys wins by 23% where values repeat and loses by 77% where they do
   not, and a value dictionary is worse than both on the corpus measured.
   Needs a wider corpus, and probably an encoder that measures and flags.
5. **Alphabet-coded value payloads** (§9.3). String and blob bytes are stored
   raw at 8 bits while labels and tails use the alphabet width. On wiki text
   that alone is a 12.5% saving on the whole file, it preserves typed values
   and O(1) location, and it is a much smaller change than restructuring
   values into the trie. Measure before the spec freezes.
6. **Region-aware label coding** (§9.3). If values ever share the trie, keys
   and values should not share a symbol code: their alphabets differ enough
   (38 vs 66 distinct bytes on the JSON corpus) that one code taxes both.
