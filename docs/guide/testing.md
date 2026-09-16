---
layout: default
title: Testing
---

# Testing

<!-- Copyright (c) 2026 M. A. Chatterjee -->

TriePack has comprehensive test suites across C, C++, Python, JavaScript, Go,
Rust, Swift, Kotlin, and Java, with cross-language fixture validation ensuring
binary compatibility. C/C++, Python, and JavaScript maintain **100% line coverage**.

## Running C/C++ Tests

Build with tests enabled (on by default), then run via CTest:

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_JSON=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To run a specific test by name:

```bash
ctest --test-dir build -R test_bitstream
```

## Running Python Tests

```bash
cd bindings/python
pip install -e ".[test]"
pytest -v
```

## Running JavaScript Tests

```bash
cd bindings/javascript
npm ci
npm test
```

## Test Summary

| Suite | Test Programs / Files | Total Tests | Line Coverage |
|-------|----------------------|-------------|---------------|
| C Bitstream | 6 | 210 | 100% |
| C Core | 7 | 179 | 100% |
| C JSON | 4 | 120 | 100% |
| C Cross-Language | 2 | 9 | 100% |
| C++ Wrappers | 3 | 47 | 100% |
| Examples (smoke) | 7 | 7 | — |
| **C/C++ Total** | **29** | **572** | **100%** |
| Python | 6 | 220 | 100% |
| JavaScript | 7 | 226 | 100% |
| Go | 3 | 164 | — |
| Rust | 3 | 82 | — |
| Swift | 2 | 34 | — |
| Kotlin | 3 | 52 | — |
| Java | 3 | 159 | — |
| **Grand Total** | **56** | **1,509** | — |

Counts include the cross-language conformance suite, which contributes cases
to every row. The C "Cross-Language" figure is small because
`test_conformance.c` runs the whole corpus inside two Unity tests rather than
one per case.

## C/C++ Test Organization

Tests live in `tests/` and use the [Unity](https://github.com/ThrowTheSwitch/Unity)
test framework (v2.6.0). They are organized by component.

### Bitstream Tests

| File | What it covers |
|------|----------------|
| `test_bitstream_bits.c` | Bit-level read/write, peek, seek, cursor management |
| `test_bitstream_bytes.c` | Byte-level u8/u16/u32/u64 read/write, NULL writer params |
| `test_bitstream_varint.c` | VarInt encode/decode (LEB128 unsigned, zigzag signed) |
| `test_bitstream_symbol.c` | Fixed-width symbol I/O, UTF-8 encode/decode, surrogate rejection |
| `test_bitstream_rom.c` | Stateless ROM-safe read functions (no allocation) |
| `test_bitstream_errors.c` | `tp_result_str` coverage, NULL params, EOF errors, seek/advance past end, destroy NULL, buffer access NULL |

### Core Tests

| File | What it covers |
|------|----------------|
| `test_core_roundtrip.c` | End-to-end encoder -> dict -> lookup cycle |
| `test_core_lookup.c` | Dictionary lookup edge cases |
| `test_core_values.c` | Typed value encode/decode for all 8 value types |
| `test_core_edge_cases.c` | Empty dicts, limits, corrupted CRC, suffix flag, encoder reset, contains/get_info |
| `test_core_integrity.c` | CRC-32 validation, corruption detection |
| `test_core_internal.c` | Internal `header.c` + `value.c`: NULL params, truncated buffer, header round-trip, value round-trip for all types |

### JSON Tests

| File | What it covers |
|------|----------------|
| `test_json_roundtrip.c` | JSON string -> .trp -> JSON string round-trips |
| `test_json_dom.c` | DOM open/close, path lookup, iteration, root type, corrupted buffer |
| `test_json_edge_cases.c` | Malformed JSON, depth limits, unicode, escape sequences, trailing input, truncated strings |
| `test_json_decode.c` | .trp -> JSON text reconstruction: flat/nested/array objects, escape chars, float32/64, uint, blob, pretty-print, large structures, truncated/corrupted buffers |

### Cross-Language Tests

| File | What it covers |
|------|----------------|
| `test_cross_language.c` | Validates the original C-generated `.trp` fixture files |
| `test_conformance.c` | Runs the shared conformance corpus, and checks that malformed buffers are rejected |

### C++ Wrapper Tests

| File | What it covers |
|------|----------------|
| `test_wrapper_bitstream.cpp` | C++ `BitstreamReader` / `BitstreamWriter` RAII wrappers |
| `test_wrapper_core.cpp` | C++ `Encoder` / `Dict` / `Iterator` wrappers, move semantics |
| `test_wrapper_json.cpp` | C++ `Json` wrapper and C API interop from C++ |

### Example Smoke Tests

Example programs are also registered as CTest targets to prevent regressions:

| Test Name | Example Program |
|-----------|-----------------|
| `example_basic_encode_decode` | `basic_encode_decode` |
| `example_compaction_benchmark` | `compaction_benchmark` |
| `example_rom_lookup` | `rom_lookup` |
| `example_prefix_search` | `prefix_search` |
| `example_cpp_usage` | `cpp_usage` |
| `example_json_roundtrip` | `json_roundtrip` |
| `example_json_complex` | `json_complex` |

## Python Test Organization

Python tests live in `bindings/python/tests/` and use [pytest](https://docs.pytest.org/).
The Python binding is a pure-Python native implementation (no FFI).

| File | Tests | What it covers |
|------|-------|----------------|
| `test_crc32.py` | 5 | CRC-32 known-answer tests including `"123456789"` -> `0xCBF43926` |
| `test_bitstream.py` | 19 | `BitWriter`/`BitReader` bit-level and byte-level operations, edge cases |
| `test_varint.py` | 16 | VarInt unsigned/signed round-trips, zigzag mapping, encoding sizes, overflow |
| `test_triepack.py` | 45 | Encode/decode round-trips: all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, error handling, edge cases, issue #1 regressions |
| `test_values.py` | 7 | Value encode/decode for all types: null, bool, int, uint, float32, float64, string, blob |
| `test_fixtures.py` | 14 | 7 decode tests + 7 **byte-for-byte** encode match against C-generated `.trp` fixture files |
| `test_conformance.py` | 113 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **220** | |

## JavaScript Test Organization

JavaScript tests live in `bindings/javascript/tests/` and use [Jest](https://jestjs.io/).
The JavaScript binding is a pure-JS native implementation (no FFI).

| File | Tests | What it covers |
|------|-------|----------------|
| `triepack.test.js` | 40 | Encode/decode round-trips: all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, version check, crafted trie error paths, issue #1 regressions |
| `bitstream.test.js` | 21 | `BitWriter`/`BitReader` bit-level and byte-level operations, u64, growth, EOF |
| `varint.test.js` | 13 | VarInt unsigned/signed round-trips, overflow, negative rejection, the exact-integer range |
| `values.test.js` | 7 | Value encode/decode: null, undefined, bool, float32, unknown tag |
| `crc32.test.js` | 8 | CRC-32 known-answer tests, empty input, incremental |
| `fixtures.test.js` | 24 | 7 decode + 7 encode match + 7 cross-read + 3 error tests |
| `conformance.test.js` | 113 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **226** | |

## Go Test Organization

Go tests live in `bindings/go/` and use Go's built-in `testing` package.

| File | Tests | What it covers |
|------|-------|----------------|
| `triepack_test.go` | ~29 | Encode/decode round-trips: all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, the alphabet limit |
| `fixtures_test.go` | ~14 | Decode + byte-identical encode match against C-generated `.trp` fixture files |
| `conformance_test.go` | ~121 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **164** | |

```bash
cd bindings/go
go test -v ./...
```

## Rust Test Organization

Rust tests live in `bindings/rust/` and use Rust's built-in test framework.

| File | Tests | What it covers |
|------|-------|----------------|
| `src/*.rs` (unit) | 44 | BitWriter/BitReader, CRC-32, VarInt, Values encode/decode |
| `tests/integration_test.rs` | 34 | Round-trips, shared prefixes, UTF-8, magic bytes, CRC corruption, fixture decode + encode match, issue #1 regressions |
| `tests/conformance_test.rs` | 4 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **82** | |

```bash
cd bindings/rust
cargo test
```

## Swift Test Organization

Swift tests live in `bindings/swift/Tests/` and use XCTest via Swift Package Manager.

| File | Tests | What it covers |
|------|-------|----------------|
| `TriepackTests.swift` | 30 | Round-trips for all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, fixture decode + encode match, issue #1 regressions |
| `ConformanceTests.swift` | 4 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **34** | |

```bash
cd bindings/swift
swift test
```

## Kotlin Test Organization

Kotlin tests live in `bindings/kotlin/src/test/` and use JUnit 5 via Gradle.

| File | Tests | What it covers |
|------|-------|----------------|
| `TriePackTest.kt` | ~31 | Encode/decode round-trips: all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, version check, issue #1 regressions |
| `FixturesTest.kt` | ~14 | Decode + byte-identical encode match against C-generated `.trp` fixture files |
| `ConformanceTest.kt` | 4 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **52** | |

```bash
cd bindings/kotlin
./gradlew test
```

## Java Test Organization

Java tests live in `bindings/java/src/test/` and use JUnit 5 via Gradle.

| File | Tests | What it covers |
|------|-------|----------------|
| `TriePackTest.java` | ~34 | Encode/decode round-trips: all value types, shared prefixes, UTF-8 keys, magic bytes, CRC corruption, version check, issue #1 regressions |
| `FixturesTest.java` | ~14 | Decode + byte-identical encode match against C-generated `.trp` fixture files |
| `ConformanceTest.java` | 111 | The shared conformance corpus, and rejection of malformed buffers |
| **Total** | **159** | |

```bash
cd bindings/java
./gradlew test
```

### Cross-Language Conformance Suite

`tests/conformance/` holds one case list that every implementation runs, so
the C library and the eight bindings cannot drift apart without a test
noticing. Each implementation has a harness that reads `cases.txt` and, for
each of the 50 cases:

- **Decode**: read `fixtures/<name>.trp` and check the values against the corpus
- **Encode**: encode the corpus values and check the bytes against the same file

Byte-for-byte encoding is the strong claim: any implementation can produce a
`.trp` another can read, and two implementations given the same input produce
the same file.

`malformed/` holds 11 buffers with one field damaged each — bad magic, wrong
version, broken CRC, truncation, and out-of-range trie configs. Every
implementation asserts that all of them are rejected.

The corpus and its fixtures are generated; CI fails if the checked-in files
are not what the generators produce. See
[`tests/conformance/README.md`](https://github.com/deftio/triepack/blob/main/tests/conformance/README.md)
for the file format and how to add a case.

### Original Fixture Set

`tests/fixtures/` predates the conformance suite and covers seven of the same
cases; the two are byte-identical where they overlap, and a test would fail if
they stopped being so.

| Fixture | Description |
|---------|-------------|
| `empty.trp` | Empty dictionary (zero keys) |
| `single_null.trp` | `{"hello": null}` |
| `single_int.trp` | `{"key": 42}` |
| `multi_mixed.trp` | Five keys: bool, float64, int, string, uint |
| `shared_prefix.trp` | `{"abc": 10, "abd": 20, "xyz": 30}` |
| `large.trp` | 100 keys: `key_0000` through `key_0099` |
| `keys_only.trp` | `{"apple": null, "banana": null, "cherry": null}` |

## Test Data Files

| File | Size | Purpose |
|------|------|---------|
| [`tests/data/common_words_10k.txt`](https://github.com/deftio/triepack/blob/main/tests/data/common_words_10k.txt) | 82 KB | 10,000 unique English words for trie compression benchmarks |
| [`tests/data/benchmark_100k.json`](https://github.com/deftio/triepack/blob/main/tests/data/benchmark_100k.json) | 202 KB | Synthetic product catalog (200 items, nested JSON, all types) |

## Adding a New Test

1. Create a new source file in `tests/`, e.g. `tests/test_feature.c`.
2. Register it in `tests/CMakeLists.txt` using the helper function:
   ```cmake
   triepack_add_c_test(test_feature test_feature.c triepack_core)
   ```
   For C++ tests:
   ```cmake
   triepack_add_cpp_test(test_feature test_feature.cpp triepack_wrapper)
   ```
3. Run `ctest` to verify it passes.

## Code Coverage

Enable coverage instrumentation and generate an HTML report:

```bash
cmake -B build -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
cmake --build build --target coverage
```

The HTML report is written to `build/coverage/index.html`.

Coverage uses `gcov` and `lcov` (or `llvm-cov` on macOS). Install them if
not already available:

```bash
# Linux
sudo apt-get install lcov

# macOS
brew install lcov
```

## Coverage Target

The project maintains **100% line coverage** for C/C++, Python, and
JavaScript:

| Language | Lines/Statements | Coverage |
|----------|-----------------|----------|
| C/C++ | 2,395 lines | 100% |
| Python | 590 statements | 100% |
| JavaScript | all files | 100% |
| Go | — | not measured |
| Rust | — | not measured |
| Swift | — | not measured |
| Kotlin | — | not measured |
| Java | — | not measured |

Allocation failure paths (malloc/realloc returning NULL) are excluded from
coverage measurement via `LCOV_EXCL` markers since they require custom
allocator injection to test. Coverage gaps in example programs and
platform-specific fallbacks are acceptable.
