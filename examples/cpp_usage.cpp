/*
 * cpp_usage.cpp
 *
 * Demonstrates the C++ wrapper API for triepack. The wrapper provides RAII
 * handles and owning values, so no explicit cleanup is needed.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.hpp"

#include <cstdio>
#include <string>
#include <vector>

using triepack::Dict;
using triepack::Encoder;
using triepack::Iterator;
using triepack::Status;
using triepack::Type;
using triepack::Value;

static const char *type_name(Type t)
{
    switch (t) {
    case Type::Null:
        return "null";
    case Type::Bool:
        return "bool";
    case Type::Int:
        return "int";
    case Type::UInt:
        return "uint";
    case Type::Float32:
        return "f32";
    case Type::Float64:
        return "f64";
    case Type::String:
        return "string";
    case Type::Blob:
        return "blob";
    }
    return "?";
}

static void print_value(const Value &v)
{
    switch (v.type()) {
    case Type::Null:
        std::printf("null");
        break;
    case Type::Bool:
        std::printf("%s", v.as_bool() ? "true" : "false");
        break;
    case Type::Int:
        std::printf("%lld", static_cast<long long>(v.as_int()));
        break;
    case Type::UInt:
        std::printf("%llu", static_cast<unsigned long long>(v.as_uint()));
        break;
    case Type::Float32:
        std::printf("%g", static_cast<double>(v.as_float32()));
        break;
    case Type::Float64:
        std::printf("%g", v.as_float64());
        break;
    case Type::String:
        std::printf("\"%s\"", v.as_string().c_str());
        break;
    case Type::Blob:
        std::printf("<%zu bytes>", v.as_blob().size());
        break;
    }
}

int main()
{
    std::printf("=== C++ Wrapper Example ===\n\n");

    /* Step 1 -- Add entries. Values carry their own type. */
    Encoder enc;
    enc.add("apple", Value::unsigned_integer(10));
    enc.add("application", Value::string("a longer word"));
    enc.add("apply", Value::float64(3.5));
    enc.add("banana", Value::integer(-20));
    enc.add("cherry", Value::boolean(true));
    enc.add("date", Value::null());

    std::vector<uint8_t> raw;
    raw.push_back(0xDE);
    raw.push_back(0xAD);
    enc.add("elderberry", Value::blob(raw));

    std::printf("Added %zu entries\n", enc.count());

    /* Step 2 -- Build. The vector owns the bytes. */
    std::vector<uint8_t> data;
    Status rc = enc.build(data);
    if (rc != Status::Ok) {
        std::fprintf(stderr, "encode failed: %s\n", triepack::message(rc));
        return 1;
    }
    std::printf("Encoded %zu bytes\n\n", data.size());

    /* Step 3 -- Open for reading. The Dict borrows `data`, which outlives it. */
    Dict dict(data.data(), data.size());
    if (!dict.is_open()) {
        std::fprintf(stderr, "open failed: %s\n", triepack::message(dict.status()));
        return 1;
    }
    std::printf("Dictionary contains %zu entries\n\n", dict.size());

    /* Step 4 -- Look up individual keys. */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry", "fig"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        Value v;
        if (dict.lookup(keys[i], v) == Status::Ok) {
            std::printf("  %-12s %-7s ", keys[i], type_name(v.type()));
            print_value(v);
            std::printf("\n");
        } else {
            std::printf("  %-12s NOT FOUND\n", keys[i]);
        }
    }

    /* Step 5 -- Walk every key, in lexicographic order. */
    std::printf("\nAll entries:\n");
    Iterator it(dict);
    while (it.next()) {
        std::printf("  %-12s ", it.key().c_str());
        print_value(it.value());
        std::printf("\n");
    }

    /* Step 6 -- Prefix search descends the trie; it does not scan. */
    std::printf("\nKeys starting with \"appl\":\n");
    Iterator prefix(dict, "appl");
    while (prefix.next()) {
        std::printf("  %-12s ", prefix.key().c_str());
        print_value(prefix.value());
        std::printf("\n");
    }

    /* Step 7 -- Move semantics. */
    std::printf("\nMove semantics:\n");
    Dict moved(static_cast<Dict &&>(dict));
    std::printf("  dict.handle()  = %s\n", dict.handle() ? "valid" : "null (moved)");
    std::printf("  moved.handle() = %s\n", moved.handle() ? "valid" : "null");

    Value v;
    if (moved.lookup("cherry", v) == Status::Ok) {
        std::printf("  moved.lookup(\"cherry\") -> %s\n", v.as_bool() ? "true" : "false");
    }

    /* Step 8 -- RAII: every handle releases itself on scope exit. */
    std::printf("\nDone.\n");
    return 0;
}
