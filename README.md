# triepack v1.0.0

[![CI Build & Test](https://github.com/deftio/triepack/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/triepack/actions/workflows/ci.yml)
[![Coverage Report](https://github.com/deftio/triepack/actions/workflows/coverage.yml/badge.svg)](https://github.com/deftio/triepack/actions/workflows/coverage.yml)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE.txt)

A compressed trie-based dictionary format for fast, compact key-value storage.

TriePack encodes dictionaries into a compact binary format (`.trp`) optimized for fast lookups, prefix search, and ROM-safe deployment. It uses prefix sharing and bit-level packing with configurable symbol encoding and full value type support.

## Features

- **Compact binary format** — compressed tries with suffix sharing
- **Fast lookups** — O(key-length) point queries
- **Prefix search** — iterate all keys matching a prefix
- **Fuzzy search** — find keys within edit distance d<=2
- **ROM-safe** — readers work directly on `const` buffers with zero allocation
- **Typed values** — null, bool, int, uint, float, double, string, blob, array, nested dict
- **JSON support** — encode/decode JSON documents to/from `.trp` format
- **C99 core** — portable C library with C++11 wrappers
- **32-bit and 64-bit** — works on both architectures

## Quick Start

```bash
# Build
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### C API

```c
#include "triepack/triepack.h"
#include <stdio.h>
#include <stdlib.h>

/* Encode */
tp_encoder *enc = NULL;
tp_encoder_create(&enc);

tp_value v = tp_value_int(42);
tp_encoder_add(enc, "hello", &v);

v = tp_value_string("foo");
tp_encoder_add(enc, "world", &v);

uint8_t *buf = NULL;
size_t len = 0;
tp_encoder_build(enc, &buf, &len);

/* Decode */
tp_dict *dict = NULL;
tp_dict_open(&dict, buf, len);

tp_value val;
if (tp_dict_lookup(dict, "hello", &val) == TP_OK)
    printf("hello -> %lld\n", (long long)val.data.int_val);

tp_dict_close(&dict);
tp_encoder_destroy(&enc);
free(buf);
```

### C++ API

```cpp
#include <triepack/triepack.hpp>

triepack::Encoder enc;
// ... add entries, build, lookup via triepack::Dict
```

## Library Stack

```
triepack_json          (JSON encode/decode)
    |
triepack_core          (trie codec: encoder, dictionary, iterator)
    |
triepack_bitstream     (bit-level I/O, VarInt, UTF-8)
```

Each layer can be used independently. `triepack_wrapper` provides C++11 RAII wrappers over all three.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build test suite |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `BUILD_JSON` | ON | Build JSON library |
| `BUILD_DOCS` | OFF | Build Doxygen documentation |
| `ENABLE_COVERAGE` | OFF | Enable code coverage instrumentation |

## Language Bindings

All bindings are native implementations that read/write the `.trp` format directly (no FFI).

| Language | Status | Directory |
|----------|--------|-----------|
| Python | Implemented | `bindings/python/` |
| JavaScript | Implemented | `bindings/javascript/` |
| TypeScript | Not yet implemented | `bindings/typescript/` |
| Go | Not yet implemented | `bindings/go/` |
| Swift | Not yet implemented | `bindings/swift/` |
| Rust | Not yet implemented | `bindings/rust/` |

## File Format

- Magic bytes: `TRP\0` (`0x54 0x52 0x50 0x00`)
- File extension: `.trp`
- 32-byte fixed header
- Two-trie architecture with configurable addressing modes
- CRC32/SHA256/xxHash64 integrity checking

See `docs/internals/` for format details.

## Documentation

- [Getting Started](docs/guide/getting-started.md)
- [Building](docs/guide/building.md)
- [API Reference](docs/guide/api-reference.md)
- [Examples](docs/guide/examples.md)
- [Testing](docs/guide/testing.md)
- [Release Process](docs/guide/release-process.md)

## Project Status

**v1.0.5 released.** Core C library (bitstream, trie codec, JSON), C++ wrappers, Python binding, and JavaScript binding are implemented. 27 test programs with ~400 individual tests across C, C++, and Python.

## Roadmap

### v1.1 — Client Libraries
- [ ] TypeScript binding (native `.trp` reader/writer)
- [ ] Go binding
- [ ] Swift binding (with SPM package)
- [ ] Rust binding (with crate on crates.io)
- [ ] npm package for JavaScript/TypeScript
- [ ] PyPI package for Python

### v1.2 — Format Enhancements
- [ ] Suffix table (shared ending compression)
- [ ] Huffman symbol encoding (for large dictionaries)
- [ ] Nested dict values (embed sub-dictionaries inline)

### v1.3 — Tooling & Ecosystem
- [ ] `trp` CLI: encode/decode/validate/inspect (in progress)
- [ ] Fuzzy search (edit distance d<=2)
- [ ] Performance benchmarks across languages
- [ ] Language binding conformance test suite

## License

BSD-2-Clause. See [LICENSE.txt](LICENSE.txt).

Copyright (c) 2026 M. A. Chatterjee
