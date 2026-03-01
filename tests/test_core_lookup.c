/* test_core_lookup.c — lookup tests for triepack core */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared test dictionary */
static uint8_t *g_buf;
static size_t g_len;
static tp_dict *g_dict;

void setUp(void) {}
void tearDown(void) {}

static void build_test_dict(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "a", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "ab", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "abc", &v);
    v = tp_value_int(4);
    tp_encoder_add(enc, "abd", &v);
    v = tp_value_int(5);
    tp_encoder_add(enc, "b", &v);
    v = tp_value_int(6);
    tp_encoder_add(enc, "xyz", &v);
    v = tp_value_int(7);
    tp_encoder_add(enc, "xyzw", &v);

    tp_encoder_build(enc, &g_buf, &g_len);
    tp_dict_open(&g_dict, g_buf, g_len);
    tp_encoder_destroy(&enc);
}

static void free_test_dict(void)
{
    tp_dict_close(&g_dict);
    free(g_buf);
    g_buf = NULL;
}

/* ── Tests ─────────────────────────────────────────────────────────── */

void test_lookup_existing_key(void)
{
    build_test_dict();

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(g_dict, "abc", &out));
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);

    free_test_dict();
}

void test_lookup_missing_key(void)
{
    build_test_dict();

    tp_value out;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(g_dict, "missing", &out));

    free_test_dict();
}

void test_lookup_prefix_of_existing_key(void)
{
    build_test_dict();

    tp_value out;
    /* "xy" is a prefix of "xyz" but not a key itself */
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(g_dict, "xy", &out));

    free_test_dict();
}

void test_lookup_key_that_is_prefix(void)
{
    build_test_dict();

    tp_value out;
    /* "a" is a prefix of "ab" but also a key itself */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(g_dict, "a", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);

    /* "ab" is a prefix of "abc" but also a key itself */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(g_dict, "ab", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);

    free_test_dict();
}

void test_lookup_key_extending_existing(void)
{
    build_test_dict();

    tp_value out;
    /* "abcd" extends "abc" but doesn't exist */
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(g_dict, "abcd", &out));

    /* "xyzwv" extends "xyzw" but doesn't exist */
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(g_dict, "xyzwv", &out));

    free_test_dict();
}

void test_contains_true(void)
{
    build_test_dict();

    bool found = false;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(g_dict, "abc", &found));
    TEST_ASSERT_TRUE(found);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(g_dict, "b", &found));
    TEST_ASSERT_TRUE(found);

    free_test_dict();
}

void test_contains_false(void)
{
    build_test_dict();

    bool found = true;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(g_dict, "zzz", &found));
    TEST_ASSERT_FALSE(found);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(g_dict, "xy", &found));
    TEST_ASSERT_FALSE(found);

    free_test_dict();
}

void test_lookup_n_with_explicit_length(void)
{
    build_test_dict();

    tp_value out;
    /* Use lookup_n to look up "abc" but with only 2 bytes = "ab" */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup_n(g_dict, "abc", 2, &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);

    /* Full length */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup_n(g_dict, "abc", 3, &out));
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);

    free_test_dict();
}

void test_dict_info(void)
{
    build_test_dict();

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(g_dict, &info));
    TEST_ASSERT_EQUAL_UINT8(1, info.format_version_major);
    TEST_ASSERT_EQUAL_UINT8(0, info.format_version_minor);
    TEST_ASSERT_EQUAL_UINT32(7, info.num_keys);
    TEST_ASSERT_TRUE(info.has_values);
    TEST_ASSERT_FALSE(info.has_suffix_table);
    TEST_ASSERT_TRUE(info.total_bytes > 0);

    free_test_dict();
}

void test_dict_count(void)
{
    build_test_dict();

    TEST_ASSERT_EQUAL_UINT32(7, tp_dict_count(g_dict));

    free_test_dict();
}

void test_lookup_all_keys(void)
{
    build_test_dict();

    const char *keys[] = {"a", "ab", "abc", "abd", "b", "xyz", "xyzw"};
    int64_t vals[] = {1, 2, 3, 4, 5, 6, 7};

    for (int i = 0; i < 7; i++) {
        tp_value out;
        TEST_ASSERT_EQUAL_MESSAGE(TP_OK, tp_dict_lookup(g_dict, keys[i], &out), keys[i]);
        TEST_ASSERT_EQUAL_INT64(vals[i], out.data.int_val);
    }

    free_test_dict();
}

void test_lookup_with_null_value_out(void)
{
    build_test_dict();

    /* Lookup with NULL value pointer should still succeed */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(g_dict, "abc", NULL));

    free_test_dict();
}

void test_lookup_empty_string(void)
{
    /* Build dict with empty key */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(99);
    tp_encoder_add(enc, "", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "", &out));
    TEST_ASSERT_EQUAL_INT64(99, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_lookup_single_char_keys(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "a", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "b", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "c", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "a", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "b", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "c", &out));
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "d", &out));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lookup_existing_key);
    RUN_TEST(test_lookup_missing_key);
    RUN_TEST(test_lookup_prefix_of_existing_key);
    RUN_TEST(test_lookup_key_that_is_prefix);
    RUN_TEST(test_lookup_key_extending_existing);
    RUN_TEST(test_contains_true);
    RUN_TEST(test_contains_false);
    RUN_TEST(test_lookup_n_with_explicit_length);
    RUN_TEST(test_dict_info);
    RUN_TEST(test_dict_count);
    RUN_TEST(test_lookup_all_keys);
    RUN_TEST(test_lookup_with_null_value_out);
    RUN_TEST(test_lookup_empty_string);
    RUN_TEST(test_lookup_single_char_keys);
    return UNITY_END();
}
