/*
 * cpp_usage.cpp
 *
 * Demonstrates the C++ wrapper API for triepack.  The wrapper
 * provides RAII handles so no explicit cleanup is needed.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.hpp"

#include <cstdio>

int main()
{
    /*
     * Step 1 - Create an encoder.
     *
     * triepack::Encoder enc;
     */

    /*
     * Step 2 - Add entries and build.
     *
     * (API methods to be defined on the C++ wrapper)
     */

    /*
     * Step 3 - Open for reading.
     *
     * triepack::Dict dict(blob_data, blob_size);
     */

    /*
     * Step 4 - Lookup.
     *
     * (API methods to be defined on the C++ wrapper)
     */

    /*
     * Step 5 - RAII: destructors release all resources automatically.
     */

    std::printf("triepack C++ wrapper example (stub -- wrapper not yet implemented)\n");
    return 0;
}
