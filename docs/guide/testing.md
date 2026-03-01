---
layout: default
title: Testing
---

# Testing

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Running Tests

Build with tests enabled (on by default), then run via CTest:

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To run a specific test by name:

```bash
ctest --test-dir build -R test_bitstream
```

## Test Organization

Tests live in `tests/` and are organized by component:

| File                          | Component tested                     |
|-------------------------------|--------------------------------------|
| `test_bitstream_bits.c`       | Bit-level read/write, peek, seek     |
| `test_bitstream_bytes.c`      | Byte-level u8/u16/u32/u64 read/write |
| `test_bitstream_varint.c`     | VarInt encode/decode (LEB128/zigzag) |
| `test_bitstream_symbol.c`     | Fixed-width symbol and UTF-8 I/O     |
| `test_bitstream_rom.c`        | Stateless ROM read functions         |
| `test_core_roundtrip.c`       | Encoder -> dict -> lookup cycle      |
| `test_core_lookup.c`          | Dictionary lookup edge cases         |
| `test_core_values.c`          | Typed value encode/decode            |
| `test_core_edge_cases.c`      | Empty dicts, limits, error paths     |
| `test_core_integrity.c`       | CRC-32 validation, corruption detect |
| `test_json_roundtrip.c`       | JSON encode/decode round-trips       |
| `test_json_dom.c`             | DOM open/close, path lookup          |
| `test_json_edge_cases.c`      | Malformed JSON, depth limits, unicode|
| `test_wrapper_bitstream.cpp`  | C++ BitstreamReader/Writer wrappers  |
| `test_wrapper_core.cpp`       | C++ Encoder/Dict/Iterator wrappers   |
| `test_wrapper_json.cpp`       | C++ JSON wrapper and C API from C++  |

Example programs are also registered as tests:

| Test Name                      | Example Program         |
|--------------------------------|-------------------------|
| `example_basic_encode_decode`  | `basic_encode_decode`   |
| `example_compaction_benchmark` | `compaction_benchmark`  |
| `example_rom_lookup`           | `rom_lookup`            |
| `example_prefix_search`        | `prefix_search`         |
| `example_cpp_usage`            | `cpp_usage`             |
| `example_json_roundtrip`       | `json_roundtrip`        |

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

The project targets **100% line coverage** for the core library. Coverage
gaps in example programs and platform-specific fallbacks are acceptable.
