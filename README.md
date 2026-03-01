# triepack v1.0.0

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

Native implementations (not FFI) are planned for:

| Language | Status | Directory |
|----------|--------|-----------|
| Python | Scaffolded | `bindings/python/` |
| TypeScript | Scaffolded | `bindings/typescript/` |
| JavaScript | Scaffolded | `bindings/javascript/` |
| Go | Scaffolded | `bindings/go/` |
| Swift | Scaffolded | `bindings/swift/` |
| Rust | Scaffolded | `bindings/rust/` |

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

**v1.0.0 released.** The bitstream library, trie codec, and JSON library are fully implemented. All 22 tests pass (16 unit test suites + 6 example integration tests). Run `compaction_benchmark` to see compression ratios on ~10k generated words.

Future work: suffix table, Huffman symbols, language bindings.

## License

BSD-2-Clause. See [LICENSE.txt](LICENSE.txt).

Copyright (c) 2026 M. A. Chatterjee
