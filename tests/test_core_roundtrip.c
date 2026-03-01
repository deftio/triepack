/* test_core_roundtrip.c — end-to-end encode/decode roundtrip tests */
#include "triepack/triepack.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Helpers ───────────────────────────────────────────────────────── */

static void roundtrip_single_key_value(const char *key, const tp_value *val)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, val));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(len >= TP_HEADER_SIZE);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, key, &out));
    TEST_ASSERT_EQUAL(val->type, out.type);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Tests ─────────────────────────────────────────────────────────── */

void test_empty_dictionary(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    TEST_ASSERT_NOT_NULL(buf);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(0, tp_dict_count(dict));

    /* Lookup any key should fail */
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "abc", &val));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_single_key_no_value(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "hello", NULL));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "hello", &val));
    TEST_ASSERT_EQUAL(TP_NULL, val.type);

    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "world", &val));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_single_key_null_value(void)
{
    tp_value v = tp_value_null();
    roundtrip_single_key_value("test", &v);
}

void test_single_key_bool_value(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value t = tp_value_bool(true);
    tp_value f = tp_value_bool(false);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "yes", &t));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "no", &f));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "yes", &out));
    TEST_ASSERT_EQUAL(TP_BOOL, out.type);
    TEST_ASSERT_TRUE(out.data.bool_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "no", &out));
    TEST_ASSERT_EQUAL(TP_BOOL, out.type);
    TEST_ASSERT_FALSE(out.data.bool_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_single_key_int_value(void)
{
    tp_value v = tp_value_int(-42);
    roundtrip_single_key_value("neg", &v);
}

void test_single_key_uint_value(void)
{
    tp_value v = tp_value_uint(12345678);
    roundtrip_single_key_value("big", &v);
}

void test_single_key_float32_value(void)
{
    tp_value v = tp_value_float32(3.14f);
    roundtrip_single_key_value("pi", &v);
}

void test_single_key_float64_value(void)
{
    tp_value v = tp_value_float64(2.718281828459045);
    roundtrip_single_key_value("e", &v);
}

void test_single_key_string_value(void)
{
    tp_value v = tp_value_string("world");
    roundtrip_single_key_value("hello", &v);
}

void test_single_key_blob_value(void)
{
    uint8_t data[] = {0x00, 0xFF, 0x42, 0xDE, 0xAD};
    tp_value v = tp_value_blob(data, sizeof(data));
    roundtrip_single_key_value("bin", &v);
}

void test_multiple_keys_with_values(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v1 = tp_value_int(1);
    tp_value v2 = tp_value_int(2);
    tp_value v3 = tp_value_int(3);
    tp_value v4 = tp_value_int(4);
    tp_value v5 = tp_value_int(5);

    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "alpha", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "beta", &v2));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "gamma", &v3));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "delta", &v4));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "epsilon", &v5));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(5, tp_dict_count(dict));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "alpha", &out));
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "epsilon", &out));
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(5, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_shared_prefix_keys(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v1 = tp_value_int(10);
    tp_value v2 = tp_value_int(20);
    tp_value v3 = tp_value_int(30);

    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "abc", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "abd", &v2));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "xyz", &v3));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abc", &out));
    TEST_ASSERT_EQUAL_INT64(10, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abd", &out));
    TEST_ASSERT_EQUAL_INT64(20, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "xyz", &out));
    TEST_ASSERT_EQUAL_INT64(30, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "ab", &out));
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "abe", &out));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_large_key_count(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    char key[16];
    for (int i = 0; i < 200; i++) {
        snprintf(key, sizeof(key), "key_%04d", i);
        tp_value v = tp_value_int(i);
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, &v));
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(200, tp_dict_count(dict));

    /* Verify a few lookups */
    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "key_0000", &out));
    TEST_ASSERT_EQUAL_INT64(0, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "key_0100", &out));
    TEST_ASSERT_EQUAL_INT64(100, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "key_0199", &out));
    TEST_ASSERT_EQUAL_INT64(199, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "key_0200", &out));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_encoder_reset_and_rebuild(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v1 = tp_value_int(1);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "first", &v1));

    uint8_t *buf1 = NULL;
    size_t len1 = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf1, &len1));

    /* Reset and add different data */
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_reset(enc));
    TEST_ASSERT_EQUAL_UINT32(0, tp_encoder_count(enc));

    tp_value v2 = tp_value_int(2);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "second", &v2));

    uint8_t *buf2 = NULL;
    size_t len2 = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf2, &len2));

    /* Both should open successfully */
    tp_dict *dict1 = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict1, buf1, len1));

    tp_dict *dict2 = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict2, buf2, len2));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict1, "first", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict1, "second", &out));

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict2, "second", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict2, "first", &out));

    tp_dict_close(&dict1);
    tp_dict_close(&dict2);
    tp_encoder_destroy(&enc);
    free(buf1);
    free(buf2);
}

void test_binary_reproducibility(void)
{
    /* Same input should produce identical output */
    tp_encoder *enc1 = NULL;
    tp_encoder *enc2 = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc2));

    tp_value v1 = tp_value_int(42);
    tp_value v2 = tp_value_string("hello");

    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc1, "a", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc1, "b", &v2));

    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc2, "a", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc2, "b", &v2));

    uint8_t *buf1 = NULL, *buf2 = NULL;
    size_t len1 = 0, len2 = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc1, &buf1, &len1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc2, &buf2, &len2));

    TEST_ASSERT_EQUAL(len1, len2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, len1);

    tp_encoder_destroy(&enc1);
    tp_encoder_destroy(&enc2);
    free(buf1);
    free(buf2);
}

void test_unchecked_open(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    tp_value v = tp_value_int(99);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "test", &v));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open_unchecked(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "test", &out));
    TEST_ASSERT_EQUAL_INT64(99, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_keys_with_no_values_flag(void)
{
    /* All null values: no has_values flag should be set */
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "a", NULL));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "b", NULL));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "c", NULL));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(dict, &info));
    TEST_ASSERT_FALSE(info.has_values);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "a", &out));
    TEST_ASSERT_EQUAL(TP_NULL, out.type);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_mixed_value_types(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v_bool = tp_value_bool(true);
    tp_value v_int = tp_value_int(-100);
    tp_value v_uint = tp_value_uint(200);
    tp_value v_f32 = tp_value_float32(1.5f);
    tp_value v_f64 = tp_value_float64(-2.5);
    tp_value v_str = tp_value_string("hi");
    uint8_t blob_data[] = {0xCA, 0xFE};
    tp_value v_blob = tp_value_blob(blob_data, 2);

    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "bool", &v_bool));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "int", &v_int));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "uint", &v_uint));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "f32", &v_f32));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "f64", &v_f64));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "str", &v_str));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "blob", &v_blob));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "bool", &out));
    TEST_ASSERT_EQUAL(TP_BOOL, out.type);
    TEST_ASSERT_TRUE(out.data.bool_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "int", &out));
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(-100, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "uint", &out));
    TEST_ASSERT_EQUAL(TP_UINT, out.type);
    TEST_ASSERT_EQUAL_UINT64(200, out.data.uint_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "f32", &out));
    TEST_ASSERT_EQUAL(TP_FLOAT32, out.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, out.data.float32_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "f64", &out));
    TEST_ASSERT_EQUAL(TP_FLOAT64, out.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, -2.5, out.data.float64_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "str", &out));
    TEST_ASSERT_EQUAL(TP_STRING, out.type);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "blob", &out));
    TEST_ASSERT_EQUAL(TP_BLOB, out.type);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_duplicate_keys_last_wins(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v1 = tp_value_int(1);
    tp_value v2 = tp_value_int(2);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "dup", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "dup", &v2));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "dup", &out));
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_keys_added_out_of_order(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value v1 = tp_value_int(3);
    tp_value v2 = tp_value_int(1);
    tp_value v3 = tp_value_int(2);

    /* Add out of lexicographic order */
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "cherry", &v1));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "apple", &v2));
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "banana", &v3));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "apple", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "banana", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "cherry", &out));
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_dictionary);
    RUN_TEST(test_single_key_no_value);
    RUN_TEST(test_single_key_null_value);
    RUN_TEST(test_single_key_bool_value);
    RUN_TEST(test_single_key_int_value);
    RUN_TEST(test_single_key_uint_value);
    RUN_TEST(test_single_key_float32_value);
    RUN_TEST(test_single_key_float64_value);
    RUN_TEST(test_single_key_string_value);
    RUN_TEST(test_single_key_blob_value);
    RUN_TEST(test_multiple_keys_with_values);
    RUN_TEST(test_shared_prefix_keys);
    RUN_TEST(test_large_key_count);
    RUN_TEST(test_encoder_reset_and_rebuild);
    RUN_TEST(test_binary_reproducibility);
    RUN_TEST(test_unchecked_open);
    RUN_TEST(test_keys_with_no_values_flag);
    RUN_TEST(test_mixed_value_types);
    RUN_TEST(test_duplicate_keys_last_wins);
    RUN_TEST(test_keys_added_out_of_order);
    return UNITY_END();
}
