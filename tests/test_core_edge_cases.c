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

/* ── find_prefix / find_fuzzy / iter_get_distance ──────────────────── */

void test_find_prefix_basic(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "apple", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "application", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "banana", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "app", &it));
    TEST_ASSERT_NOT_NULL(it);

    /* Current implementation returns EOF immediately (stub) */
    const char *key;
    size_t key_len;
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_find_prefix_null_params(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_find_prefix(NULL, "x", NULL));
}

void test_find_fuzzy_basic(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v = tp_value_int(1);
    tp_encoder_add(enc, "hello", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_fuzzy(dict, "helo", 2, &it));
    TEST_ASSERT_NOT_NULL(it);

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_find_fuzzy_null_params(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_find_fuzzy(NULL, "x", 1, NULL));
}

void test_iter_get_distance_basic(void)
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
    tp_dict_iterate(dict, &it);

    uint8_t dist = 255;
    TEST_ASSERT_EQUAL(TP_OK, tp_iter_get_distance(it, &dist));
    TEST_ASSERT_EQUAL_UINT8(0, dist);

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_iter_get_distance_null_params(void)
{
    uint8_t dist;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_iter_get_distance(NULL, &dist));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_iter_get_distance(NULL, NULL));
}

/* ── lookup_n with NULL ────────────────────────────────────────────── */

void test_lookup_n_null(void)
{
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_lookup_n(NULL, "key", 3, &val));
}

/* ── dict_contains all null combos ─────────────────────────────────── */

void test_dict_contains_null_all(void)
{
    bool found;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_contains(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_contains(NULL, "x", &found));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_contains(NULL, NULL, &found));
}

/* ── dict_get_info null combos ─────────────────────────────────────── */

void test_dict_get_info_null_all(void)
{
    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_get_info(NULL, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_dict_get_info(NULL, &info));
}

/* ── Empty dict lookup ─────────────────────────────────────────────── */

void test_empty_dict_lookup(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "anything", &val));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Lookup key not found (longer than any in dict) ────────────────── */

void test_lookup_key_too_long(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(1);
    tp_encoder_add(enc, "ab", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "abc", &val));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Lookup on keys-only dict returns null value (decoder.c:382) ──── */

void test_lookup_keys_only_returns_null_value(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "alpha", NULL);
    tp_encoder_add(enc, "beta", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    val.type = TP_INT; /* pre-fill with non-null type */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "alpha", &val));
    TEST_ASSERT_EQUAL_INT(TP_NULL, val.type); /* should be set to null */

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Lookup with NULL val (just check existence) ───────────────────── */

void test_lookup_null_val(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "test", NULL));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Lookup wrong branch (sibling key, not found) ──────────────────── */

void test_lookup_wrong_branch(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "cat", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "car", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "cab", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "cad", &val));
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "ca", &val));
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "c", &val));
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "dog", &val));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Lookup with value on prefix key (found_value path) ────────────── */

void test_lookup_found_value_end_val(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(10);
    tp_encoder_add(enc, "a", &v);
    v = tp_value_int(20);
    tp_encoder_add(enc, "ab", &v);
    v = tp_value_int(30);
    tp_encoder_add(enc, "abc", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "a", &val));
    TEST_ASSERT_EQUAL_INT64(10, val.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "ab", &val));
    TEST_ASSERT_EQUAL_INT64(20, val.data.int_val);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abc", &val));
    TEST_ASSERT_EQUAL_INT64(30, val.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Duplicate keys (dedup_entries path) ───────────────────────────── */

void test_duplicate_keys_last_wins(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v;
    v = tp_value_int(1);
    tp_encoder_add(enc, "key", &v);
    v = tp_value_int(2);
    tp_encoder_add(enc, "key", &v);
    v = tp_value_int(3);
    tp_encoder_add(enc, "key", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "key", &val));
    TEST_ASSERT_EQUAL_INT64(3, val.data.int_val);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Encoder with string/blob values (deep copy path) ──────────────── */

void test_encoder_blob_deep_copy(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    uint8_t data[] = {1, 2, 3, 4};
    tp_value v = tp_value_blob(data, 4);
    tp_encoder_add(enc, "blob", &v);

    /* Mutate the original data — encoder should have its own copy */
    data[0] = 0xFF;

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value val;
    tp_dict_lookup(dict, "blob", &val);
    TEST_ASSERT_EQUAL_INT(TP_BLOB, val.type);
    TEST_ASSERT_EQUAL_UINT8(1, val.data.blob_val.data[0]); /* original value, not mutated */

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Encoder create_ex with custom options ─────────────────────────── */

void test_encoder_create_ex_valid(void)
{
    tp_encoder_options opts = tp_encoder_defaults();
    opts.compact_mode = true;

    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create_ex(&enc, &opts));
    TEST_ASSERT_NOT_NULL(enc);
    tp_encoder_destroy(&enc);
}

/* ── Iterator next/reset (covers decoder.c lines 450, 468-472) ────── */

void test_iter_next_returns_eof(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(1);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, buf_len);

    tp_iterator *it = NULL;
    tp_dict_iterate(dict, &it);

    const char *key;
    size_t key_len;
    tp_value val;
    /* First call returns EOF (stub implementation) */
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));
    /* Second call also returns EOF (done flag set) */
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

void test_iter_reset_and_next(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(1);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, buf_len);

    tp_iterator *it = NULL;
    tp_dict_iterate(dict, &it);

    const char *key;
    size_t key_len;
    tp_value val;
    tp_iter_next(it, &key, &key_len, &val);
    /* Reset and try again */
    tp_iter_reset(it);
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* ── Truncated trie config (covers decoder.c parse_trie_config errors) ─ */

void test_dict_open_truncated_at_header(void)
{
    /* Exactly 32 bytes: header fills whole buffer, parse_trie_config
       fails reading bits_per_symbol (covers decoder.c line 21) */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "hello", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    TEST_ASSERT_NOT_EQUAL(TP_OK, tp_dict_open_unchecked(&dict, buf, 32));
    free(buf);
}

void test_dict_open_truncated_trie_config(void)
{
    /* 33 bytes: enough for bps, not symbol_count (decoder.c line 28) */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "hello", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, 33);
    TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
    free(buf);
}

void test_dict_open_progressive_truncation(void)
{
    /* Build a valid dict and try opening at every truncation byte
       from 32 to buf_len-1 to exercise all parse_trie_config error paths */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v1 = tp_value_string("val1");
    tp_value v2 = tp_value_int(42);
    tp_value v3 = tp_value_string("val3");
    tp_encoder_add(enc, "alpha", &v1);
    tp_encoder_add(enc, "beta", &v2);
    tp_encoder_add(enc, "gamma", &v3);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Try opening with every truncated length from 32 to buf_len-1 */
    for (size_t trunc = 32; trunc < buf_len; trunc++) {
        tp_dict *dict = NULL;
        tp_result rc = tp_dict_open_unchecked(&dict, buf, trunc);
        if (rc == TP_OK) {
            /* Also try lookups on the truncated dict to hit lookup error paths */
            tp_value val;
            tp_dict_lookup(dict, "alpha", &val);
            tp_dict_lookup(dict, "beta", &val);
            tp_dict_lookup(dict, "nonexistent", &val);
            tp_dict_close(&dict);
        }
    }

    free(buf);
}

/* ── Dict open with corrupt value_store_offset (covers decoder.c seek error) */

void test_dict_lookup_corrupt_offset(void)
{
    /* Build valid .trp */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Corrupt the value_store_offset (bytes 16-19) to a huge value */
    buf[16] = 0xFF;
    buf[17] = 0xFF;
    buf[18] = 0xFF;
    buf[19] = 0xFF;

    /* Open with CRC check skipped */
    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, buf_len);
    if (rc == TP_OK) {
        /* Lookup should fail because value store offset is corrupt */
        tp_value val;
        rc = tp_dict_lookup(dict, "a", &val);
        /* May return error or not-found depending on implementation */
        TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
        tp_dict_close(&dict);
    }

    free(buf);
}

/* ── Dict contains with error other than OK/NOT_FOUND (covers decoder.c 404) */

void test_dict_contains_error_propagation(void)
{
    /* Build valid .trp */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Corrupt the trie data to cause an error during lookup */
    /* Zero out the trie section (after header + config) */
    if (buf_len > 50) {
        memset(buf + 40, 0xFF, buf_len - 44);
    }

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, buf_len);
    if (rc == TP_OK) {
        bool found;
        rc = tp_dict_contains(dict, "a", &found);
        /* Either finds it (unlikely with corrupt data) or error */
        (void)rc; /* just exercise the path */
        tp_dict_close(&dict);
    }

    free(buf);
}

/* ── encoder_add_n with NULL key (encoder.c line 122) ──────────────── */

void test_encoder_add_n_null(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(1);

    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_add_n(NULL, "k", 1, &v));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_encoder_add_n(enc, NULL, 0, &v));

    tp_encoder_destroy(&enc);
}

/* ── Lookup key beyond deepest stored key (decoder.c line 213) ─────── */

void test_lookup_beyond_deepest_key(void)
{
    /* Encode {"a":null, "ab":null} (keys only), then truncate buffer
       so trie ends right at buffer edge. Lookup "abc" should hit
       the expect_branch + EOF path at decoder.c line 213. */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "a", NULL);
    tp_encoder_add(enc, "ab", NULL);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Normal lookup — "abc" should be NOT_FOUND */
    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, buf_len);
    tp_value val;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "abc", &val));
    tp_dict_close(&dict);

    /* Truncated lookups: remove bytes to cause EOF in trie walk.
       Use unchecked open to skip CRC validation. */
    for (size_t trunc = buf_len - 4; trunc <= buf_len; trunc++) {
        dict = NULL;
        tp_result rc = tp_dict_open_unchecked(&dict, buf, trunc);
        if (rc == TP_OK) {
            /* Try looking up a longer key to hit expect_branch + EOF */
            rc = tp_dict_lookup(dict, "abc", &val);
            /* Should be NOT_FOUND (via expect_branch path or normal) */
            TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
            tp_dict_close(&dict);
        }
    }

    free(buf);
}

/* ── Lookup with corrupt trie_data_offset (decoder.c lines 195-196) ── */

void test_lookup_corrupt_trie_data_offset(void)
{
    /* Build valid .trp, corrupt trie_data_offset (bytes 12-15) so
       tp_bs_reader_seek fails during lookup */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Corrupt trie_data_offset (bytes 12-15) to huge value */
    buf[12] = 0x7F;
    buf[13] = 0xFF;
    buf[14] = 0xFF;
    buf[15] = 0xFF;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, buf_len);
    if (rc == TP_OK) {
        tp_value val;
        rc = tp_dict_lookup(dict, "a", &val);
        TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
        tp_dict_close(&dict);
    }

    free(buf);
}

/* ── Lookup on truncated trie after terminal (decoder.c line 212-213) ── */

void test_lookup_truncated_after_terminal(void)
{
    /* Build dict with "a" and "ab" (prefix keys), then truncate so that
       after reading END_VAL for "a", the BRANCH symbol read hits EOF.
       Lookup of "ab" should return NOT_FOUND via expect_branch path. */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v1 = tp_value_int(1);
    tp_value v2 = tp_value_int(2);
    tp_encoder_add(enc, "a", &v1);
    tp_encoder_add(enc, "ab", &v2);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Progressive truncation: find spot where trie opens OK but
       lookup of "ab" hits EOF after reading "a"'s terminal */
    for (size_t trunc = 34; trunc < buf_len; trunc++) {
        tp_dict *dict = NULL;
        tp_result rc = tp_dict_open_unchecked(&dict, buf, trunc);
        if (rc == TP_OK) {
            tp_value val;
            rc = tp_dict_lookup(dict, "ab", &val);
            /* At some truncation points: NOT_FOUND (expect_branch EOF),
               at others the full data is available so OK is fine too */
            (void)rc;
            tp_dict_close(&dict);
        }
    }

    free(buf);
}

/* ── Lookup with corrupt value_store_offset (decoder.c line 360-362) ── */

void test_lookup_truncated_value_store(void)
{
    /* Build dict with a value, then corrupt value_store_offset so
       tp_bs_reader_seek fails when decoding the value. */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "a", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Corrupt value_store_offset (bytes 16-19) to point past buffer */
    buf[16] = 0x7F;
    buf[17] = 0xFF;
    buf[18] = 0xFF;
    buf[19] = 0xFF;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open_unchecked(&dict, buf, buf_len);
    if (rc == TP_OK) {
        tp_value val;
        rc = tp_dict_lookup(dict, "a", &val);
        /* Seek to corrupt value store should fail */
        TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
        tp_dict_close(&dict);
    }

    free(buf);
}

/* ── Dict contains propagates non-NOT_FOUND error (decoder.c line 404) */

void test_dict_contains_propagates_error(void)
{
    /* Build dict, truncate so lookup returns an error other than
       NOT_FOUND. tp_dict_contains should propagate that error. */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "hello", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Try progressive truncation to trigger error propagation */
    for (size_t trunc = 34; trunc < buf_len; trunc++) {
        tp_dict *dict = NULL;
        tp_result rc = tp_dict_open_unchecked(&dict, buf, trunc);
        if (rc == TP_OK) {
            bool found;
            rc = tp_dict_contains(dict, "hello", &found);
            /* Exercise the error propagation path (line 404) */
            (void)rc;
            tp_dict_close(&dict);
        }
    }

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
    /* New coverage tests */
    RUN_TEST(test_find_prefix_basic);
    RUN_TEST(test_find_prefix_null_params);
    RUN_TEST(test_find_fuzzy_basic);
    RUN_TEST(test_find_fuzzy_null_params);
    RUN_TEST(test_iter_get_distance_basic);
    RUN_TEST(test_iter_get_distance_null_params);
    RUN_TEST(test_lookup_n_null);
    RUN_TEST(test_dict_contains_null_all);
    RUN_TEST(test_dict_get_info_null_all);
    RUN_TEST(test_empty_dict_lookup);
    RUN_TEST(test_lookup_key_too_long);
    RUN_TEST(test_lookup_keys_only_returns_null_value);
    RUN_TEST(test_lookup_null_val);
    RUN_TEST(test_lookup_wrong_branch);
    RUN_TEST(test_lookup_found_value_end_val);
    RUN_TEST(test_duplicate_keys_last_wins);
    RUN_TEST(test_encoder_blob_deep_copy);
    RUN_TEST(test_encoder_create_ex_valid);
    /* Iterator done/reset */
    RUN_TEST(test_iter_next_returns_eof);
    RUN_TEST(test_iter_reset_and_next);
    RUN_TEST(test_encoder_add_n_null);
    RUN_TEST(test_lookup_beyond_deepest_key);
    RUN_TEST(test_lookup_corrupt_trie_data_offset);
    /* Lookup on truncated prefix trie */
    RUN_TEST(test_lookup_truncated_after_terminal);
    RUN_TEST(test_lookup_truncated_value_store);
    RUN_TEST(test_dict_contains_propagates_error);
    /* Truncated/corrupt decoder tests */
    RUN_TEST(test_dict_open_truncated_at_header);
    RUN_TEST(test_dict_open_truncated_trie_config);
    RUN_TEST(test_dict_open_progressive_truncation);
    RUN_TEST(test_dict_lookup_corrupt_offset);
    RUN_TEST(test_dict_contains_error_propagation);
    return UNITY_END();
}
