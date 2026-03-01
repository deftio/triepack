---
layout: default
title: Examples
---

# Examples

<!-- Copyright (c) 2026 M. A. Chatterjee -->

Example programs are built when `BUILD_EXAMPLES=ON` (the default). After
building, the executables are in `build/examples/`.

## Building the Examples

```bash
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_JSON=ON
cmake --build build
```

All examples are also registered as CTest targets. Run them with:

```bash
ctest --test-dir build -R example_
```

---

## Example Programs

### `basic_encode_decode`

**What it demonstrates:** Encodes a small set of key-value pairs into a
triepack blob, then reads them back with the dictionary API. Shows
integer values, string values, and key-only (set membership) entries.

```bash
./build/examples/basic_encode_decode
```

Expected output:

```
Entries added: 6
Encoded blob: XX bytes

Dictionary contains 6 keys

apple  -> 42
banana -> 17
cherry -> 8
date   -> "hello"
elderberry exists: yes
missing exists: no

Done.
```

---

### `compaction_benchmark`

**What it demonstrates:** Generates ~10,000 unique words programmatically
(base words + prefixes + suffixes) and measures TriePack compression ratio.
Encodes both keys-only and keys+values variants. Verifies all lookups.

```bash
./build/examples/compaction_benchmark
```

Example output:

```
TriePack Compaction Benchmark
─────────────────────────────
Keys:              10000
Raw key bytes:     85432

=== Keys Only (no values) ===
Bits per symbol:   5
Encoded blob:      XXXXX bytes
Compression ratio: X.XXx
Bits per key:      XX.X
Lookup verified:   10000/10000 OK

=== Keys + uint values ===
Encoded blob:      XXXXX bytes
Compression ratio: X.XXx
Bits per key:      XX.X
Lookup verified:   10000/10000 OK

Done.
```

---

### `rom_lookup`

**What it demonstrates:** Encodes a dictionary of HTTP status codes, then
opens it with `tp_dict_open_unchecked()` to demonstrate ROM-style access
with zero integrity overhead. Performs lookups and shows dictionary
metadata via `tp_dict_get_info()`.

```bash
./build/examples/rom_lookup
```

Expected output:

```
ROM Lookup Example
──────────────────
Encoded 5 HTTP status codes into XX bytes

Lookup results (unchecked open):
  200 -> OK
  404 -> Not Found
  500 -> Internal Server Error
  999 -> NOT FOUND (expected)

Dictionary info:
  Format version: 1.0
  Keys: 5
  Bits per symbol: X
  Total bytes: XX

Done.
```

---

### `prefix_search`

**What it demonstrates:** Encodes a word list with shared prefixes (words
starting with "app", "ban", "car", etc.), then uses `tp_dict_contains()`
to check membership and `tp_dict_get_info()` to display encoding metadata.

```bash
./build/examples/prefix_search
```

Expected output:

```
Prefix Search Example
─────────────────────
Encoded 12 words into XX bytes

Membership checks:
  apple:   found
  apply:   found
  banana:  found
  missing: NOT found

Dictionary info:
  Keys: 12
  Bits per symbol: X
  Has values: no

Done.
```

---

### `json_roundtrip`

**What it demonstrates:** Encodes a JSON object into the `.trp` binary
format, decodes it back to JSON (both compact and pretty-printed), and
performs DOM-style path lookups on the encoded data.

Requires `BUILD_JSON=ON`.

```bash
./build/examples/json_roundtrip
```

Expected output:

```
JSON Round-Trip Example
───────────────────────
Original JSON (XX bytes):
  {"name":"TriePack","version":1,"active":true}

Encoded to XX bytes (.trp)
Compression ratio: X.Xx

Decoded (compact):
  {"active":true,"name":"TriePack","version":1}

Decoded (pretty):
  {
    "active": true,
    "name": "TriePack",
    "version": 1
  }

DOM path lookups:
  name -> "TriePack"
  version -> 1

Done.
```

---

### `cpp_usage`

**What it demonstrates:** Uses the C++ RAII wrappers (`triepack::Encoder`,
`triepack::Dict`) to encode key-value pairs and look them up. Shows
move semantics and the full encode/decode lifecycle in C++.

```bash
./build/examples/cpp_usage
```

Expected output:

```
C++ Usage Example
─────────────────
Encoded 3 entries into XX bytes

Lookups:
  apple  -> 42
  banana -> 17
  cherry -> 99

Dict size: 3

Done.
```

---

## Writing Your Own

Link against the appropriate library target:

```cmake
add_executable(my_example my_example.c)
target_link_libraries(my_example PRIVATE triepack_core)
```

For JSON examples, link `triepack_json`. For C++ examples, link
`triepack_wrapper`:

```cmake
add_executable(my_cpp_example my_example.cpp)
target_link_libraries(my_cpp_example PRIVATE triepack_wrapper)
```
