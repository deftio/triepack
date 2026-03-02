/**
 * @file test_wrapper_core.cpp
 * @brief C++ wrapper tests for core Encoder/Dict/Iterator.
 */

extern "C" {
#include "unity.h"
}

#include "triepack/triepack.hpp"

#include <cstdlib>
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

/* ── Tests ───────────────────────────────────────────────────────────── */

void test_encoder_create(void)
{
    triepack::Encoder enc;
    TEST_ASSERT_NOT_NULL(enc.handle());
}

void test_encoder_insert_and_encode(void)
{
    triepack::Encoder enc;
    enc.insert("apple", 10);
    enc.insert("banana", 20);
    enc.insert("cherry", 30);

    const uint8_t *data = nullptr;
    size_t size = 0;
    int rc = enc.encode(&data, &size);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN(0, size);
}

void test_dict_open_and_lookup(void)
{
    triepack::Encoder enc;
    enc.insert("hello", 42);
    enc.insert("world", 99);

    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    TEST_ASSERT_NOT_NULL(dict.handle());

    int32_t val = 0;
    TEST_ASSERT_TRUE(dict.lookup("hello", &val));
    TEST_ASSERT_EQUAL_INT32(42, val);

    TEST_ASSERT_TRUE(dict.lookup("world", &val));
    TEST_ASSERT_EQUAL_INT32(99, val);
}

void test_dict_size(void)
{
    triepack::Encoder enc;
    enc.insert("a", 1);
    enc.insert("b", 2);
    enc.insert("c", 3);

    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    TEST_ASSERT_EQUAL(3, dict.size());
}

void test_lookup_missing_key(void)
{
    triepack::Encoder enc;
    enc.insert("exists", 1);

    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    int32_t val = 0;
    TEST_ASSERT_FALSE(dict.lookup("missing", &val));
}

void test_move_encoder(void)
{
    triepack::Encoder enc1;
    enc1.insert("key", 42);

    triepack::Encoder enc2(static_cast<triepack::Encoder &&>(enc1));
    TEST_ASSERT_NULL(enc1.handle());
    TEST_ASSERT_NOT_NULL(enc2.handle());

    const uint8_t *data = nullptr;
    size_t size = 0;
    int rc = enc2.encode(&data, &size);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_move_assign_encoder(void)
{
    triepack::Encoder enc1;
    enc1.insert("key", 1);

    triepack::Encoder enc2;
    enc2 = static_cast<triepack::Encoder &&>(enc1);
    TEST_ASSERT_NULL(enc1.handle());
    TEST_ASSERT_NOT_NULL(enc2.handle());
}

void test_move_dict(void)
{
    triepack::Encoder enc;
    enc.insert("x", 5);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict1(data, size);
    triepack::Dict dict2(static_cast<triepack::Dict &&>(dict1));
    TEST_ASSERT_NULL(dict1.handle());
    TEST_ASSERT_NOT_NULL(dict2.handle());

    int32_t val = 0;
    TEST_ASSERT_TRUE(dict2.lookup("x", &val));
    TEST_ASSERT_EQUAL_INT32(5, val);
}

void test_shared_prefix_keys(void)
{
    triepack::Encoder enc;
    enc.insert("abc", 1);
    enc.insert("abd", 2);
    enc.insert("xyz", 3);

    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    int32_t val = 0;

    TEST_ASSERT_TRUE(dict.lookup("abc", &val));
    TEST_ASSERT_EQUAL_INT32(1, val);

    TEST_ASSERT_TRUE(dict.lookup("abd", &val));
    TEST_ASSERT_EQUAL_INT32(2, val);

    TEST_ASSERT_TRUE(dict.lookup("xyz", &val));
    TEST_ASSERT_EQUAL_INT32(3, val);
}

/* ── Move-assign self-check coverage ─────────────────────────────── */

void test_move_assign_encoder_self(void)
{
    triepack::Encoder enc;
    enc.insert("key", 42);
    triepack::Encoder &ref = enc;
    enc = static_cast<triepack::Encoder &&>(ref);
    TEST_ASSERT_NOT_NULL(enc.handle());

    const uint8_t *data = nullptr;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(0, enc.encode(&data, &size));
}

void test_move_assign_dict(void)
{
    triepack::Encoder enc;
    enc.insert("a", 10);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict1(data, size);
    triepack::Dict dict2(data, size);
    dict2 = static_cast<triepack::Dict &&>(dict1);
    TEST_ASSERT_NULL(dict1.handle());
    TEST_ASSERT_NOT_NULL(dict2.handle());
    int32_t val = 0;
    TEST_ASSERT_TRUE(dict2.lookup("a", &val));
    TEST_ASSERT_EQUAL_INT32(10, val);
}

void test_move_assign_dict_self(void)
{
    triepack::Encoder enc;
    enc.insert("a", 5);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Dict &ref = dict;
    dict = static_cast<triepack::Dict &&>(ref);
    TEST_ASSERT_NOT_NULL(dict.handle());
    int32_t val = 0;
    TEST_ASSERT_TRUE(dict.lookup("a", &val));
    TEST_ASSERT_EQUAL_INT32(5, val);
}

void test_lookup_null_out_value(void)
{
    triepack::Encoder enc;
    enc.insert("key", 42);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    bool found = dict.lookup("key", nullptr);
    TEST_ASSERT_TRUE(found);

    found = dict.lookup("missing", nullptr);
    TEST_ASSERT_FALSE(found);
}

/* ── Iterator coverage ───────────────────────────────────────────── */

void test_iterator_create(void)
{
    triepack::Encoder enc;
    enc.insert("apple", 1);
    enc.insert("banana", 2);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it(dict);
    TEST_ASSERT_NOT_NULL(it.handle());
}

void test_iterator_next(void)
{
    triepack::Encoder enc;
    enc.insert("key", 1);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it(dict);
    /* Iterator stub returns EOF, so next() returns false */
    /* This still exercises the code path */
    (void)it.next();
}

void test_iterator_key_value_stubs(void)
{
    triepack::Encoder enc;
    enc.insert("key", 42);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it(dict);
    /* Stubs return nullptr/0 */
    TEST_ASSERT_NULL(it.key());
    TEST_ASSERT_EQUAL_INT32(0, it.value());
}

void test_iterator_move_construct(void)
{
    triepack::Encoder enc;
    enc.insert("a", 1);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it1(dict);
    triepack::Iterator it2(static_cast<triepack::Iterator &&>(it1));
    TEST_ASSERT_NULL(it1.handle());
    TEST_ASSERT_NOT_NULL(it2.handle());
}

void test_iterator_move_assign(void)
{
    triepack::Encoder enc;
    enc.insert("a", 1);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it1(dict);
    triepack::Iterator it2(dict);
    it2 = static_cast<triepack::Iterator &&>(it1);
    TEST_ASSERT_NULL(it1.handle());
    TEST_ASSERT_NOT_NULL(it2.handle());
}

void test_iterator_move_assign_self(void)
{
    triepack::Encoder enc;
    enc.insert("a", 1);
    const uint8_t *data = nullptr;
    size_t size = 0;
    enc.encode(&data, &size);

    triepack::Dict dict(data, size);
    triepack::Iterator it(dict);
    triepack::Iterator &ref = it;
    it = static_cast<triepack::Iterator &&>(ref);
    TEST_ASSERT_NOT_NULL(it.handle());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoder_create);
    RUN_TEST(test_encoder_insert_and_encode);
    RUN_TEST(test_dict_open_and_lookup);
    RUN_TEST(test_dict_size);
    RUN_TEST(test_lookup_missing_key);
    RUN_TEST(test_move_encoder);
    RUN_TEST(test_move_assign_encoder);
    RUN_TEST(test_move_dict);
    RUN_TEST(test_shared_prefix_keys);
    /* Coverage additions */
    RUN_TEST(test_move_assign_encoder_self);
    RUN_TEST(test_move_assign_dict);
    RUN_TEST(test_move_assign_dict_self);
    RUN_TEST(test_lookup_null_out_value);
    RUN_TEST(test_iterator_create);
    RUN_TEST(test_iterator_next);
    RUN_TEST(test_iterator_key_value_stubs);
    RUN_TEST(test_iterator_move_construct);
    RUN_TEST(test_iterator_move_assign);
    RUN_TEST(test_iterator_move_assign_self);
    return UNITY_END();
}
