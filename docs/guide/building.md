# Building triepack

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Prerequisites

| Requirement     | Minimum Version |
|-----------------|-----------------|
| CMake           | 3.16+           |
| C compiler      | C99-compatible  |
| C++ compiler    | C++11 (for C++ wrapper only) |
| Doxygen         | any (optional, for docs) |

## Basic Build

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

## Build Options

| Option            | Default | Description                              |
|-------------------|---------|------------------------------------------|
| `BUILD_TESTS`     | `ON`    | Build the test suite                     |
| `BUILD_EXAMPLES`  | `ON`    | Build example programs                   |
| `BUILD_JSON`      | `ON`    | Build the triepack_json library          |
| `BUILD_DOCS`      | `OFF`   | Generate Doxygen HTML documentation      |
| `ENABLE_COVERAGE` | `OFF`   | Instrument for code coverage reporting   |

Pass options with `-D`:

```bash
cmake -B build -DBUILD_DOCS=ON -DENABLE_COVERAGE=ON
```

## Build Types

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release   # optimized, no debug info
cmake -B build -DCMAKE_BUILD_TYPE=Debug      # -g, assertions enabled
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo  # optimized + debug info
```

## Cross-Compilation Notes

### 32-bit build on a 64-bit host

triepack supports both 32-bit and 64-bit targets. To build for 32-bit:

```bash
cmake -B build32 -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32
cmake --build build32
```

On Debian/Ubuntu you may need the multilib packages:

```bash
sudo apt-get install gcc-multilib g++-multilib
```

### Embedded / bare-metal

triepack's core library avoids heap allocation in the read path, making it
suitable for ROM-based embedded use. Provide a CMake toolchain file:

```bash
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
```

## Installing

```bash
cmake --install build --prefix /usr/local
```

This installs headers to `<prefix>/include/triepack/` and libraries to
`<prefix>/lib/`.
