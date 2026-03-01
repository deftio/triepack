---
layout: default
title: Binary Format Specification
---

# TriePack Format Specification

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This document defines the binary format of `.trp` files produced by the
TriePack encoder and consumed by the TriePack decoder.

---

## 1. Overview

A `.trp` file stores a string-keyed dictionary using a compressed prefix
trie with typed values. The format is designed for:

- Compact storage (prefix sharing, bit-level packing)
- Fast O(key-length) lookups via skip pointers
- ROM-safe deployment (no load-time fixups)
- CRC-32 integrity verification

### File Layout

```
Offset    Size       Description
──────    ──────     ───────────────────────────────────
0         32 B       Header (fixed size)
32        variable   Data stream (trie config + trie + values)
end-4     4 B        CRC-32 footer
```

All multi-byte integers in the header are **big-endian** (MSB first).

---

## 2. Header (32 Bytes)

```
Byte    Width   Field
────    ─────   ──────────────────────────────────
0-3     4       Magic bytes: "TRP\0" (0x54 0x52 0x50 0x00)
4       1       version_major (must be 1)
5       1       version_minor (currently 0)
6-7     2       flags (bit field, see below)
8-11    4       num_keys (number of keys in dictionary)
12-15   4       trie_data_offset (bits, relative to byte 32)
16-19   4       value_store_offset (bits, relative to byte 32)
20-23   4       suffix_table_offset (reserved, must be 0)
24-27   4       total_data_bits (total bits in data stream)
28-31   4       reserved (must be 0)
```

### 2.1 Magic Bytes

The first four bytes identify a `.trp` file: `0x54 0x52 0x50 0x00`
(ASCII "TRP" followed by a null byte). A reader must reject files
that do not start with this sequence.

### 2.2 Version

`version_major` identifies breaking format changes. The current version
is 1. A reader must reject files with an unrecognized major version.
`version_minor` identifies backwards-compatible additions.

### 2.3 Flags

| Bit | Name                 | Meaning                              |
|-----|----------------------|--------------------------------------|
| 0   | `has_values`         | Value store is present               |
| 1   | `has_suffix_table`   | Suffix table is present (reserved)   |
| 2   | `has_nested_dicts`   | Contains nested dict values (reserved)|
| 3   | `compact_mode`       | Compact encoding variant (reserved)  |
| 4-15| (reserved)           | Must be 0                            |

### 2.4 Offsets

All offsets are in **bits**, relative to byte 32 (the start of the data
stream). To compute the absolute bit position:

```
absolute_bit_pos = 256 + offset_value
```

(256 = 32 header bytes × 8 bits)

---

## 3. Trie Configuration

The data stream begins immediately after the header (at byte 32). The
first section is the trie configuration:

```
Width           Field
─────           ──────────────────────────────────
4 bits          bits_per_symbol (bps)
8 bits          symbol_count (alphabet + 6 control codes)
6 × bps bits    control_code_map (one code per control code)
N VarInts       symbol_table (byte value per non-control code)
```

### 3.1 bits_per_symbol

Determines how many bits each trie symbol occupies. The encoder
auto-selects the minimum width to cover all symbols:

```
total_symbols = unique_byte_values_in_keys + 6 (control codes)
bps = ceil(log2(total_symbols))
```

Common values:

| bps | Max symbols | Typical use                      |
|-----|-------------|----------------------------------|
| 5   | 32          | English lowercase (26) + controls|
| 6   | 64          | Alphanumeric + controls          |
| 7   | 128         | Full ASCII                       |
| 8   | 256         | Binary keys                      |

### 3.2 Control Code Map

Six control codes are read as `bps`-bit values. The map tells the
decoder which code value corresponds to each control:

| Index | Name       | Meaning                                    |
|-------|------------|--------------------------------------------|
| 0     | `END`      | Terminal node, no value                    |
| 1     | `END_VAL`  | Terminal with value (VarInt index follows)  |
| 2     | `SKIP`     | Skip pointer (VarInt bit distance follows)  |
| 3     | `SUFFIX`   | Suffix reference (reserved, not implemented)|
| 4     | `ESCAPE`   | Literal escape (reserved)                  |
| 5     | `BRANCH`   | Branch point (VarInt child count follows)  |

### 3.3 Symbol Table

For each non-control code (codes 6 through `symbol_count - 1`), a
VarInt-encoded byte value is stored. This maps code → byte value:

```
code 6  → VarInt → byte value (e.g., 'a' = 97)
code 7  → VarInt → byte value (e.g., 'b' = 98)
...
```

The decoder builds a reverse map (byte value → code) for lookup.

---

## 4. Prefix Trie

The prefix trie immediately follows the trie configuration. Keys are
sorted lexicographically and encoded into a compressed prefix trie using
a recursive algorithm.

### 4.1 Encoding Algorithm

Given a sorted set of entries with a known prefix length:

1. **Find common prefix**: Extend the prefix while all entries share the
   same character at that position.

2. **Write common prefix symbols**: Emit the `bps`-bit symbol code for
   each shared character.

3. **Check for terminal**: If any entry's key ends exactly at the common
   prefix, emit `END` or `END_VAL` (followed by a VarInt value index).

4. **Count children**: Group remaining entries by their distinguishing
   character at the branch position.

5. **Encode children**:
   - **0 children**: Done (leaf node).
   - **1 child, no terminal**: Continue linearly (no `BRANCH`). The
     child writes its own first character as a regular symbol.
   - **Multiple children** (or 1 child after terminal): Emit `BRANCH` +
     VarInt child count. For each child except the last, emit `SKIP` +
     VarInt skip distance (in bits). Then recursively encode each child.

### 4.2 Skip Distances

The `SKIP` control code is followed by a VarInt that encodes the **bit
distance** from the current position to the end of this child's subtree.
The decoder uses this to jump over non-matching children without
decoding them.

Skip distances are computed by a dry-run pass (`trie_subtree_size`)
before the actual write, ensuring exact values.

### 4.3 Worked Example

Encoding keys `{"abc", "abd", "xyz"}` with integer values `{10, 20, 30}`:

```
Sorted entries: abc=10, abd=20, xyz=30

Root: no common prefix ('a' vs 'x')
  → BRANCH 2

  Child 'a' (not last, needs SKIP):
    → SKIP <distance>
    Common prefix: "ab"
    → symbols 'a' 'b'
    Branch at position 2: 'c' and 'd'
    → BRANCH 2
      Child 'c': SKIP <dist>, 'c', END_VAL 0
      Child 'd': 'd', END_VAL 1

  Child 'x' (last, no SKIP):
    Common prefix: "xyz"
    → symbols 'x' 'y' 'z'
    → END_VAL 2
```

Stream: `BRANCH 2 SKIP <n> 'a' 'b' BRANCH 2 SKIP <m> 'c' END_VAL 0 'd' END_VAL 1 'x' 'y' 'z' END_VAL 2`

---

## 5. Value Store

When the `has_values` flag is set, the value store follows the prefix
trie. It contains typed values in **depth-first, left-to-right** order
(matching the order terminals appear during trie traversal).

Each value is encoded as:

```
4 bits     type tag
variable   payload (depends on type)
```

### 5.1 Value Types

| Tag | Type      | Payload                                    |
|-----|-----------|--------------------------------------------|
| 0   | `null`    | (none -- 0 bytes)                          |
| 1   | `bool`    | 1 bit (0 = false, 1 = true)                |
| 2   | `int`     | Signed VarInt (zigzag + LEB128)            |
| 3   | `uint`    | Unsigned VarInt (LEB128)                   |
| 4   | `float32` | 32 bits (IEEE 754 binary32, big-endian)    |
| 5   | `float64` | 64 bits (IEEE 754 binary64, big-endian)    |
| 6   | `string`  | VarInt length (bytes) + raw UTF-8 bytes    |
| 7   | `blob`    | VarInt length (bytes) + raw bytes          |
| 8   | `array`   | (reserved for future use)                  |
| 9   | `dict`    | (reserved -- nested TriePack dictionary)   |

### 5.2 Value Indexing

The `END_VAL` control code is followed by a VarInt value index. To
decode a value:

1. Seek to the start of the value store
2. Decode and skip `index` values sequentially
3. Decode the target value

String and blob values support **zero-copy** when the stream is
byte-aligned: the decoded value points directly into the source buffer.

---

## 6. CRC-32 Footer

The last 4 bytes of a `.trp` file contain a CRC-32 checksum in
big-endian byte order, computed over all preceding bytes (header + data
stream + alignment padding).

```
CRC-32 polynomial:  0xEDB88320 (reflected)
Initial value:      0xFFFFFFFF
Final XOR:          0xFFFFFFFF
```

`tp_dict_open()` verifies the CRC and returns `TP_ERR_CORRUPT` on
mismatch. `tp_dict_open_unchecked()` skips verification for trusted
data or performance-critical paths.

### Patching

The encoder builds the file in a single pass, writing a placeholder
header. After the data stream is complete:

1. Align to byte boundary
2. Compute CRC-32 over all bytes so far
3. Append 4-byte CRC
4. Patch header bytes 12-27 with final offsets
5. Recompute CRC-32 over patched data
6. Update the last 4 bytes with the corrected CRC

---

## 7. VarInt Encoding

See the [Bitstream Specification](bitstream-spec.md) for full details on
VarInt encoding (Sections 5 and 6). Summary:

- **Unsigned**: LEB128 -- 7 data bits + 1 continuation bit per byte
- **Signed**: Zigzag transform then LEB128

---

## 8. Encoding Pipeline

The full encoding pipeline in `tp_encoder_build()`:

```
 1. Sort entries lexicographically
 2. Deduplicate keys (last-added value wins)
 3. Detect if any values are non-null (set has_values flag)
 4. Analyze symbols: scan keys, compute bps, build symbol maps
 5. Write 32-byte header (placeholder offsets)
 6. Write trie config: bps, symbol_count, control codes, symbol table
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

## 9. Lookup Algorithm

To look up a key in a compiled dictionary:

```
1. Seek to trie_start (byte 32 + trie_data_offset)
2. key_idx = 0
3. While true:
   a. Read one bps-bit symbol
   b. If END:
        Key fully consumed? → found (null value)
        Otherwise: expect BRANCH next, or NOT_FOUND
   c. If END_VAL:
        Read VarInt value index
        Key fully consumed? → found (decode value)
        Otherwise: expect BRANCH next, or NOT_FOUND
   d. If BRANCH:
        Read VarInt child_count
        For each child (except last): read SKIP + VarInt distance
        Peek first symbol of each child
        If matches key[key_idx]: consume symbol, continue
        No match → NOT_FOUND
   e. If regular symbol:
        Compare to key[key_idx]
        Match → advance key_idx, continue
        Mismatch → NOT_FOUND
```

---

## 10. ROM Deployment

The `.trp` format supports direct ROM placement:

- All offsets are bit positions (absolute or relative to data start)
- No pointers in the binary format
- The decoder holds a `const uint8_t *` pointer with no mutation
- String/blob values use zero-copy into the source buffer
- No load-time fixups, relocations, or dynamic allocation beyond the
  `tp_dict` control structure (~100 bytes on stack or heap)
