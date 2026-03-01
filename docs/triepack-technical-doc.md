---
layout: default
title: Technical Deep Dive
---

# TriePack Technical Documentation

This document describes the internal workings of the TriePack library:
how keys and values are encoded, how the binary format is structured,
and how lookup, encoding, and decoding algorithms operate.

---

## 1. Introduction

TriePack is a compact binary format for storing string-keyed dictionaries
using a compressed prefix trie. It is designed for:

- **Compact storage** -- prefix sharing, variable-width symbol encoding,
  and bit-level packing minimize size.
- **Fast lookup** -- O(key-length) lookups via skip-pointer trie navigation.
- **ROM-ability** -- the format can be placed in read-only memory with
  no load-time fixups.
- **Typed values** -- null, bool, integers, floats, strings, and blobs.
- **Integrity** -- CRC-32 footer protects the entire file.

The library is organized as a stack:

```
  triepack_bitstream   -- bit-level I/O (read/write arbitrary-width fields)
       |
  triepack_core        -- trie encoder/decoder, header, values, CRC
       |
  triepack_json        -- JSON overlay (separate library)
```

---

## 2. File Format Overview

A `.trp` file has three regions:

```
 Offset   Size     Description
 ──────   ──────   ───────────────────────────────────
 0        32 B     Header (fixed size)
 32       var      Data stream (trie config + trie + values)
 end-4    4 B      CRC-32 footer
```

All multi-byte integers are big-endian (MSB first).

---

## 3. Header Format

The 32-byte header is:

```
 Byte  Width  Field
 ────  ─────  ──────────────────────────
 0-3     4    Magic bytes: "TRP\0" (0x54 0x52 0x50 0x00)
 4       1    version_major (must be 1)
 5       1    version_minor
 6-7     2    flags (bit field)
 8-11    4    num_keys
 12-15   4    trie_data_offset (bits, relative to byte 32)
 16-19   4    value_store_offset (bits, relative to byte 32)
 20-23   4    suffix_table_offset (reserved, 0)
 24-27   4    total_data_bits
 28-31   4    reserved (0)
```

### Flags

| Bit | Name                 | Meaning                          |
|-----|----------------------|----------------------------------|
| 0   | has_values           | Value store is present           |
| 1   | has_suffix_table     | Suffix table is present (future) |
| 2   | has_nested_dicts     | Nested dict values (future)      |
| 3   | compact_mode         | Compact encoding variant         |

---

## 4. Data Stream

The data stream immediately follows the header (byte 32). It contains:

1. **Trie configuration** (symbol table metadata)
2. **Prefix trie** (the compressed key structure)
3. **Value store** (typed values, sequential)

### 4.1 Trie Configuration

```
 Width         Field
 ─────         ──────────────────────────
 4 bits        bits_per_symbol (bps)
 8 bits        symbol_count (alphabet_size + 6 control codes)
 6 * bps bits  special_symbol_map (one code per control code)
 N varints     symbol_table (one varint per alphabet symbol)
```

- `bits_per_symbol` determines how many bits each symbol occupies.
  With `bps=5`, you can encode up to 32 symbols (26 letters + 6 controls).
- The first 6 symbol codes are reserved for control codes.
- The symbol table maps each code (from index 6 onward) to a byte value
  using VarInt encoding.

### 4.2 Control Codes

Six control codes are used in the trie stream:

| Code | Name     | Meaning                                        |
|------|----------|------------------------------------------------|
| 0    | END      | Terminal node, no value                        |
| 1    | END_VAL  | Terminal node with value (followed by VarInt)  |
| 2    | SKIP     | Skip pointer (followed by VarInt distance)     |
| 3    | SUFFIX   | Jump to suffix table (reserved, not implemented) |
| 4    | ESCAPE   | Literal next symbol (reserved)                 |
| 5    | BRANCH   | Branch point (followed by VarInt child count)  |

---

## 5. Trie Encoding

Keys are sorted lexicographically and encoded into a compressed prefix
trie. The encoding uses a recursive algorithm:

### 5.1 Algorithm

Given a sorted set of entries with a known prefix length:

1. **Find common prefix**: Extend the prefix while all entries share the
   same character at that position.

2. **Write common prefix symbols**: Emit the symbol code for each
   shared character.

3. **Check for terminal**: If any entry's key ends exactly at the common
   prefix, emit END or END_VAL (with a VarInt value index).

4. **Count children**: Group remaining entries by their character at the
   branch position.

5. **Encode children**:
   - **0 children**: Done (leaf node).
   - **1 child, no terminal**: Continue linearly (no BRANCH needed).
     The single child's distinguishing character is implicit.
   - **Multiple children** (or 1 child + terminal): Emit BRANCH +
     VarInt child count. For each child except the last, emit SKIP +
     VarInt skip distance. Then recursively encode each child subtree.

### 5.2 Worked Example

Encoding keys `{"abc", "abd", "xyz"}` with integer values `{10, 20, 30}`:

```
Sorted entries: abc=10, abd=20, xyz=30

Step 1: trie_write(0..3, prefix=0)
  Common prefix: empty (entries start with 'a','a','x' -- not all same)
  No terminal at position 0
  Children: group 'a' = [abc, abd], group 'x' = [xyz]
  Emit: BRANCH 2

  Child 'a' (not last): compute subtree size for SKIP distance
    Emit: SKIP <distance>
    Recurse: trie_write(0..2, prefix=0)
      Common prefix: "ab" (both start with 'a','b')
      Write symbols: 'a' 'b'
      No terminal at position 2
      Children: group 'c' = [abc], group 'd' = [abd]
      Emit: BRANCH 2
      Child 'c': SKIP <dist>, recurse
        Write: 'c' END_VAL 0
      Child 'd': last child, no SKIP
        Write: 'd' END_VAL 1

  Child 'x' (last): no SKIP
    Recurse: trie_write(2..3, prefix=0)
      Common prefix: "xyz"
      Write symbols: 'x' 'y' 'z'
      Terminal: END_VAL 2
```

Result stream: `BRANCH 2 SKIP <n> 'a' 'b' BRANCH 2 SKIP <m> 'c' END_VAL 0 'd' END_VAL 1 'x' 'y' 'z' END_VAL 2`

### 5.3 Two-Pass Encoding

SKIP distances must be known before writing, but they depend on the
subtree sizes. The encoder uses a two-pass approach:

1. **Dry-run pass** (`trie_subtree_size`): Recursively computes the bit
   count for each subtree without writing. Tracks the value index counter
   so VarInt sizes for value indices are exact.

2. **Write pass** (`trie_write`): Actually writes the trie, using the
   dry-run pass to compute each SKIP distance on the fly.

---

## 6. Value Store

When `has_values` is set, the value store follows the trie data. It
contains typed values in the order they appear during the trie traversal
(depth-first, left-to-right).

Each value is encoded as:

```
 4 bits    type tag
 variable  payload (depends on type)
```

### Value Types

| Tag | Type    | Payload                                       |
|-----|---------|-----------------------------------------------|
| 0   | null    | (none)                                        |
| 1   | bool    | 1 bit (0=false, 1=true)                       |
| 2   | int     | Signed VarInt (zigzag encoded)                 |
| 3   | uint    | Unsigned VarInt                                |
| 4   | float32 | 32 bits (IEEE 754)                             |
| 5   | float64 | 64 bits (IEEE 754)                             |
| 6   | string  | VarInt length + raw UTF-8 bytes                |
| 7   | blob    | VarInt length + raw bytes                      |
| 8   | array   | (reserved)                                     |
| 9   | dict    | (reserved)                                     |

---

## 7. VarInt Encoding

VarInts use LEB128 encoding: each byte uses 7 data bits and 1
continuation bit (MSB). The continuation bit is 1 if more bytes follow,
0 for the last byte.

### Unsigned Example

```
Value  Bytes
─────  ────────────────
0      0x00
127    0x7F
128    0x80 0x01
300    0xAC 0x02
```

### Signed (Zigzag)

Signed integers use zigzag encoding before LEB128:
`zigzag(n) = (n << 1) ^ (n >> 63)`

```
Value  Zigzag  Bytes
─────  ──────  ──────
 0     0       0x00
-1     1       0x01
 1     2       0x02
-2     3       0x03
```

---

## 8. Lookup Algorithm

To look up a key in a compiled dictionary:

```
1. Seek to trie_start
2. While true:
   a. Read one symbol (bps bits)
   b. If END:
        - Key fully consumed? -> found (null value)
        - Otherwise: look for BRANCH next, or NOT_FOUND
   c. If END_VAL:
        - Read VarInt value index
        - Key fully consumed? -> found (decode value at index)
        - Otherwise: look for BRANCH next, or NOT_FOUND
   d. If BRANCH:
        - Read VarInt child_count
        - For each child (except last): read SKIP + distance
        - Peek first symbol of each child
        - If it matches key[key_idx]: consume symbol, advance, continue
        - If no match after all children: NOT_FOUND
   e. If regular symbol:
        - Compare to key[key_idx]
        - Match: advance key_idx, continue
        - Mismatch: NOT_FOUND
```

### Value Decoding

When a value index is found:
1. Seek to `value_start`
2. Skip `value_index` values (decode and discard each)
3. Decode the target value

String and blob values use zero-copy when the buffer is byte-aligned:
the decoded value points directly into the source buffer.

---

## 9. Integrity Check

The last 4 bytes of a `.trp` file contain a CRC-32 checksum (big-endian)
computed over all preceding bytes (header + data stream, aligned).

```
CRC-32 polynomial: 0xEDB88320 (reflected)
Initial value:     0xFFFFFFFF
Final XOR:         0xFFFFFFFF
```

`tp_dict_open()` verifies the CRC. `tp_dict_open_unchecked()` skips
verification for trusted data (faster).

---

## 10. Symbol Analysis

The encoder automatically determines the optimal `bits_per_symbol`:

1. Scan all keys to find unique byte values (the alphabet).
2. Compute `total_symbols = alphabet_size + 6` (control codes).
3. Find the minimum `bps` such that `2^bps >= total_symbols`.
4. Assign control codes to slots 0-5.
5. Assign alphabet characters to slots 6+, in byte-value order.

For English lowercase keys (26 letters + 6 controls = 32), `bps=5`.
For ASCII keys with digits and punctuation, `bps=7` is typical.

---

## 11. Addressing Modes

The format supports three addressing modes (stored in header flags):

| Mode             | Description                                |
|------------------|--------------------------------------------|
| TP_ADDR_BIT      | Maximum density; symbols packed at bit level |
| TP_ADDR_BYTE     | Byte-aligned symbols; simpler decoding      |
| TP_ADDR_SYMBOL   | UTF-8 or fixed-width symbol alignment       |

The current implementation (v1.0) uses bit addressing for the trie and
byte addressing for values.

---

## 12. ROM-ability

The `.trp` format is designed for read-only memory:

- All offsets are either absolute bit positions or relative to the data
  stream start.
- The decoder (`tp_dict`) holds a const pointer to the buffer with no
  mutation.
- String and blob values use zero-copy pointers directly into the buffer.
- No load-time fixups, relocations, or memory allocation beyond the
  `tp_dict` control structure.

This makes TriePack suitable for embedded systems where the dictionary
can be compiled into firmware and stored in flash or ROM.

---

## 13. Encoding Pipeline

The full encoding pipeline in `tp_encoder_build()`:

```
1. Sort entries lexicographically
2. Deduplicate keys (last-added wins)
3. Detect if any values are non-null (set has_values flag)
4. Analyze symbols: scan keys, compute bps, build symbol maps
5. Write 32-byte header (placeholder offsets)
6. Write trie config: bps, symbol_count, control code map, symbol table
7. Record trie_data_offset
8. Encode prefix trie (two-pass: dry-run for SKIP sizes, then write)
9. Record value_store_offset
10. Write value store (sequential typed values)
11. Align to byte boundary
12. Compute CRC-32, write 4-byte footer
13. Patch header with final offsets
14. Recompute CRC-32 over patched data
15. Return buffer to caller
```

---

## 14. API Summary

### Encoder

```c
tp_encoder_create(&enc);                  // create with defaults
tp_encoder_add(enc, "key", &value);       // add key/value pair
tp_encoder_build(enc, &buf, &len);        // build compressed trie
tp_encoder_reset(enc);                    // clear for reuse
tp_encoder_destroy(&enc);                 // free
```

### Decoder

```c
tp_dict_open(&dict, buf, len);            // open with CRC check
tp_dict_open_unchecked(&dict, buf, len);  // open without CRC
tp_dict_lookup(dict, "key", &val);        // O(key-length) lookup
tp_dict_contains(dict, "key", &found);    // existence check
tp_dict_count(dict);                      // number of keys
tp_dict_get_info(dict, &info);            // format metadata
tp_dict_close(&dict);                     // free
```

---

## 15. Future Work

- **Suffix table**: Cross-branch deduplication of common endings
  (e.g., "-ing", "-tion"). The header reserves space for this.
- **Huffman symbols**: Frequency-based variable-width symbol encoding
  for large dictionaries.
- **Nested dict values**: Store sub-dictionaries as values.

---

## See Also

- [How It Works](guide/how-it-works.md) -- conceptual guide with diagrams
- [Binary Format Specification](internals/format-spec.md) -- byte-level format
- [Bitstream Specification](internals/bitstream-spec.md) -- bit-level I/O details
- [API Reference](guide/api-reference.md) -- function-by-function usage guide
