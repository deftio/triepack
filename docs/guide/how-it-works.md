---
layout: default
title: How It Works
---

# How TriePack Works

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This guide explains the key ideas behind TriePack's compression and lookup
algorithms. For the byte-level binary format, see the
[Format Specification](../internals/format-spec.md). For the bitstream
primitives, see the [Bitstream Specification](../internals/bitstream-spec.md).

---

## 1. The Core Idea: Prefix Tries

A **trie** (from "retrieval") is a tree where each edge is labeled with a
character, and each path from root to leaf spells out a key. Tries have a
useful property: keys that share a common prefix share the same path in the
tree.

### Example: Four Keys

Consider storing `car`, `card`, `care`, `cat`:

```
     root
      │
      c
      │
      a
     / \
    r    t ─── END ("cat")
   /|\
  d  e  END ("car")
  │  │
 END END
("card")("care")
```

The prefix `ca` is stored once, not four times. The prefix `car` is shared
between `car`, `card`, and `care`. This is **prefix sharing** -- the
fundamental source of TriePack's compression.

### Why Not a Hash Table?

A hash table stores each key independently. With 10,000 English words
averaging 8 characters, that's 80,000 bytes of key data plus overhead.
A prefix trie shares the common prefixes, so `un-`, `re-`, `pre-`,
`anti-`, `-tion`, `-ing` etc. are stored once across all words that use
them. In practice, a trie of 10,000 English words can be **2-4x smaller**
than the raw key data.

Additionally, trie lookup is O(key-length), not O(1)-amortized like a hash
table. But tries have no worst-case hash collisions, no rehashing, and no
pointer-chasing through chains -- the decoder walks a sequential bit
stream with skip pointers.

---

## 2. Bit-Level Symbol Packing

Standard tries use one byte per character. TriePack goes further by
analyzing the key alphabet and assigning each character a **fixed-width
code using the minimum number of bits**.

### Symbol Analysis

The encoder scans all keys to find the set of unique byte values (the
"alphabet"). It then adds 6 control codes used for trie structure
(END, END_VAL, SKIP, SUFFIX, ESCAPE, BRANCH) and computes the minimum
bits-per-symbol:

```
Example: keys contain only lowercase English letters (a-z)

Alphabet size:     26 letters
Control codes:      6
Total symbols:     32
Bits per symbol:   5  (because 2^5 = 32)
```

Each character now costs **5 bits instead of 8** -- a 37.5% savings on top
of the prefix sharing. For a typical English word list, this means each
key character costs only 5 bits.

The bits-per-symbol adapts automatically to the data:

| Key data | Unique chars | + 6 controls | bps | Savings vs. 8-bit |
|----------|-------------|--------------|-----|-------------------|
| Lowercase English | 26 | 32 | 5 | 37.5% |
| Alphanumeric | 36 | 42 | 6 | 25% |
| Full ASCII | 95 | 101 | 7 | 12.5% |
| Binary keys | 256 | 262 | 9 | -12.5% (overhead) |

For most text-based keys, `bps` is 5-7, providing significant savings.

### Control Codes

Six symbol codes are reserved for trie structure, not data:

| Code | Name | Purpose |
|------|------|---------|
| END | Terminal (no value) | Marks the end of a key that has no associated value |
| END_VAL | Terminal (with value) | Marks the end of a key that has a value; followed by a VarInt index into the value store |
| SKIP | Skip pointer | Followed by a VarInt bit distance; tells the decoder how far to jump to skip a subtree |
| BRANCH | Branch point | Followed by a VarInt child count; indicates a branching point in the trie |
| SUFFIX | Suffix reference | Reserved for future suffix deduplication |
| ESCAPE | Literal escape | Reserved for future use |

These control codes are mixed into the same symbol stream as data
characters. The decoder reads one `bps`-bit symbol at a time and checks
whether it's a control code or a data character.

---

## 3. The Encoding Process

### Step by Step

Given a set of key-value pairs, the encoder produces a `.trp` blob:

```
1. Sort all keys lexicographically
2. Deduplicate (if the same key was added twice, last value wins)
3. Analyze the alphabet → compute bits-per-symbol, build symbol table
4. Write 32-byte header (placeholder offsets)
5. Write trie configuration (bps, symbol count, symbol table)
6. Encode the prefix trie (two-pass: size calculation, then write)
7. Write the value store (sequential typed values)
8. Align to byte boundary, compute CRC-32 footer
9. Patch header with final offsets, recompute CRC-32
```

### Two-Pass Trie Encoding

The trie is encoded recursively: at each branch, the encoder needs to
write skip pointers that tell the decoder how many bits to jump to reach
each child subtree. But the skip distance depends on the subtree size,
which isn't known until the subtree is encoded.

TriePack solves this with a **two-pass approach**:

1. **Dry-run pass**: Recursively compute the bit count for each subtree
   without writing anything. This gives exact skip distances.

2. **Write pass**: Actually write the trie, using the dry-run sizes to
   emit correct skip pointers.

Both passes use the same recursive algorithm, ensuring consistency.

### Worked Example

Encoding keys `abc=10`, `abd=20`, `xyz=30`:

```
Sorted: abc, abd, xyz

Pass 1 (root):
  No common prefix ('a' vs 'x') → BRANCH with 2 children

  Child 'a' subtree:
    Common prefix "ab" → 2 symbols (10 bits at bps=5)
    Branch at 'c' vs 'd' → BRANCH with 2 children
      Child 'c': symbol + END_VAL + varint = 11 bits
      Child 'd': symbol + END_VAL + varint = 11 bits
    Total 'a' subtree: ~47 bits

  Child 'x' subtree:
    Common prefix "xyz" → 3 symbols + END_VAL + varint = ~21 bits

Pass 2 (write):
  BRANCH 2
  SKIP 47          ← skip past 'a' subtree (47 bits)
  'a' 'b'          ← common prefix symbols
  BRANCH 2
  SKIP 11          ← skip past 'c' child
  'c' END_VAL 0    ← first value (index 0)
  'd' END_VAL 1    ← second value (index 1)
  'x' 'y' 'z' END_VAL 2  ← third value (index 2)
```

The result is a flat bit stream that can be navigated with skip pointers.

---

## 4. The Lookup Algorithm

Looking up a key is a walk through the bit stream, matching one character
at a time:

```
lookup("abd"):
  1. Read symbol → BRANCH (control code)
     Read child count → 2

  2. Read SKIP distance → 47 bits
     Peek first child's first symbol → 'a'
     Key[0] = 'a' → match! Enter this child.

  3. Read symbol → 'a' (matches key[0], advance)
     Read symbol → 'b' (matches key[1], advance)

  4. Read symbol → BRANCH
     Read child count → 2

  5. Read SKIP distance → 11 bits
     Peek first child's first symbol → 'c'
     Key[2] = 'd', not 'c' → skip 11 bits (jump past 'c' subtree)

  6. Read symbol → 'd' (matches key[2], advance)
     Read symbol → END_VAL
     Key fully consumed → FOUND!
     Read value index → 1
     Decode value at index 1 → 20
```

Key properties of the lookup:

- **O(key-length)** -- each character of the search key is examined at most
  once. Non-matching subtrees are jumped over entirely using skip pointers.

- **No decompression** -- the decoder reads directly from the encoded bit
  stream. No intermediate data structures are built.

- **No allocation** -- the decoder uses a cursor into the source buffer.
  The only state is the bit position and the current key index.

### What About Missing Keys?

If a key doesn't exist, the lookup terminates as soon as a mismatch is
found:

```
lookup("axe"):
  1. BRANCH → 2 children
  2. First child starts with 'a' → match, enter
  3. Read 'a' (match), read 'b' → key[1] is 'x', not 'b' → NOT FOUND
```

The decoder returns `TP_ERR_NOT_FOUND` immediately. Only the common prefix
up to the mismatch point is examined.

---

## 5. The Value Store

Values are stored separately from the trie, in a sequential **value
store** that follows the trie data in the `.trp` file.

### Why Separate?

Storing values inline in the trie would bloat the trie structure and
slow down key navigation (the decoder would have to skip over values
it doesn't care about at every branch). By separating keys and values:

- **Key navigation is fast** -- the trie contains only symbols and
  control codes. No value data to skip over.
- **Value access is direct** -- once a key is found, its value index
  points to a specific position in the value store.

### Value Encoding

Each value in the store is prefixed with a 4-bit type tag:

```
┌──────┬───────────────────────────────────────┐
│ tag  │ payload                               │
│ 4 b  │ (depends on type)                     │
└──────┴───────────────────────────────────────┘
```

| Type | Tag | Payload | Example size |
|------|-----|---------|-------------|
| null | 0 | (none) | 4 bits |
| bool | 1 | 1 bit (0=false, 1=true) | 5 bits |
| int | 2 | Zigzag VarInt (signed) | 12 bits for value 42 |
| uint | 3 | VarInt (unsigned) | 12 bits for value 42 |
| float32 | 4 | 32 bits IEEE 754 | 36 bits |
| float64 | 5 | 64 bits IEEE 754 | 68 bits |
| string | 6 | VarInt length + raw bytes | variable |
| blob | 7 | VarInt length + raw bytes | variable |

**VarInt encoding** uses LEB128 (7 data bits + 1 continuation bit per
byte). Small values take fewer bytes:

| Value range | VarInt bytes |
|------------|-------------|
| 0 -- 127 | 1 |
| 128 -- 16,383 | 2 |
| 16,384 -- 2,097,151 | 3 |
| up to 2^64-1 | 10 (max) |

For signed integers, a **zigzag transform** maps small negative numbers
to small unsigned numbers before LEB128 encoding. This way, -1 takes
1 byte (not 10).

### Zero-Copy Strings

String and blob values support **zero-copy access**: the decoded value
contains a pointer directly into the `.trp` buffer. No allocation, no
copying. This is possible because:

1. String data is stored as raw bytes in the value store
2. The value store is byte-aligned before string/blob payloads
3. The decoder returns a `const char *` pointing into the source buffer

The caller must keep the source buffer alive for as long as decoded
string values are in use.

---

## 6. CRC-32 Integrity

The last 4 bytes of a `.trp` file contain a CRC-32 checksum computed
over all preceding bytes. This catches corruption from storage errors,
truncation, or transmission problems.

```
┌──────────────────────────┬────────┐
│  Header + Data Stream    │ CRC-32 │
│  (N bytes)               │ 4 B BE │
└──────────────────────────┴────────┘
                                ↑
                          computed over all N preceding bytes
```

- `tp_dict_open()` verifies the CRC automatically and returns
  `TP_ERR_CORRUPT` on mismatch.
- `tp_dict_open_unchecked()` skips verification for trusted data
  (ROM, firmware) or when speed matters.

The CRC uses the standard reflected polynomial (0xEDB88320), matching
zlib and Ethernet.

---

## 7. ROM Deployment

A key design goal is that `.trp` files can be placed directly in
read-only memory (ROM, flash, PROGMEM) with **zero load-time setup**:

- **No pointers** in the binary format. All references are bit offsets.
- **No relocations** or fixups needed when the file is loaded at a
  different address.
- **No mutation** -- the decoder holds a `const uint8_t *` and never
  writes to the buffer.
- **Minimal allocation** -- the `tp_dict` control structure is ~100
  bytes and can live on the stack.

### Embedding in Firmware

```c
/* Generated by: xxd -i dictionary.trp */
static const uint8_t dict_data[] = {
    0x54, 0x52, 0x50, 0x00, /* TRP\0 magic */
    /* ... rest of .trp data ... */
};

void lookup_example(void) {
    tp_dict *dict = NULL;
    tp_dict_open_unchecked(&dict, dict_data, sizeof(dict_data));

    tp_value val;
    if (tp_dict_lookup(dict, "status.ok", &val) == TP_OK) {
        /* val.data.string_val.str points into dict_data[] */
    }

    tp_dict_close(&dict);
}
```

The `tp_dict_open_unchecked()` call skips CRC verification (the data is
in ROM and presumably correct). The decoded string value points directly
into the `dict_data` array -- no copies, no heap.

### Stateless ROM Functions

For even lower overhead, the bitstream library provides stateless
functions that operate on raw `const uint8_t *` pointers with no object
allocation:

```c
uint64_t val;
tp_bs_read_bits_at(rom_buffer, bit_offset, 5, &val);
```

These are safe to call from interrupt handlers and bare-metal
environments.

---

## 8. JSON Support

The optional `triepack_json` library encodes JSON documents into `.trp`
format by **flattening** the JSON structure into dot-path keys:

```
Input JSON:
{
    "name": "Alice",
    "address": {
        "city": "Portland",
        "zip": "97201"
    },
    "scores": [95, 87, 92]
}

Flattened keys:
  "name"           → string "Alice"
  "address.city"   → string "Portland"
  "address.zip"    → string "97201"
  "scores[0]"      → int 95
  "scores[1]"      → int 87
  "scores[2]"      → int 92
```

These flattened key-value pairs are fed to the standard TriePack encoder.
The hierarchical JSON structure is reconstructed on decode by unflattening
the dot-path keys back into nested objects and arrays.

### Path Lookups

The DOM-style API supports direct path lookups without decoding the
entire document:

```c
tp_json *j = NULL;
tp_json_open(&j, buf, buf_len);

tp_value val;
tp_json_lookup_path(j, "address.city", &val);
/* val.type == TP_STRING, val.data.string_val.str == "Portland" */

tp_json_lookup_path(j, "scores[1]", &val);
/* val.type == TP_INT, val.data.int_val == 87 */

tp_json_close(&j);
```

---

## 9. Alignment and Embedded Use

When deploying TriePack on embedded systems or in ROM, memory alignment
is a practical concern. Here is the current state:

- **Output buffer alignment.** The buffer returned by `tp_encoder_build()`
  is `malloc()`'d, which guarantees `max_align_t` alignment (typically 8
  or 16 bytes). This satisfies any integer or pointer alignment requirement.

- **Header alignment.** The 32-byte header is naturally int-aligned (32
  bytes = 8 x 4-byte words). All header fields are read through the
  bitstream API, not via direct struct casts, so unaligned-access faults
  cannot occur from header reads.

- **Data section.** After the header, data is bit-packed (no alignment
  guarantees) in the default `TP_ADDR_BIT` mode. For `TP_ADDR_BYTE` mode,
  all fields start on byte boundaries. No padding is added between the
  header and data section -- this maximizes compression.

- **Static / ROM placement.** When embedding a `.trp` file in ROM or a
  statically allocated array, ensure the buffer starts on a 4-byte
  boundary:

  ```c
  /* GCC / Clang */
  static const uint8_t dict_data[]
      __attribute__((aligned(4))) = { /* .trp bytes */ };

  /* Or use a linker script to place it in an aligned section */
  ```

- **CRC footer.** The 4-byte CRC-32 footer is byte-aligned (the writer
  calls `tp_bs_writer_align_to_byte()` before writing it).

No internal padding between sections is planned -- the current design
prioritizes compression density. If a future use case requires word-aligned
access to the data section (e.g., DMA transfers), optional alignment padding
can be added as a format extension.

---

## 10. Comparison with Other Formats

| Feature | TriePack | JSON | MessagePack | Protocol Buffers | SQLite |
|---------|----------|------|-------------|-----------------|--------|
| Key compression | Prefix trie (excellent) | None | None | Schema-based (no keys) | B-tree index |
| Value types | 8 types | 4 types | 18 types | Schema-defined | SQL types |
| Lookup speed | O(key-length) | O(n) parse | O(n) scan | O(1) field number | O(log n) B-tree |
| ROM-safe | Yes | No | No | No | No |
| Zero-copy strings | Yes | No | Possible | Possible (FlatBuffers) | No |
| Self-describing | Yes (symbol table) | Yes | Yes | No (needs schema) | Yes |
| Mutable | No (rebuild) | Yes (text) | Yes (rebuild) | Yes (rebuild) | Yes |
| Typical use | Read-mostly dictionaries | Config, APIs | Serialization | RPC, storage | Databases |

TriePack is purpose-built for **read-mostly string-keyed dictionaries**.
It is not a general-purpose serialization format (use MessagePack or
Protocol Buffers for that) and does not support in-place mutation (use
SQLite for read-write workloads). Its sweet spot is static or rarely-
updated dictionaries where compact size and fast lookup matter.

---

## 11. Tools

### CLI Inspector (`trp`)

The `trp` command-line tool inspects `.trp` files without writing any code.
Build it from the `tools/` directory:

```
Inspection:
  trp info     <file>            Show header metadata
  trp list     <file>            List all keys with types and values
  trp get      <file> <key>      Look up a single key
  trp dump     <file>            Hex dump of header + structure summary
  trp search   <file> <prefix>   Prefix search
  trp validate <file>            Validate integrity (magic + checksum)

Conversion:
  trp encode   <input.json> [-o output.trp]   JSON -> TriePack
  trp decode   <input.trp>  [-o output.json]  TriePack -> JSON [--pretty]
```

### Web Inspector (planned)

A browser-based tool for inspecting `.trp` files is planned as a future
enhancement. The JavaScript binding already supports full encode/decode,
making this a natural fit -- either as a pure client-side drag-and-drop
page or a lightweight server-side tool. This is separate from the CLI
tool and is tracked as a future enhancement.

---

---

## 12. Bitstream Primer: Arbitrary-Width Bit Fields

Most programming works with bytes (8 bits), words (16/32/64 bits), and
the standard integer types built on them. TriePack's bitstream library
operates at a lower level: it reads and writes integers of **any width
from 1 to 64 bits**, packed tightly with no wasted space between fields.

### Why Sub-Byte Integers?

Consider storing the letters a-z in a trie. With 26 letters + 6 control
codes = 32 symbols, each symbol needs exactly 5 bits (because 2^5 = 32).
Using a full byte per symbol wastes 3 bits -- a 37.5% overhead.

For 10,000 keys averaging 8 characters each:

```
Full bytes:  10,000 x 8 x 8 = 640,000 bits
5-bit packed: 10,000 x 8 x 5 = 400,000 bits  (37.5% smaller)
```

### Writing and Reading Packed Fields

```c
// Write two values into a single byte
tp_bs_write_bits(w, 21, 5);   // 21 = 10101 (5 bits)
tp_bs_write_bits(w,  3, 3);   //  3 = 011   (3 bits)
// Result: byte = 10101011 = 0xAB
```

The library handles byte-boundary crossing automatically:

```c
tp_bs_write_bits(w, 21, 5);   // 10101 → bits 0-4 of byte 0
tp_bs_write_bits(w, 97, 7);   // 1100001 → spans bytes 0-1
```

Reading is the reverse:

```c
uint64_t val;
tp_bs_read_bits(r, 5, &val);   // reads 5 bits → val = 21
tp_bs_read_bits(r, 7, &val);   // reads 7 bits → val = 97
```

### Signed Fields and Sign Extension

For signed values, the MSB is the sign bit. Reading a 5-bit signed field
containing -3 (bit pattern `11101`):

```c
int64_t val;
tp_bs_read_bits_signed(r, 5, &val);   // reads 11101 → val = -3
```

The library sign-extends the narrow field to `int64_t` automatically.

### VarInt Encoding

For values with unpredictable range, TriePack uses LEB128 VarInt encoding
(7 data bits + 1 continuation bit per byte). Small values take fewer bytes:

| Value range | VarInt bytes |
|------------|-------------|
| 0 -- 127 | 1 |
| 128 -- 16,383 | 2 |
| 16,384 -- 2,097,151 | 3 |

For signed integers, **zigzag encoding** maps small negatives to small
positives before LEB128: -1 becomes 1, 1 becomes 2, -2 becomes 3, etc.

### ROM-Safe Stateless Functions

For embedded systems, stateless functions read from raw `const uint8_t *`
pointers with no reader object:

```c
uint64_t val;
tp_bs_read_bits_at(rom_data, bit_offset, 5, &val);
```

No object, no cursor, no heap. Safe for interrupt handlers.

For the complete bitstream reference, see the
[Bitstream Specification](../internals/bitstream-spec.md).

---

## 13. Performance Benchmarks

The following benchmarks were measured using the project's static test data
files. Compression ratios are deterministic and reproducible; timing numbers
are illustrative (Apple M-series, single-threaded, `-O2`).

### Dictionary Compression

Two word lists were tested: 10,000 real English words from [`common_words_10k.txt`](https://github.com/deftio/triepack/blob/main/tests/data/common_words_10k.txt)
(natural distribution, 82 KB) and 10,000 generated words with shared prefixes/suffixes
(simulating API keys, config paths, or i18n keys).

**10,000 English words** (71,824 raw key bytes)

| Mode | Encoded size | Compression | Bits/key | bps |
|------|-------------|-------------|----------|-----|
| Keys only | 45,054 B | 1.59x | 36.0 | 5 |
| Keys + integer values | 90,451 B | 0.79x | 72.4 | 5 |

**10,000 generated words with shared prefixes** (117,556 raw key bytes)

| Mode | Encoded size | Compression | Bits/key | bps |
|------|-------------|-------------|----------|-----|
| Keys only | 44,552 B | 2.64x | 35.6 | 5 |
| Keys + integer values | 89,774 B | 1.31x | 71.8 | 5 |

Key observations:

- **Prefix sharing matters.** Words with common prefixes (un-, re-, pre-,
  -tion, -ing) compress 2.64x vs. 1.59x for natural English words. API
  endpoint paths, configuration keys, and i18n keys typically share prefixes
  heavily and will see compression closer to the 2.64x figure.

- **Keys-only mode is very compact.** Without values, the trie stores only
  key structure at ~36 bits per key. This is ideal for spell-check
  dictionaries, bloom filter alternatives, and membership-test sets.

- **Values add overhead.** Each value carries a 4-bit type tag plus its
  VarInt-encoded payload. For small integer values, this roughly doubles the
  encoded size.

### Lookup Speed

| Dataset | Keys | Lookup (keys-only) | Lookup (keys+values) |
|---------|------|--------------------|---------------------|
| 10K English words | 10,000 | 3.8 us/key | 551 us/key |
| 10K generated words | 10,000 | 3.7 us/key | N/A |

Keys-only lookups are fast (~3.8 us) because the decoder walks the trie
with skip pointers, examining only the key path. Key+value lookups are
slower because the value store is sequential: looking up value index N
requires skipping N-1 preceding values. For large dictionaries with
values, consider keys-only membership checks when values aren't needed.

### JSON Compression

**Large document: 200-product catalog** ([`benchmark_100k.json`](https://github.com/deftio/triepack/blob/main/tests/data/benchmark_100k.json), 202 KB)

| Metric | Value |
|--------|-------|
| JSON input | 202,408 bytes |
| Encoded .trp | 134,287 bytes |
| Compression | 66.3% of JSON |
| Flattened keys | 5,044 |
| Bits per symbol | 6 |
| Encode time | 14 ms |
| Single path lookup | ~1 us |

The catalog contains 200 products with nested objects (manufacturer,
dimensions, reviews), arrays, strings, numbers, and booleans. The .trp
format achieves 34% savings primarily from key prefix sharing -- repeated
field names like `products[N].name`, `products[N].price` share the
`products[` prefix across all 200 entries.

**Small JSON documents**

| Document | JSON | .trp | Ratio | Keys |
|----------|------|------|-------|------|
| Empty object `{}` | 2 B | 50 B | 2500% | 1 |
| Simple config (3 fields) | 45 B | 96 B | 213% | 4 |
| Nested object (2 levels) | 59 B | 103 B | 175% | 4 |
| Array of 4 strings | 42 B | 113 B | 269% | 5 |
| Mixed types (5 fields) | 61 B | 106 B | 174% | 6 |
| App config (18 fields) | 376 B | 397 B | 106% | 18 |

For small documents, .trp files are **larger** than JSON because of fixed
overhead: 32-byte header, symbol table, CRC-32 footer, and root-type
metadata. The crossover point where .trp becomes smaller than JSON is
approximately **400-500 bytes** of input JSON, depending on key structure.

**When TriePack beats JSON:** Documents with many keys sharing common
prefixes (repeated field names in arrays of objects, deeply nested
configuration, i18n key hierarchies). The 200-product catalog saves 34%
because `products[0].manufacturer.name` through `products[199].manufacturer.name`
share the prefix path.

**When JSON is smaller:** Tiny documents with few keys and no shared
prefixes. For <500 bytes of JSON with unique keys, the header overhead
dominates. Use plain JSON for these cases.

### Running the Benchmarks

The compaction benchmark is built as part of the example suite:

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build
./build/examples/compaction_benchmark
```

The benchmark tool in [`tools/run_benchmarks.c`](https://github.com/deftio/triepack/blob/main/tools/run_benchmarks.c) measures both dictionary
and JSON compression against the static test data files:

```bash
gcc -O2 -o run_benchmarks tools/run_benchmarks.c \
  -Iinclude -Lbuild/src/json -Lbuild/src/core -Lbuild/src/bitstream \
  -ltriepack_json -ltriepack_core -ltriepack_bitstream
./run_benchmarks tests/data
```

---

## Next Steps

- [Getting Started](getting-started) -- build, install, tutorial walkthrough
- [API Reference](api-reference) -- every function with usage examples
- [Binary Format Specification](../internals/format-spec) -- byte-level format details
- [Examples](examples) -- six runnable programs demonstrating different use cases
