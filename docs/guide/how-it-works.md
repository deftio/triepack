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

## 9. Comparison with Other Formats

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

## Next Steps

- [Getting Started](getting-started.md) -- build, install, write your first program
- [API Reference](api-reference.md) -- every function with usage examples
- [Binary Format Specification](../internals/format-spec.md) -- byte-level format details
- [Examples](examples.md) -- six runnable programs demonstrating different use cases
