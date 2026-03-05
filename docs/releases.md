---
layout: default
title: Releases
---

# Releases

<!-- Copyright (c) 2026 M. A. Chatterjee -->

Download releases from the [GitHub Releases page](https://github.com/deftio/triepack/releases).

---

## v1.0.7 -- 2026-03-04

100% line coverage achieved across all three languages.

### Added
- **100% line coverage** in C/C++ (2,395 lines, 27 test programs), Python (590 statements,
  97 tests), and JavaScript (99 tests, 6 suites)
- New C test for keys-only dictionary lookup returning null value
- New JavaScript tests: values encode/decode, bitstream edge cases, crafted trie error paths
- New Python tests: encoder edge cases, decoder error paths, bitstream bounds checking

### Fixed
- **Dead code removed** from C, Python, and JavaScript encoders: unreachable start>=end guards,
  single-child branches (proven impossible by common-prefix loop invariant), and dedup loops
  (dict keys are unique)

### Changed
- LCOV_EXCL markers added to allocation failure paths in 14 C source files
- Updated testing docs, README project status, and test counts

### Test Totals
- C/C++: 27 test programs, 2,395 source lines at 100% coverage
- Python: 5 test files, 97 tests, 590 statements at 100% coverage
- JavaScript: 6 test suites, 99 tests, all files at 100% coverage

---

## v1.0.6 -- 2026-03-02

Bug fixes for CI stability, CLI conversion commands, and README improvements.

### Fixed
- **Encoder string value corruption** -- `tp_encoder_add_n()` now deep-copies string
  and blob data so the encoder owns it, preventing use-after-return from stack buffers
- **JSON decoder heap overflow** -- segment dedup was using entry index as byte offset
  into key strings, causing out-of-bounds reads
- **Bitstream zigzag UB** -- left-shift of negative `int64_t` replaced with unsigned cast

### Added
- **CLI conversion commands** -- `trp encode` (JSON to `.trp`), `trp decode` (`.trp` to
  JSON with `--pretty`), `trp validate` (integrity check); all accept `-` for stdin
- README badges (CI build, coverage, BSD-2-Clause license)
- README roadmap section with language binding and format enhancement milestones

### Changed
- CLI: removed `trp json` (replaced by `trp decode`)
- README: language bindings table updated (Python, JavaScript = implemented)

---

## v1.0.5 -- 2026-03-02

Python binding, expanded test coverage, complex JSON example, and test data files.

### Added
- **Native Python binding** -- pure-Python `.trp` encoder/decoder with byte-for-byte
  cross-language compatibility (70 tests, 5 test files)
- **Complex JSON example** (`json_complex.c`) -- nested objects, arrays, DOM lookups, pretty-print
- **Test data files** -- `common_words_10k.txt` (10K English words), `benchmark_100k.json` (202 KB synthetic catalog)
- **Benchmark generator** -- `tools/generate_benchmark_json.py`
- **3 new C test files** -- `test_json_decode.c` (27 tests), `test_core_internal.c` (21 tests), `test_bitstream_errors.c` (46 tests)
- Expanded 6 existing C test files with error-path and edge-case coverage
- Updated testing documentation with full test inventory

### Test Totals
- C/C++: 27 test programs, ~330 individual tests
- Python: 5 test files, 70 individual tests
- Grand total: ~400 tests across C, C++, and Python

---

## v1.0.4 -- 2026-03-02

Documentation improvement.

### Fixed
- Bitstream guide: clarify signed bit-field extraction with worked example

---

## v1.0.3 -- 2026-03-02

Site styling fixes.

### Fixed
- Move stylesheet to `assets/main.scss` so minima theme loads correctly
- Remove hardcoded top bar above navigation
- Add whitespace around section dividers
- Adjust version label size and color for readability

---

## v1.0.2 -- 2026-03-01

JavaScript binding, cross-language fixtures, CI and site fixes.

### Added
- **Native JavaScript binding** -- pure-JS `.trp` encoder/decoder
- **Cross-language fixture files** -- 7 `.trp` files for interop testing
  (`empty`, `single_null`, `single_int`, `multi_mixed`, `shared_prefix`, `large`, `keys_only`)
- **Cross-language test** (`test_cross_language.c`) validating fixture files from C, JavaScript, and Python

### Fixed
- 32-bit CI: enable Unity 64-bit type support, switch Pages to workflow-based build
- Site margins: use 75% viewport width, fix `!important` overrides
- clang-tidy: suppress `misc-no-recursion` false positive, fix dead store bug
- GitHub Pages: fix lcov coverage report overwriting the docs site, fix broken navigation links

---

## v1.0.1 -- 2026-03-01

GitHub Pages deployment fix.

### Fixed
- Docs site was showing lcov coverage report instead of the Jekyll documentation site
- Broken navigation links on the docs site

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
git checkout v1.0.7
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Verifying

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_JSON=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Creating a Release

See the [Release Process](guide/release-process.md) guide.
