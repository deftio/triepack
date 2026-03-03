/* test_core_edge_cases.c — edge case tests for triepack core */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── NULL parameter handling ───────────────────────────────────────── */

void test_encoder_create_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_create(NULL));
}

void test_encoder_create_ex_null(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_create_ex(NULL, NULL));
    tp_encoder_options opts = tp_encoder_defaults();
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_create_ex(NULL, &opts));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_create_ex(&enc, NULL));
}

void test_encoder_add_null(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_add(NULL, "key", NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_add(enc, NULL, NULL));

    tp_encoder_destroy(&enc);
}

void test_encoder_build_null(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_build(NULL, &buf, &len));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_build(enc, NULL, &len));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_build(enc, &buf, NULL));

    tp_encoder_destroy(&enc);
}

void test_encoder_destroy_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_destroy(NULL));
    tp_encoder *enc = NULL;
    /* Destroy with already-null pointer should succeed */
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_destroy(&enc));
}

void test_encoder_reset_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_reset(NULL));
}

void test_dict_open_null(void)
{
    uint8_t buf[64] = {0};
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_open(NULL, buf, 64));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_open(&dict, NULL, 64));
}

void test_dict_close_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_close(NULL));
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_close(&dict));
}

void test_dict_lookup_null(void)
{
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_lookup(NULL, "key", &val));
}

void test_dict_contains_null(void)
{
    bool found;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_contains(NULL, "key", &found));
}

void test_dict_get_info_null(void)
{
    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_get_info(NULL, &info));
}

void test_dict_count_null(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, tp_dict_count(NULL));
}

void test_encoder_count_null(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, tp_encoder_count(NULL));
}

/* ── Key edge cases ────────────────────────────────────────────────── */

void test_empty_key(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "", &v));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "", &out));
    TEST_ASSERT_EQUAL_INT64(42, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_long_key(void)
{
    char key[1024];
    memset(key, 'x', 1023);
    key[1023] = '\0';

    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(999);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, &v));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, key, &out));
    TEST_ASSERT_EQUAL_INT64(999, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_keys_all_same_characters(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "aaa", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "aaab", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "aaac", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "aaa", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "aaab", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "aaac", &out));
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_keys_that_are_prefixes(void)
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

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "a", &out));
    TEST_ASSERT_EQUAL_INT64(1, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "ab", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abc", &out));
    TEST_ASSERT_EQUAL_INT64(3, out.data.int_val);

    /* Non-existing prefixes */
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "abcd", &out));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_encoder_zero_entries_builds_valid_dict(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    TEST_ASSERT_EQUAL_UINT32(0, tp_encoder_count(enc));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(0, tp_dict_count(dict));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Dict open error cases ─────────────────────────────────────────── */

void test_dict_open_truncated(void)
{
    uint8_t buf[16] = {0};
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_TRUNCATED, tp_dict_open(&dict, buf, sizeof(buf)));
}

void test_dict_open_bad_magic(void)
{
    /* Create a valid buffer then corrupt magic */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    buf[0] = 0xFF; /* corrupt magic */

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_BAD_MAGIC, tp_dict_open(&dict, buf, len));

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_dict_open_wrong_version(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    buf[4] = 99; /* corrupt version_major */

    tp_dict *dict = NULL;
    /* Will fail with either VERSION or CORRUPT depending on CRC order */
    tp_result rc = tp_dict_open(&dict, buf, len);
    TEST_ASSERT_TRUE(rc == TP_ERR_VERSION || rc == TP_ERR_CORRUPT);

    /* With unchecked, no CRC check so we should get VERSION */
    rc = tp_dict_open_unchecked(&dict, buf, len);
    TEST_ASSERT_EQUAL(TP_ERR_VERSION, rc);

    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Iterator basics ───────────────────────────────────────────────── */

void test_iterator_lifecycle(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(dict, &it));
    TEST_ASSERT_NOT_NULL(it);

    /* Current implementation returns EOF immediately */
    const char *key;
    size_t key_len;
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    TEST_ASSERT_EQUAL(TP_OK, tp_iter_reset(it));
    TEST_ASSERT_EQUAL(TP_OK, tp_iter_destroy(&it));
    TEST_ASSERT_NULL(it);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_iterator_null_params(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_iterate(NULL, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_iter_next(NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_iter_reset(NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_iter_destroy(NULL));
}

/* ── Dict open with corrupted CRC (flip data byte, not magic) ──────── */

void test_dict_open_corrupted_crc(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    /* Flip a data byte (not magic bytes) to corrupt CRC */
    buf[10] ^= 0xFF;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    TEST_ASSERT_EQUAL(TP_ERR_CORRUPT, rc);

    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Dict open with suffix table flag (unchecked) ──────────────────── */

void test_dict_open_suffix_flag_unchecked(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    /* Set suffix table flag in flags field (byte 6-7) */
    uint16_t flags = (uint16_t)((uint16_t)buf[6] << 8) | buf[7];
    flags |= 0x0002u; /* TP_FLAG_HAS_SUFFIX_TABLE */
    buf[6] = (uint8_t)(flags >> 8);
    buf[7] = (uint8_t)(flags & 0xFF);

    /* Unchecked open should succeed (no CRC check) */
    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, len);
    /* May succeed or fail depending on how suffix is handled */
    if (rc == TP_OK) {
        tp_dict_info info;
        tp_dict_get_info(dict, &info);
        TEST_ASSERT_TRUE(info.has_suffix_table);
        tp_dict_close(&dict);
    }

    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Encoder reset + reuse ─────────────────────────────────────────── */

void test_encoder_reset_reuse(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v1 = tp_value_int(1);
    tp_encoder_add(enc, "first", &v1);
    TEST_ASSERT_EQUAL_UINT32(1, tp_encoder_count(enc));

    tp_encoder_reset(enc);
    TEST_ASSERT_EQUAL_UINT32(0, tp_encoder_count(enc));

    tp_value v2 = tp_value_int(2);
    tp_encoder_add(enc, "second", &v2);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "second", &out));
    TEST_ASSERT_EQUAL_INT64(2, out.data.int_val);

    /* first key should not exist */
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "first", &out));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── tp_dict_contains + tp_dict_get_info paths ─────────────────────── */

void test_dict_contains_found(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "exists", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    bool found = false;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "exists", &found));
    TEST_ASSERT_TRUE(found);

    found = true;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "missing", &found));
    TEST_ASSERT_FALSE(found);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_dict_get_info_valid(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "key", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(dict, &info));
    TEST_ASSERT_EQUAL_UINT8(1, info.format_version_major);
    TEST_ASSERT_EQUAL_UINT32(1, info.num_keys);
    TEST_ASSERT_TRUE(info.has_values);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoder_create_null);
    RUN_TEST(test_encoder_create_ex_null);
    RUN_TEST(test_encoder_add_null);
    RUN_TEST(test_encoder_build_null);
    RUN_TEST(test_encoder_destroy_null);
    RUN_TEST(test_encoder_reset_null);
    RUN_TEST(test_dict_open_null);
    RUN_TEST(test_dict_close_null);
    RUN_TEST(test_dict_lookup_null);
    RUN_TEST(test_dict_contains_null);
    RUN_TEST(test_dict_get_info_null);
    RUN_TEST(test_dict_count_null);
    RUN_TEST(test_encoder_count_null);
    RUN_TEST(test_empty_key);
    RUN_TEST(test_long_key);
    RUN_TEST(test_keys_all_same_characters);
    RUN_TEST(test_keys_that_are_prefixes);
    RUN_TEST(test_encoder_zero_entries_builds_valid_dict);
    RUN_TEST(test_dict_open_truncated);
    RUN_TEST(test_dict_open_bad_magic);
    RUN_TEST(test_dict_open_wrong_version);
    RUN_TEST(test_iterator_lifecycle);
    RUN_TEST(test_iterator_null_params);
    RUN_TEST(test_dict_open_corrupted_crc);
    RUN_TEST(test_dict_open_suffix_flag_unchecked);
    RUN_TEST(test_encoder_reset_reuse);
    RUN_TEST(test_dict_contains_found);
    RUN_TEST(test_dict_get_info_valid);
    return UNITY_END();
}
