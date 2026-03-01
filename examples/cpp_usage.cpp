/*
 * cpp_usage.cpp
 *
 * Demonstrates the C++ wrapper API for triepack. The wrapper
 * provides RAII handles so no explicit cleanup is needed.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.hpp"

#include <cstdio>
#include <cstdlib>

int main()
{
    std::printf("=== C++ Wrapper Example ===\n\n");

    /* Step 1 -- Create an encoder and add entries. */
    triepack::Encoder enc;

    enc.insert("apple", 10);
    enc.insert("banana", 20);
    enc.insert("cherry", 30);
    enc.insert("date", 40);
    enc.insert("elderberry", 50);

    /* Step 2 -- Build the compressed blob. */
    const uint8_t *data = nullptr;
    size_t size = 0;
    int rc = enc.encode(&data, &size);
    if (rc != 0) {
        std::fprintf(stderr, "encode failed: %d\n", rc);
        return 1;
    }
    std::printf("Encoded %zu bytes\n\n", size);

    /* Step 3 -- Open for reading (RAII: dict auto-closes on scope exit). */
    triepack::Dict dict(data, size);

    std::printf("Dictionary contains %zu entries\n\n", dict.size());

    /* Step 4 -- Look up values. */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry", "fig"};
    size_t num_keys = sizeof(keys) / sizeof(keys[0]);

    for (size_t i = 0; i < num_keys; i++) {
        int32_t val = 0;
        if (dict.lookup(keys[i], &val)) {
            std::printf("  %-12s -> %d\n", keys[i], val);
        } else {
            std::printf("  %-12s -> NOT FOUND\n", keys[i]);
        }
    }

    /* Step 5 -- Move semantics demo. */
    std::printf("\nMove semantics:\n");
    triepack::Dict dict2(static_cast<triepack::Dict &&>(dict));
    std::printf("  dict.handle()  = %s\n", dict.handle() ? "valid" : "null (moved)");
    std::printf("  dict2.handle() = %s\n", dict2.handle() ? "valid" : "null");

    int32_t val = 0;
    if (dict2.lookup("cherry", &val)) {
        std::printf("  dict2.lookup(\"cherry\") -> %d\n", val);
    }

    /* Step 6 -- RAII: destructors release all resources automatically. */
    /* The encoder data buffer was allocated by tp_encoder_build; we must
       free it since the C++ wrapper doesn't own it. */
    std::free(const_cast<uint8_t *>(data));

    std::printf("\nDone.\n");
    return 0;
}
