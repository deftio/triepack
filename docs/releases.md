---
layout: default
title: Releases
---

# Releases

<!-- Copyright (c) 2026 M. A. Chatterjee -->

Download releases from the [GitHub Releases page](https://github.com/deftio/triepack/releases).

---

## v1.0.0 -- 2026-02-28

First stable release. All core libraries are implemented and tested.

### Bitstream Library (`triepack_bitstream`)

- Arbitrary-width bit field read/write (1-64 bits per field)
- MSB-first and LSB-first bit ordering
- Fixed-width byte reads: u8, u16, u32, u64 (big-endian)
- VarInt encoding: unsigned (LEB128) and signed (zigzag + LEB128)
- Fixed-width symbol read/write (configurable bits-per-symbol)
- UTF-8 codepoint read/write with full validation
- Stateless ROM functions (`_at` variants) for zero-allocation reads
- Growable writer with configurable buffer growth policy
- Zero-copy reader over `const` buffers (ROM-safe)
- Direct pointer access for zero-copy string/blob reads
- Bulk copy (reader-to-writer)
- 64-bit cursor addressing

### Core Trie Codec (`triepack_core`)

- Encoder: batch insert key-value pairs, build compressed trie
- Automatic symbol analysis (optimal bits-per-symbol selection)
- Two-pass trie encoding with skip pointers for O(key-length) lookups
- Full typed value system: null, bool, int, uint, float32, float64, string, blob
- Value construction helpers (`tp_value_int()`, `tp_value_string()`, etc.)
- Dictionary reader with CRC-32 integrity verification
- Unchecked open for trusted/ROM data
- Key lookup, existence check, key count, dictionary metadata
- 32-byte binary header with magic bytes (`TRP\0`), version, flags, offsets
- CRC-32 footer (reflected polynomial, matches zlib)

### JSON Library (`triepack_json`)

- One-shot JSON encode: parse JSON string, produce `.trp` blob
- One-shot JSON decode: reconstruct JSON from `.trp` blob
- Pretty-printed decode with configurable indentation
- DOM-style access: open, path lookup, root type, count
- Flattened dot-path key scheme (`a.b.c`, `items[0]`)
- Supports objects, arrays, strings, numbers, booleans, null
- Unicode escape handling (`\uXXXX`) with surrogate pair support

### C++ Wrappers (`triepack_wrapper`)

- `triepack::Encoder` -- RAII encoder with move semantics
- `triepack::Dict` -- RAII dictionary reader with move semantics
- `triepack::Iterator` -- RAII iterator with move semantics
- `triepack::BitstreamReader` / `BitstreamWriter` -- RAII bitstream wrappers
- `triepack::Json` -- RAII JSON DOM wrapper

### Build & CI

- CMake 3.16+ build system (C99 + C++11, no extensions)
- Strict warnings: `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow`
- Unity test framework (v2.6.0) via FetchContent
- GitHub Actions: Ubuntu GCC, Ubuntu Clang, macOS Clang, 32-bit
- clang-tidy linting, clang-format checking
- Code coverage (gcov/lcov), Doxygen docs, GitHub Pages

### Tests & Examples

- 16 test suites with full coverage of bitstream, core, JSON, and C++ wrappers
- 6 example programs registered as integration tests:
  `basic_encode_decode`, `compaction_benchmark`, `rom_lookup`,
  `prefix_search`, `json_roundtrip`, `cpp_usage`

### Documentation

- Getting started guide, build instructions, comprehensive API reference
- Binary format specification with worked examples
- Bitstream specification with cross-byte field diagrams
- Technical deep dive document
- Jekyll-based GitHub Pages site with Doxygen API docs

---

## v0.1.0 -- 2026-02-27

Initial project scaffolding.

- Directory structure, CMake build system, public API headers
- Stub implementations for all libraries
- CI/CD pipeline configuration (GitHub Actions)
- Language binding scaffolding (Python, TypeScript, JavaScript, Go, Swift, Rust)

---

## Installing a Release

### From Source

```bash
git clone https://github.com/deftio/triepack.git
cd triepack
git checkout v1.0.0
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Verifying

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Creating a Release

See the [Release Process](guide/release-process.md) guide.
