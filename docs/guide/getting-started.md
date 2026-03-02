---
layout: default
title: Getting Started
---

# Getting Started with TriePack

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## What is TriePack?

TriePack is a compressed trie-based dictionary format implemented as a C
library. It encodes key-value data into a compact, ROM-friendly binary
representation (`.trp` files) using prefix sharing and bit-level packing.
The encoded output supports direct read access without full decompression.

## Quick Install

```bash
git clone https://github.com/deftio/triepack.git
cd triepack
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To install system-wide:

```bash
cmake --install build --prefix /usr/local
```

## Basic Usage (C API)

The typical workflow is: create an encoder, insert key-value pairs, build
the compressed blob, then query it with the dictionary reader.

```c
#include "triepack/triepack.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* --- Encode --- */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_string("world");
    tp_encoder_add(enc, "hello", &v);

    v = tp_value_int(42);
    tp_encoder_add(enc, "count", &v);

    /* Keys-only entry (set membership) */
    tp_encoder_add(enc, "flag", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    /* --- Decode / Query --- */
    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    if (tp_dict_lookup(dict, "hello", &val) == TP_OK) {
        printf("hello => %.*s\n",
               (int)val.data.string_val.str_len,
               val.data.string_val.str);
    }

    if (tp_dict_lookup(dict, "count", &val) == TP_OK) {
        printf("count => %lld\n", (long long)val.data.int_val);
    }

    bool found = false;
    tp_dict_contains(dict, "flag", &found);
    printf("flag exists: %s\n", found ? "yes" : "no");

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
    return 0;
}
```

## Linking

With CMake's `find_package`:

```cmake
find_package(triepack REQUIRED)
target_link_libraries(myapp PRIVATE triepack::triepack)
```

Or link directly against the library targets:

```cmake
target_link_libraries(myapp PRIVATE triepack_core)
```

## Library Stack

```
triepack_json          (JSON encode/decode)
    |
triepack_core          (trie codec: encoder, dictionary, iterator)
    |
triepack_bitstream     (bit-level I/O, VarInt, UTF-8)
```

Each layer can be used independently.

## Next Steps

- [How It Works](how-it-works.md) -- trie encoding, lookup, and value store explained
- [Bitstream Guide](bitstream-guide.md) -- sub-byte integers, sign extension, and packed bit fields
- [Building](building.md) -- build options and cross-compilation
- [API Reference](api-reference.md) -- full function listing with usage examples
- [Examples](examples.md) -- six runnable example programs
