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

| File                     | Component tested           |
|--------------------------|----------------------------|
| `test_bitstream.c`       | Bit-level read/write       |
| `test_encoder.c`         | Trie encoding pipeline     |
| `test_dict.c`            | Dictionary lookup/iteration|
| `test_json.c`            | JSON encode/decode         |
| `test_wrapper.cpp`       | C++ wrapper classes        |
| `test_edge_cases.c`      | Empty inputs, limits, fuzz |

## Adding a New Test

1. Create a new source file in `tests/`, e.g. `tests/test_feature.c`.
2. Register it in `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_feature test_feature.c)
   target_link_libraries(test_feature PRIVATE triepack::triepack)
   add_test(NAME test_feature COMMAND test_feature)
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
