---
layout: default
title: Home
---

# TriePack

**A compressed trie-based dictionary format for fast, compact key-value storage.**

TriePack encodes string-keyed dictionaries into a compact binary format
(`.trp`) that supports direct O(key-length) lookups without decompression.
It is designed for situations where you need a read-mostly dictionary that
is small, fast to query, and can live in ROM or flash memory with zero
load-time setup.

---

## The Problem

You have a dictionary -- string keys mapped to typed values -- and you need
to ship it in a compact form. Maybe it's a word list for spell-checking, a
configuration table baked into firmware, a set of HTTP status codes, or a
localization dictionary with thousands of translated strings.

Your options are limited:

| Approach | Size | Lookup | ROM-safe | Typed values |
|----------|------|--------|----------|--------------|
| JSON file | Large (text) | O(n) scan or parse-then-hash | No (needs parsing) | Strings only |
| SQLite | Moderate | O(log n) B-tree | No (needs filesystem) | Yes |
| Hash table | Moderate | O(1) amortized | No (pointer fixups) | Yes |
| Sorted array + binary search | Good | O(log n) | Yes | Manual |
| **TriePack** | **Excellent** | **O(key-length)** | **Yes** | **Yes** |

TriePack's prefix trie shares common prefixes across keys and packs symbols
at the bit level, producing encoded output that is typically **2-4x smaller**
than the raw key data alone. Lookups walk the trie in O(key-length) time
using skip pointers -- no hashing, no binary search, no full decompression.

---

## How It Works

TriePack uses a **compressed prefix trie** -- a tree structure where shared
key prefixes are stored once, and branches represent the points where keys
diverge.

### From Keys to Trie

Consider encoding three keys: `apple`, `application`, `banana`.

A naive approach stores each key separately (26 bytes total). A prefix trie
shares the common prefix `appl`:

```
root
├── appl
│   ├── e          → value for "apple"
│   └── ication   → value for "application"
└── banana         → value for "banana"
```

The shared prefix `appl` is stored once, saving 4 bytes. With thousands of
keys sharing common prefixes (URLs, file paths, English words), the savings
compound dramatically.

### Bit-Level Packing

TriePack doesn't store each character as a full byte. The encoder analyzes
all keys to find the unique alphabet, then assigns each character a
fixed-width code using the minimum number of bits:

```
Alphabet: {a, b, c, e, i, l, n, o, p, t} + 6 control codes = 16 symbols
Bits per symbol: 4 (because ceil(log2(16)) = 4)

Each character costs 4 bits instead of 8 -- a 2x savings on top of
the prefix sharing.
```

For English lowercase text (26 letters + 6 control codes = 32 symbols),
each symbol takes just **5 bits** instead of 8.

### Skip Pointers

The trie is serialized as a linear bit stream. At each branch point, the
encoder writes **skip pointers** -- VarInt-encoded bit distances that tell
the decoder exactly how far to jump to reach each child subtree. This
enables O(key-length) lookups without scanning:

```
To look up "banana":
1. Read first symbol → 'a' (branch: 'a' or 'b')
2. Key starts with 'b', not 'a' → use skip pointer to jump past 'a' subtree
3. Land at 'b' subtree → read "banana" → found
```

No character of any non-matching subtree is ever examined.

### Value Store

Values are stored separately from the trie, in a sequential value store.
Each value is tagged with a 4-bit type and encoded compactly:

| Type | Encoding | Typical size |
|------|----------|-------------|
| null | (tag only) | 4 bits |
| bool | tag + 1 bit | 5 bits |
| int | tag + zigzag VarInt | 5-76 bits |
| uint | tag + VarInt | 5-76 bits |
| float32 | tag + 32 bits | 36 bits |
| float64 | tag + 64 bits | 68 bits |
| string | tag + VarInt length + raw bytes | variable |
| blob | tag + VarInt length + raw bytes | variable |

Small integers (common in practice) take as little as 1-2 bytes. Strings
and blobs support **zero-copy** reads -- the decoded value points directly
into the source buffer, with no allocation.

---

## The Binary Format

A `.trp` file has three regions:

```
┌──────────────────────────────┐
│  Header (32 bytes, fixed)    │  Magic "TRP\0", version, flags,
│                              │  key count, section offsets
├──────────────────────────────┤
│  Data Stream (variable)      │
│  ┌────────────────────────┐  │
│  │ Trie Config            │  │  bits-per-symbol, symbol table,
│  │                        │  │  control code assignments
│  ├────────────────────────┤  │
│  │ Prefix Trie            │  │  compressed key structure with
│  │                        │  │  skip pointers and value indices
│  ├────────────────────────┤  │
│  │ Value Store            │  │  sequential typed values
│  │                        │  │  (only if has_values flag is set)
│  └────────────────────────┘  │
├──────────────────────────────┤
│  CRC-32 Footer (4 bytes)     │  integrity check over all
│                              │  preceding bytes
└──────────────────────────────┘
```

All multi-byte integers are big-endian. All offsets are in bits, relative
to the start of the data stream (byte 32). The format contains no pointers,
no relocations, and no alignment padding within the trie -- it can be placed
directly in ROM or flash memory.

For the full byte-level specification, see the
[Binary Format Specification](internals/format-spec.md).

---

## Compression Results

The `compaction_benchmark` example encodes ~10,000 generated English-like
words and measures compression:

```
Keys:              10000
Raw key bytes:     85432

Keys only:         ~32 KB encoded    (2.6x compression)
Keys + values:     ~45 KB encoded    (with uint values per key)
Lookup verified:   10000/10000 OK
```

Compression ratio depends on how much prefix sharing exists in the keys.
Word lists, URL paths, file paths, and dotted identifiers compress well.
Random binary keys compress poorly (but still benefit from bit packing).

---

## Design Goals

TriePack was designed with these priorities:

1. **Compactness** -- Minimize encoded size via prefix sharing and
   bit-level packing. Every bit counts on embedded devices with limited
   flash.

2. **Fast lookups** -- O(key-length) point queries via skip-pointer
   navigation. No decompression step, no index construction, no hash
   table building.

3. **ROM-safe** -- The encoded format contains no pointers. The decoder
   holds a `const uint8_t *` to the buffer and never mutates it. String
   and blob values point directly into the source buffer. The only heap
   allocation is the ~100-byte `tp_dict` control structure (which can
   also go on the stack).

4. **Typed values** -- Not just string-to-string. Support null, bool,
   integers (signed/unsigned, variable-length), IEEE 754 floats, strings,
   and binary blobs. Small integers use 1-2 bytes via VarInt encoding.

5. **Integrity** -- CRC-32 footer protects the entire file. Detected
   automatically on open, or skippable for trusted data.

6. **Portability** -- Pure C99, no extensions, no platform-specific code.
   Builds and passes all tests on 32-bit and 64-bit architectures. The
   same encoded file works on any platform.

---

## Library Architecture

TriePack is implemented as three layered C libraries, each usable
independently:

```
┌─────────────────────────────────────────┐
│  triepack_json                          │  JSON encode/decode overlay
│  Encodes JSON documents to .trp format  │  (optional, separate library)
│  using flattened dot-path keys          │
├─────────────────────────────────────────┤
│  triepack_core                          │  Trie codec
│  Encoder: insert keys → build .trp     │  Encoder, dictionary reader,
│  Dictionary: open .trp → lookup keys   │  iterator, CRC-32 integrity
├─────────────────────────────────────────┤
│  triepack_bitstream                     │  Bit-level I/O foundation
│  Read/write arbitrary-width fields      │  VarInt, symbols, UTF-8,
│  Zero-copy reader, growable writer      │  stateless ROM functions
└─────────────────────────────────────────┘
```

**triepack_bitstream** provides the core primitive: read and write integers
of arbitrary bit width (1-64 bits) packed into a contiguous byte buffer.
Everything else -- VarInts, symbols, the trie structure, values -- is built
on top of this single operation.

**triepack_core** implements the encoder (sort keys, analyze alphabet,
two-pass trie encoding, value serialization, CRC-32) and the dictionary
reader (header validation, trie navigation, value decoding).

**triepack_json** sits on top of the core library and provides JSON
encode/decode by flattening JSON objects into dot-path keys
(`address.city`, `items[0].name`) and using the core encoder under the
hood.

C++ RAII wrappers (`triepack_wrapper`) wrap all three layers with
`Encoder`, `Dict`, `Iterator`, and `BitstreamReader`/`Writer` classes
that handle lifetime management automatically.

---

## Quick Start

### Build

```bash
git clone https://github.com/deftio/triepack.git
cd triepack
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Encode and Query (C)

```c
#include "triepack/triepack.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* Create encoder and add entries */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v = tp_value_string("world");
    tp_encoder_add(enc, "hello", &v);

    v = tp_value_int(42);
    tp_encoder_add(enc, "count", &v);

    tp_encoder_add(enc, "flag", NULL);  /* key-only (set membership) */

    /* Build the compressed .trp blob */
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    /* Open and query */
    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    if (tp_dict_lookup(dict, "hello", &val) == TP_OK)
        printf("hello => %.*s\n",
               (int)val.data.string_val.str_len,
               val.data.string_val.str);

    if (tp_dict_lookup(dict, "count", &val) == TP_OK)
        printf("count => %lld\n", (long long)val.data.int_val);

    bool found = false;
    tp_dict_contains(dict, "flag", &found);
    printf("flag exists: %s\n", found ? "yes" : "no");

    /* Cleanup */
    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
    return 0;
}
```

### Encode and Query (C++)

```cpp
#include <triepack/triepack.hpp>

triepack::Encoder enc;
enc.insert("apple", 42);
enc.insert("banana", 17);

const uint8_t *data;
size_t size;
enc.encode(&data, &size);

triepack::Dict dict(data, size);
int32_t val;
if (dict.lookup("apple", &val))
    printf("apple => %d\n", val);  // 42
```

### JSON Round-Trip

```c
#include "triepack/triepack_json.h"

const char *json = "{\"name\":\"Alice\",\"age\":30,\"active\":true}";
uint8_t *buf = NULL;
size_t buf_len = 0;
tp_json_encode(json, strlen(json), &buf, &buf_len);

char *decoded = NULL;
size_t decoded_len = 0;
tp_json_decode_pretty(buf, buf_len, "  ", &decoded, &decoded_len);
printf("%.*s\n", (int)decoded_len, decoded);

free(decoded);
free(buf);
```

---

## Use Cases

**Embedded firmware** -- Bake a configuration dictionary or string table
into flash. The decoder reads directly from the `const` buffer with no
parsing step and no heap allocation beyond the ~100-byte control structure.

**Spell-checking / word lists** -- Encode a dictionary of valid words as
keys-only (no values). The trie provides O(word-length) membership testing
with excellent compression on natural-language word lists.

**Localization** -- Store translated strings as key-value pairs where keys
are dotted identifiers (`menu.file.open`) and values are translated text.
Prefix sharing on the dotted paths compresses well.

**Static configuration** -- Replace JSON/YAML config files with a
pre-compiled `.trp` blob. Faster to load (no parsing), smaller on disk,
and supports typed values natively.

**HTTP status codes, MIME types, country codes** -- Small lookup tables
that benefit from compact encoding and fast point queries.

**JSON compression** -- Encode JSON documents into `.trp` format for
compact storage. The JSON library flattens objects into dot-path keys
and reconstructs them on decode.

---

## Documentation

### Guides

- [Getting Started](guide/getting-started) -- install, build, tutorial walkthrough
- [How It Works](guide/how-it-works) -- trie encoding, lookup, bitstream primer
- [API Reference](guide/api-reference) -- every public function with usage examples
- [Examples](guide/examples) -- six runnable example programs
- [Building & Testing](guide/building) -- build options, cross-compilation, test suite

### Internals

- [Architecture](internals/architecture.md) -- library stack, encoding pipeline, two-trie design
- [Binary Format Specification](internals/format-spec.md) -- byte-level `.trp` file format
- [Bitstream Specification](internals/bitstream-spec.md) -- bit-level I/O, VarInt, UTF-8
- [Technical Deep Dive](triepack-technical-doc.md) -- encoding pipeline, algorithms, ROM deployment

### Project

- [GitHub Repository](https://github.com/deftio/triepack) -- source code, issues, pull requests
- [Releases](releases.md) -- release history and downloads
- [Release Process](guide/release-process.md) -- versioning policy and release checklist
- [Code Coverage](coverage/) -- line and branch coverage report

---

## License

BSD-2-Clause. Copyright (c) 2026 M. A. Chatterjee.
