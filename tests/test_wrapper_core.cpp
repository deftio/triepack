/**
 * @file test_wrapper_core.cpp
 * @brief C++ wrapper tests for Value, Encoder, Dict and Iterator.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" {
#include "unity.h"
}

#include "triepack/triepack.hpp"

#include <cstring>
#include <string>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

using triepack::Dict;
using triepack::Encoder;
using triepack::Iterator;
using triepack::Status;
using triepack::Type;
using triepack::Value;

/* ── Helpers ─────────────────────────────────────────────────────────── */

namespace
{

/// Encode a small fixed dictionary used by several tests.
std::vector<uint8_t> build_sample()
{
    Encoder enc;
    enc.add("apple", Value::unsigned_integer(1));
    enc.add("application", Value::unsigned_integer(2));
    enc.add("apply", Value::unsigned_integer(3));
    enc.add("banana", Value::unsigned_integer(4));
    std::vector<uint8_t> out;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(enc.build(out)));
    return out;
}

void assert_status(Status expected, Status actual)
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

} // namespace

/* ── Value ───────────────────────────────────────────────────────────── */

void test_value_defaults_to_null(void)
{
    Value v;
    TEST_ASSERT_TRUE(v.is_null());
    TEST_ASSERT_TRUE(v.type() == Type::Null);
    TEST_ASSERT_TRUE(v == Value::null());
}

void test_value_holds_each_type(void)
{
    TEST_ASSERT_TRUE(Value::boolean(true).as_bool());
    TEST_ASSERT_EQUAL_INT64(-42, Value::integer(-42).as_int());
    TEST_ASSERT_EQUAL_UINT64(42u, Value::unsigned_integer(42).as_uint());
    TEST_ASSERT_EQUAL_FLOAT(0.5f, Value::float32(0.5f).as_float32());
    TEST_ASSERT_EQUAL_DOUBLE(1.5, Value::float64(1.5).as_float64());
    TEST_ASSERT_TRUE(Value::string("hi").as_string() == "hi");

    std::vector<uint8_t> raw;
    raw.push_back(0xDE);
    raw.push_back(0xAD);
    TEST_ASSERT_TRUE(Value::blob(raw).as_blob() == raw);
}

void test_value_wrong_accessor_returns_default(void)
{
    /* Reading the wrong member gives a default, not a reinterpreted payload. */
    Value v = Value::string("text");
    TEST_ASSERT_EQUAL_INT64(0, v.as_int());
    TEST_ASSERT_EQUAL_UINT64(0u, v.as_uint());
    TEST_ASSERT_FALSE(v.as_bool());
    TEST_ASSERT_TRUE(v.as_blob().empty());
}

void test_value_equality(void)
{
    TEST_ASSERT_TRUE(Value::integer(7) == Value::integer(7));
    TEST_ASSERT_TRUE(Value::integer(7) != Value::unsigned_integer(7));
    TEST_ASSERT_TRUE(Value::float64(0.0) != Value::float64(-0.0));
    TEST_ASSERT_TRUE(Value::string("a") != Value::string("b"));
}

/* ── Encoder ─────────────────────────────────────────────────────────── */

void test_encoder_create(void)
{
    Encoder enc;
    TEST_ASSERT_NOT_NULL(enc.handle());
    TEST_ASSERT_EQUAL_size_t(0, enc.count());
}

void test_encoder_add_and_build(void)
{
    Encoder enc;
    assert_status(Status::Ok, enc.add("apple", Value::unsigned_integer(10)));
    assert_status(Status::Ok, enc.add("banana", Value::unsigned_integer(20)));
    TEST_ASSERT_EQUAL_size_t(2, enc.count());

    std::vector<uint8_t> out;
    assert_status(Status::Ok, enc.build(out));
    TEST_ASSERT_GREATER_THAN(36, out.size()); /* header + CRC at minimum */
    TEST_ASSERT_EQUAL_UINT8('T', out[0]);
}

void test_encoder_reset(void)
{
    Encoder enc;
    enc.add("a", Value::null());
    TEST_ASSERT_EQUAL_size_t(1, enc.count());
    assert_status(Status::Ok, enc.reset());
    TEST_ASSERT_EQUAL_size_t(0, enc.count());
}

void test_encoder_accepts_keys_with_embedded_nul(void)
{
    Encoder enc;
    const char key[] = {'a', '\0', 'b'};
    assert_status(Status::Ok, enc.add(key, sizeof(key), Value::unsigned_integer(1)));

    std::vector<uint8_t> out;
    assert_status(Status::Ok, enc.build(out));

    Dict dict(out.data(), out.size());
    Value v;
    assert_status(Status::Ok, dict.lookup(key, sizeof(key), v));
    TEST_ASSERT_EQUAL_UINT64(1u, v.as_uint());
}

void test_encoder_build_clears_previous_output(void)
{
    Encoder enc;
    enc.add("k", Value::null());
    std::vector<uint8_t> out(9, 0xFF);
    assert_status(Status::Ok, enc.build(out));
    TEST_ASSERT_EQUAL_UINT8('T', out[0]);
}

/* ── Dict ────────────────────────────────────────────────────────────── */

void test_dict_open_and_lookup(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());
    assert_status(Status::Ok, dict.status());
    TEST_ASSERT_TRUE(dict.is_open());
    TEST_ASSERT_EQUAL_size_t(4, dict.size());

    Value v;
    assert_status(Status::Ok, dict.lookup("apple", v));
    TEST_ASSERT_TRUE(v.type() == Type::UInt);
    TEST_ASSERT_EQUAL_UINT64(1u, v.as_uint());
}

void test_dict_lookup_missing_key(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());
    Value v = Value::integer(99);
    assert_status(Status::NotFound, dict.lookup("cherry", v));
    TEST_ASSERT_TRUE(v.is_null()); /* cleared on failure */
    TEST_ASSERT_FALSE(dict.contains("cherry"));
    TEST_ASSERT_TRUE(dict.contains("apple"));
}

void test_dict_open_rejects_garbage(void)
{
    std::vector<uint8_t> junk(64, 0x5A);
    Dict dict(junk.data(), junk.size());
    TEST_ASSERT_FALSE(dict.is_open());
    assert_status(Status::BadMagic, dict.status());
    TEST_ASSERT_EQUAL_size_t(0, dict.size());
}

void test_dict_default_is_closed(void)
{
    Dict dict;
    TEST_ASSERT_FALSE(dict.is_open());
    Value v;
    assert_status(Status::InvalidParam, dict.lookup("x", v));
}

void test_dict_round_trips_every_value_type(void)
{
    std::vector<uint8_t> raw;
    raw.push_back(0x00);
    raw.push_back(0xFF);

    Encoder enc;
    enc.add("a_null", Value::null());
    enc.add("b_bool", Value::boolean(true));
    enc.add("c_int", Value::integer(-1234));
    enc.add("d_uint", Value::unsigned_integer(1234));
    enc.add("e_f32", Value::float32(0.25f));
    enc.add("f_f64", Value::float64(2.5));
    enc.add("g_str", Value::string("hello"));
    enc.add("h_blob", Value::blob(raw));

    std::vector<uint8_t> buf;
    assert_status(Status::Ok, enc.build(buf));

    Dict dict(buf.data(), buf.size());
    Value v;

    assert_status(Status::Ok, dict.lookup("a_null", v));
    TEST_ASSERT_TRUE(v.is_null());
    assert_status(Status::Ok, dict.lookup("b_bool", v));
    TEST_ASSERT_TRUE(v == Value::boolean(true));
    assert_status(Status::Ok, dict.lookup("c_int", v));
    TEST_ASSERT_TRUE(v == Value::integer(-1234));
    assert_status(Status::Ok, dict.lookup("d_uint", v));
    TEST_ASSERT_TRUE(v == Value::unsigned_integer(1234));
    assert_status(Status::Ok, dict.lookup("e_f32", v));
    TEST_ASSERT_TRUE(v == Value::float32(0.25f));
    assert_status(Status::Ok, dict.lookup("f_f64", v));
    TEST_ASSERT_TRUE(v == Value::float64(2.5));
    assert_status(Status::Ok, dict.lookup("g_str", v));
    TEST_ASSERT_TRUE(v == Value::string("hello"));
    assert_status(Status::Ok, dict.lookup("h_blob", v));
    TEST_ASSERT_TRUE(v == Value::blob(raw));
}

/* ── Iterator ────────────────────────────────────────────────────────── */

void test_iterator_walks_every_key_in_order(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());

    Iterator it(dict);
    assert_status(Status::Ok, it.status());

    std::vector<std::string> keys;
    std::vector<uint64_t> values;
    while (it.next()) {
        keys.push_back(it.key());
        values.push_back(it.value().as_uint());
    }
    assert_status(Status::Eof, it.status());

    TEST_ASSERT_EQUAL_size_t(4, keys.size());
    TEST_ASSERT_TRUE(keys[0] == "apple");
    TEST_ASSERT_TRUE(keys[1] == "application");
    TEST_ASSERT_TRUE(keys[2] == "apply");
    TEST_ASSERT_TRUE(keys[3] == "banana");
    TEST_ASSERT_EQUAL_UINT64(1u, values[0]);
    TEST_ASSERT_EQUAL_UINT64(2u, values[1]);
    TEST_ASSERT_EQUAL_UINT64(3u, values[2]);
    TEST_ASSERT_EQUAL_UINT64(4u, values[3]);
}

void test_iterator_reset_replays(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());

    Iterator it(dict);
    int first_pass = 0;
    while (it.next())
        first_pass++;

    assert_status(Status::Ok, it.reset());
    int second_pass = 0;
    while (it.next())
        second_pass++;

    TEST_ASSERT_EQUAL_INT(4, first_pass);
    TEST_ASSERT_EQUAL_INT(first_pass, second_pass);
}

void test_iterator_prefix(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());

    Iterator it(dict, "appl");
    std::vector<std::string> keys;
    while (it.next())
        keys.push_back(it.key());

    TEST_ASSERT_EQUAL_size_t(3, keys.size());
    TEST_ASSERT_TRUE(keys[0] == "apple");
    TEST_ASSERT_TRUE(keys[1] == "application");
    TEST_ASSERT_TRUE(keys[2] == "apply");
}

void test_iterator_prefix_with_no_matches(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());

    Iterator it(dict, "zzz");
    TEST_ASSERT_FALSE(it.next());
    assert_status(Status::Eof, it.status());
}

void test_iterator_prefix_that_is_also_a_key(void)
{
    Encoder enc;
    enc.add("app", Value::unsigned_integer(1));
    enc.add("apple", Value::unsigned_integer(2));
    enc.add("banana", Value::unsigned_integer(3));
    std::vector<uint8_t> buf;
    enc.build(buf);

    Dict dict(buf.data(), buf.size());
    Iterator it(dict, "app");
    std::vector<std::string> keys;
    while (it.next())
        keys.push_back(it.key());

    TEST_ASSERT_EQUAL_size_t(2, keys.size());
    TEST_ASSERT_TRUE(keys[0] == "app");
    TEST_ASSERT_TRUE(keys[1] == "apple");
}

void test_iterator_over_keys_without_values(void)
{
    Encoder enc;
    enc.add("one", Value::null());
    enc.add("two", Value::null());
    std::vector<uint8_t> buf;
    enc.build(buf);

    Dict dict(buf.data(), buf.size());
    Iterator it(dict);
    int n = 0;
    while (it.next()) {
        TEST_ASSERT_TRUE(it.value().is_null());
        n++;
    }
    TEST_ASSERT_EQUAL_INT(2, n);
}

void test_iterator_on_empty_dict(void)
{
    Encoder enc;
    std::vector<uint8_t> buf;
    enc.build(buf);

    Dict dict(buf.data(), buf.size());
    Iterator it(dict);
    TEST_ASSERT_FALSE(it.next());
}

/* ── Move semantics ──────────────────────────────────────────────────── */

void test_move_encoder(void)
{
    Encoder a;
    a.add("k", Value::unsigned_integer(1));
    Encoder b(static_cast<Encoder &&>(a));
    TEST_ASSERT_NULL(a.handle());
    TEST_ASSERT_NOT_NULL(b.handle());
    TEST_ASSERT_EQUAL_size_t(1, b.count());
}

void test_move_assign_encoder(void)
{
    Encoder a;
    a.add("k", Value::unsigned_integer(1));
    Encoder b;
    b = static_cast<Encoder &&>(a);
    TEST_ASSERT_NULL(a.handle());
    TEST_ASSERT_EQUAL_size_t(1, b.count());

    /* Self-assignment must not destroy the handle. */
    Encoder &ref = b;
    b = static_cast<Encoder &&>(ref);
    TEST_ASSERT_NOT_NULL(b.handle());
}

void test_move_dict(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict a(buf.data(), buf.size());
    Dict b(static_cast<Dict &&>(a));
    TEST_ASSERT_NULL(a.handle());
    TEST_ASSERT_EQUAL_size_t(4, b.size());

    Dict c;
    c = static_cast<Dict &&>(b);
    TEST_ASSERT_NULL(b.handle());
    TEST_ASSERT_EQUAL_size_t(4, c.size());

    Dict &ref = c;
    c = static_cast<Dict &&>(ref);
    TEST_ASSERT_NOT_NULL(c.handle());
}

void test_move_iterator(void)
{
    std::vector<uint8_t> buf = build_sample();
    Dict dict(buf.data(), buf.size());

    Iterator a(dict);
    a.next();
    const std::string first = a.key();

    Iterator b(static_cast<Iterator &&>(a));
    TEST_ASSERT_NULL(a.handle());
    TEST_ASSERT_TRUE(b.key() == first);
    TEST_ASSERT_TRUE(b.next());

    Iterator c(dict);
    c = static_cast<Iterator &&>(b);
    TEST_ASSERT_NULL(b.handle());
    TEST_ASSERT_NOT_NULL(c.handle());

    Iterator &ref = c;
    c = static_cast<Iterator &&>(ref);
    TEST_ASSERT_NOT_NULL(c.handle());
}

/* ── Status strings ──────────────────────────────────────────────────── */

void test_status_messages(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", triepack::message(Status::Ok));
    TEST_ASSERT_EQUAL_STRING("key not found", triepack::message(Status::NotFound));
    TEST_ASSERT_EQUAL_STRING("operation not implemented",
                             triepack::message(Status::Unsupported));
    TEST_ASSERT_EQUAL_size_t(249, triepack::kMaxAlphabetSize);
}

int main(void)
{
    UNITY_BEGIN();
    /* Value */
    RUN_TEST(test_value_defaults_to_null);
    RUN_TEST(test_value_holds_each_type);
    RUN_TEST(test_value_wrong_accessor_returns_default);
    RUN_TEST(test_value_equality);
    /* Encoder */
    RUN_TEST(test_encoder_create);
    RUN_TEST(test_encoder_add_and_build);
    RUN_TEST(test_encoder_reset);
    RUN_TEST(test_encoder_accepts_keys_with_embedded_nul);
    RUN_TEST(test_encoder_build_clears_previous_output);
    /* Dict */
    RUN_TEST(test_dict_open_and_lookup);
    RUN_TEST(test_dict_lookup_missing_key);
    RUN_TEST(test_dict_open_rejects_garbage);
    RUN_TEST(test_dict_default_is_closed);
    RUN_TEST(test_dict_round_trips_every_value_type);
    /* Iterator */
    RUN_TEST(test_iterator_walks_every_key_in_order);
    RUN_TEST(test_iterator_reset_replays);
    RUN_TEST(test_iterator_prefix);
    RUN_TEST(test_iterator_prefix_with_no_matches);
    RUN_TEST(test_iterator_prefix_that_is_also_a_key);
    RUN_TEST(test_iterator_over_keys_without_values);
    RUN_TEST(test_iterator_on_empty_dict);
    /* Move semantics */
    RUN_TEST(test_move_encoder);
    RUN_TEST(test_move_assign_encoder);
    RUN_TEST(test_move_dict);
    RUN_TEST(test_move_iterator);
    /* Status */
    RUN_TEST(test_status_messages);
    return UNITY_END();
}
