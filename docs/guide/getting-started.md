# Getting Started with triepack

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## What is triepack?

triepack is a compressed trie-based dictionary format implemented as a C library.
It encodes key-value data into a compact, ROM-friendly binary representation using
a two-trie architecture (prefix trie for stems, suffix trie for common endings).
The encoded output supports direct read access without full decompression.

## Quick Install

```bash
git clone <repo-url> triepack
cd triepack
cmake -B build
cmake --build build
ctest --test-dir build
```

To install system-wide:

```bash
cmake --install build --prefix /usr/local
```

## Basic Usage (C API)

The typical workflow is: create an encoder, insert key-value pairs, finalize the
encoded blob, then query it with the dictionary reader.

```c
#include "triepack/triepack.h"

int main(void) {
    /* --- Encode --- */
    txz_encoder_t *enc = txz_encoder_create(NULL);

    txz_encoder_add_string(enc, "hello", "world");
    txz_encoder_add_string(enc, "help",  "me");
    txz_encoder_add_int(enc,    "count", 42);

    const uint8_t *blob = NULL;
    size_t blob_len = 0;
    txz_encoder_finalize(enc, &blob, &blob_len);

    /* --- Decode / Query --- */
    txz_dict_t *dict = txz_dict_open(blob, blob_len);

    txz_value_t val;
    if (txz_dict_lookup(dict, "hello", &val) == TXZ_OK) {
        printf("hello => %s\n", val.data.str);  /* "world" */
    }

    if (txz_dict_lookup(dict, "count", &val) == TXZ_OK) {
        printf("count => %d\n", val.data.i32);  /* 42 */
    }

    txz_dict_close(dict);
    txz_encoder_destroy(enc);
    return 0;
}
```

## Linking

With CMake's `find_package`:

```cmake
find_package(triepack REQUIRED)
target_link_libraries(myapp PRIVATE triepack::triepack)
```

Or link directly:

```bash
gcc myapp.c -ltriepack -o myapp
```

## Next Steps

- [Building](building.md) -- build options and cross-compilation
- [API Reference](api-reference.md) -- full function listing
- [Examples](examples.md) -- runnable example programs
