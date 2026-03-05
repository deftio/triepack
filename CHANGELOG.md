# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.7] - 2026-03-04

### Added
- 100% line coverage across all three languages (C/C++, Python, JavaScript)
- C/C++: 2,395 lines covered across 16 source files, 27 test programs
- Python: 590 statements covered, 97 tests across 5 test files
- JavaScript: 99 tests across 6 test suites, all files at 100%
- New C test: keys-only dictionary lookup returns null value
- New JS tests: values encode/decode, bitstream edge cases, crafted trie
  error paths, trie prefix/branch coverage
- New Python tests: encoder edge cases, decoder error paths, bitstream
  bounds checking, varint overflow

### Fixed
- Encoder: removed 4 unreachable code paths (start>=end guards and
  single-child branches proven impossible by common-prefix loop invariant)
- Python encoder: removed unreachable dedup loop (dict keys are unique)
  and single-child branch (same invariant as C)
- JavaScript encoder: removed unreachable dedup loop and single-child branch

### Changed
- Added LCOV_EXCL markers to allocation failure paths across 14 C source
  files (malloc/realloc NULL returns require custom allocator injection to
  test, excluded from coverage measurement)
- Updated testing documentation with JavaScript test inventory
- Updated README project status and test counts

## [1.0.6] - 2026-03-02

### Fixed
- Encoder: deep-copy string/blob value data to prevent use-after-return when
  JSON encoder passes stack-allocated buffers (root cause of CI flaky test)
- JSON decoder: fix heap-buffer-overflow in segment dedup that used entry index
  as byte offset into key string
- Bitstream: fix undefined behavior in zigzag encode (left-shift of negative
  value); cast to uint64_t before shifting

### Added
- README: CI build, coverage, and BSD-2-Clause license badges
- README: roadmap section with planned milestones for language bindings,
  format enhancements, and tooling
- CLI tool: `trp encode`, `trp decode`, `trp validate` commands with stdin
  support and `-o`/`--pretty` flags

### Changed
- README: updated language bindings table (Python, JavaScript now implemented)
- README: updated project status to v1.0.5 test counts
- CLI tool: removed `trp json` command (replaced by `trp decode`)

## [1.0.5] - 2026-03-02

### Added
- Native Python binding: pure-Python `.trp` encoder/decoder with byte-for-byte compatibility
- Python test suite: 70 tests across 5 files (crc32, bitstream, varint, roundtrip, fixtures)
- Complex JSON example (`json_complex.c`): nested objects, arrays, DOM lookups, pretty-print
- Test data files: `common_words_10k.txt` (10K words) and `benchmark_100k.json` (202 KB)
- Generator script: `tools/generate_benchmark_json.py`
- 3 new C test files: `test_json_decode.c`, `test_core_internal.c`, `test_bitstream_errors.c`
- Expanded existing test files with error-path and edge-case coverage

### Changed
- C test suite: 16 test programs -> 20 test programs, ~330 individual tests
- Total tests across all languages: ~400 (C/C++ + Python)
- Updated testing documentation (`docs/guide/testing.md`)
- Updated bindings README: Python and JavaScript marked as implemented

## [1.0.4] - 2026-03-02

### Fixed
- Bitstream guide: clarify signed bit-field extraction with worked example

## [1.0.3] - 2026-03-02

### Fixed
- CSS: move stylesheet to `assets/main.scss` so minima theme loads correctly
- Remove hardcoded top bar above nav, add whitespace around section dividers
- Adjust version label size and color for readability

## [1.0.2] - 2026-03-01

### Added
- Native JavaScript `.trp` implementation: pure-JS encoder/decoder
- Cross-language fixture files (7 `.trp` files) for interop testing
- Cross-language test (`test_cross_language.c`) validating fixture files

### Fixed
- 32-bit CI: enable Unity 64-bit type support, switch Pages to workflow build
- Site margins: use 75% viewport width, fix `!important` overrides
- clang-tidy: suppress `misc-no-recursion` false positive, fix dead store bug
- GitHub Pages: fix lcov report overwriting docs site, fix broken links

## [1.0.1] - 2026-03-01

### Fixed
- GitHub Pages deployment: docs site was showing lcov coverage report instead of Jekyll site
- Fix broken navigation links on docs site

## [1.0.0] - 2026-02-28

### Added

**Bitstream Library (`triepack_bitstream`)**
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

**Core Trie Codec (`triepack_core`)**
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

**JSON Library (`triepack_json`)**
- One-shot JSON encode: parse JSON string, produce `.trp` blob
- One-shot JSON decode: reconstruct JSON from `.trp` blob
- Pretty-printed decode with configurable indentation
- DOM-style access: open, path lookup, root type, count
- Flattened dot-path key scheme (`a.b.c`, `items[0]`)
- Supports objects, arrays, strings, numbers, booleans, null
- Unicode escape handling (`\uXXXX`) with surrogate pair support

**C++ Wrappers (`triepack_wrapper`)**
- `triepack::Encoder` -- RAII encoder with move semantics
- `triepack::Dict` -- RAII dictionary reader with move semantics
- `triepack::Iterator` -- RAII iterator with move semantics
- `triepack::BitstreamReader` / `BitstreamWriter` -- RAII bitstream wrappers
- `triepack::Json` -- RAII JSON DOM wrapper

**Build System**
- CMake 3.16+ build system
- C99 (no extensions) + C++11 (no extensions) enforced
- Strict compiler warnings: -Wall -Wextra -Wpedantic -Werror
- Unity test framework (v2.6.0) via FetchContent
- Code coverage support (gcov/lcov)
- Doxygen documentation generation

**CI/CD**
- GitHub Actions: Ubuntu GCC, Ubuntu Clang, macOS Clang, 32-bit
- clang-tidy linting and clang-format style checking
- Coverage report generation and GitHub Pages deploy

**Tests**
- 16 test suites covering bitstream, core, JSON, and C++ wrappers
- 6 example programs registered as integration tests

**Examples**
- `basic_encode_decode` -- encode/decode with multiple value types
- `compaction_benchmark` -- 10k-word compression ratio measurement
- `rom_lookup` -- ROM-style zero-allocation dictionary access
- `prefix_search` -- membership checking with shared-prefix keys
- `json_roundtrip` -- JSON encode/decode/DOM round-trip
- `cpp_usage` -- C++ RAII wrapper demonstration

**Documentation**
- Getting started guide, build instructions, API reference
- Binary format specification with worked examples
- Bitstream specification with cross-byte field diagrams
- Technical deep dive document
- Jekyll-based GitHub Pages site

## [0.1.0] - 2026-02-27

### Added
- Initial project scaffolding
- Directory structure, CMake build system, public API headers
- Stub implementations for all libraries
- CI/CD pipeline configuration
- Language binding scaffolding (Python, TypeScript, JavaScript, Go, Swift, Rust)
