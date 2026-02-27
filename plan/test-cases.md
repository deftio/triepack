# TXZ 1.0 Test Cases

**100% test coverage is mandatory** across all three libraries. Measured with `gcov`/`lcov`.

```
Library stack:              Test coverage required:
+--------------+
| txz-json     |  (separate library, uses txz)    100%
+--------------+
| txz          |  (core: trie codec)              100%
+--------------+
| txz-bitstream|  (foundation: bit/byte/sym I/O)  100%
+--------------+
```

All libraries are C with C++ wrappers. Tests are written in C (or C++ where convenient for test fixtures).

---

## 1. Encoding and Round-Trip Tests

The fundamental test: encode a data structure into TXZ binary, decode it, and verify the result matches the input exactly. Key trie and value trie/store round-trip independently and together.

### 1.1 Flat Key-Value Dictionary

```json
{
  "apple": 1,
  "application": 2,
  "apply": 3,
  "banana": 4,
  "band": 5,
  "bandana": 6
}
```

**Verify:**
- All 6 keys are retrievable with correct values
- Prefix sharing: "appl" stored once (covers apple, application, apply), "ban" stored once (covers banana, band, bandana)
- Encoded size < naive key-value storage (measure compression ratio)
- Keys not in the dict return NOT_FOUND

### 1.2 Nested Dictionary (DOM)

```json
{
  "server": {
    "host": "localhost",
    "port": 8080,
    "tls": {
      "enabled": true,
      "cert_path": "/etc/ssl/cert.pem",
      "key_path": "/etc/ssl/key.pem"
    }
  },
  "database": {
    "host": "localhost",
    "port": 5432,
    "name": "mydb"
  }
}
```

**Verify:**
- Nested dicts decode correctly at each level
- Value types preserved: strings, integers, booleans
- Lookup by path: e.g. lookup("server") returns a dict, then lookup("tls") within that returns another dict, then lookup("enabled") returns true
- Shared key names across nesting levels ("host", "port" appear in both "server" and "database") -- each nested dict is independent but the encoder could share suffix structures

### 1.3 String Arrays

```json
{
  "words": ["wonder", "wondering", "womb", "wombat", "world", "worry", "worrying", "worries"],
  "colors": ["red", "green", "blue", "green-blue", "blue-green"]
}
```

**Verify:**
- Arrays decode with correct order and values
- "words" array: the 8 strings sharing prefix "wo" should compress well via the prefix/suffix trie mechanism within the array encoding
- "colors" array: "green", "blue", "green-blue", "blue-green" share substrings
- Round-trip: decoded arrays match input exactly, in order

### 1.4 Full JSON Document

```json
{
  "name": "TXZ Test Suite",
  "version": 1,
  "enabled": true,
  "score": 98.6,
  "tags": ["compression", "trie", "binary"],
  "metadata": null,
  "nested": {
    "level1": {
      "level2": {
        "level3": {
          "deep_value": "found it"
        }
      }
    }
  },
  "empty_object": {},
  "empty_array": [],
  "numbers": [0, -1, 127, 128, -128, 16383, 16384, 2147483647]
}
```

**Verify:**
- Every JSON type represented: string, int, float, bool, null, object, array
- Deep nesting (3+ levels) works
- Edge case values: empty object, empty array, VarInt boundary values (127/128, 16383/16384), negative integers, max 32-bit int
- Null preserved (not confused with key-not-found)
- Float round-trips exactly (98.6 as float64)

### 1.5 Large Word List (Compression Benchmark)

Input: /usr/share/dict/words (or similar, ~100K-250K English words), key-only (no values).

**Verify:**
- All words retrievable
- Compression ratio vs. newline-separated text file
- Compression ratio vs. gzip of the same text
- Suffix extraction measurably reduces size (compare with suffix table disabled)
- Lookup time: O(key-length), not O(total-keys)

### 1.6 UTF-8 Keys

```json
{
  "cafe": "english",
  "caf\u00e9": "french",
  "\u6771\u4eac": "Tokyo",
  "\u6771\u4eac\u90fd": "Tokyo-to",
  "\u6771\u5317": "Tohoku",
  "\ud83d\ude00": "grinning face"
}
```

**Verify:**
- Multi-byte UTF-8 keys stored and retrieved correctly
- Prefix sharing works across UTF-8: "東京" and "東京都" share prefix, "東京" and "東北" share "東"
- 4-byte UTF-8 (emoji) works
- ASCII and non-ASCII keys coexist in same trie

---

## 2. Boundary and Edge Cases

### 2.1 Empty Dictionary
- Encode `{}`, decode, verify empty
- Lookup any key returns NOT_FOUND

### 2.2 Single Entry
- Encode `{"a": 1}`, decode, verify
- Minimal trie: one stem, one terminal

### 2.3 Single-Character Keys
- `{"a": 1, "b": 2, "c": 3}` -- no prefix sharing possible, all branch at root

### 2.4 Very Long Keys
- Key of 1000+ characters -- verify no overflow in skip pointers or VarInt encoding
- Key that exceeds typical stack depth for branch tracking

### 2.5 Duplicate Prefixes with Different Value Types
```json
{
  "config": {"nested": true},
  "config_value": 42,
  "config_list": [1, 2, 3]
}
```
- "config" is both a complete key (value: dict) and a prefix of other keys
- Verify all three keys resolve to correct types

### 2.6 Binary Blob Values
```json
{
  "icon_small": "<16 bytes raw>",
  "icon_large": "<4096 bytes raw>"
}
```
- Blob values (non-UTF8) stored and retrieved correctly
- Large blob doesn't break value store addressing

### 2.7 Keys That Collide with Control Codes
- Keys containing characters that map to the same symbol codes as END, SKIP, etc.
- Verify ESCAPE mechanism works correctly
- Especially in byte mode and symbol-UTF8 mode

---

## 3. Addressing Mode Tests

### 3.1 Bit Mode
- Encode the word list from 1.1 with 5-bit symbols (a-z + 6 control codes)
- Verify trie data is bit-packed (not byte-aligned)
- Measure: should be the smallest encoding

### 3.2 Byte Mode
- Same data encoded in byte mode
- Verify all symbols byte-aligned
- Measure: larger than bit mode but faster to decode

### 3.3 Symbol-UTF8 Mode
- Encode UTF-8 test case (1.6) in symbol-UTF8 mode
- Verify code points are the trie symbols, not individual bytes
- "東京" should be 2 symbols, not 6 bytes

### 3.4 Cross-Mode: Bit Trie + Byte Values
- Trie in bit mode, value store in byte mode
- Verify both sections decode correctly from the same stream

---

## 4. Integrity Tests

### 4.1 Checksum Validation
- Encode, verify checksum matches on decode
- Flip one bit in the encoded data, verify checksum fails

### 4.2 Truncation Detection
- Truncate encoded data at various points, verify graceful error (not crash)

### 4.3 Header Corruption
- Corrupt magic bytes, version, flags -- verify rejected

---

## 5. Compression Quality

### 5.1 Prefix Sharing Measurement
- Encode a sorted word list
- Compare encoded size with and without prefix trie (flat list baseline)
- Report: bits per key, bytes per key

### 5.2 Suffix Extraction Measurement
- Same word list with suffix table enabled vs. disabled
- Report additional compression from suffix dedup

### 5.3 Comparison Benchmarks
- Same data encoded as: TXZ, JSON (text), JSON (minified), MessagePack, CBOR, gzip'd JSON
- Report sizes
- Note: TXZ should win on key-heavy data with shared prefixes. It won't win on large-blob-heavy data.

---

## 6. Fuzzy Match (Exploratory -- See Notes)

See `plan/fuzzy-match-notes.md` for analysis of what's feasible.

### 6.1 Prefix Search
- Given prefix "wor", return all keys starting with "wor"
- This is natural for a trie: walk to the "wor" node, enumerate all descendants

### 6.2 Bounded Edit Distance
- Search for "wrold" (edit distance 1 from "world"), expect "world" in results
- Search for "banan" (edit distance 1 from "banana"), expect "banana" in results
- Max edit distance = 1, 2

### 6.3 Top-N Results
- Search for "aply" with max_results=5, expect: "apply", "aptly", ... ranked by edit distance

### 6.4 Performance Bounds
- Fuzzy search on 100K word list with edit distance 1: measure time
- Fuzzy search with edit distance 2: measure time (expect significantly slower)
- Establish: is this practical or does it need a separate index?
