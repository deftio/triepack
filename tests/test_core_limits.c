/**
 * @file test_core_limits.c
 * @brief Format limits and value-ownership edges of the core encoder.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/triepack.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Alphabet limit ──────────────────────────────────────────────────── */

/* Build a dictionary whose keys use exactly `n` distinct byte values. */
static tp_result build_with_alphabet(int n, uint8_t **out_buf, size_t *out_len)
{
    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return rc;

    for (int i = 0; i < n; i++) {
        uint8_t key[2];
        key[0] = (uint8_t)i;
        key[1] = 'X'; /* 'X' is one of the n values for n > 'X' */
        tp_value v = tp_value_null();
        rc = tp_encoder_add_n(enc, (const char *)key, 2, &v);
        if (rc != TP_OK) {
            tp_encoder_destroy(&enc);
            return rc;
        }
    }

    rc = tp_encoder_build(enc, out_buf, out_len);
    tp_encoder_destroy(&enc);
    return rc;
}

static void test_alphabet_at_limit_builds_and_reads(void)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    /* 249 distinct bytes + 6 control codes is exactly the 255 that
       symbol_count's 8 bits can hold. */
    TEST_ASSERT_EQUAL(TP_OK, build_with_alphabet(TP_MAX_ALPHABET_SIZE, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)TP_MAX_ALPHABET_SIZE, tp_dict_count(dict));

    /* Every key must still be findable — the symptom of the overflow was that
       the buffer built, passed its CRC, and then found nothing. */
    for (int i = 0; i < TP_MAX_ALPHABET_SIZE; i++) {
        uint8_t key[2] = {(uint8_t)i, 'X'};
        tp_value v;
        TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup_n(dict, (const char *)key, 2, &v));
    }

    tp_dict_close(&dict);
    free(buf);
}

static void test_alphabet_over_limit_is_refused(void)
{
    for (int n = TP_MAX_ALPHABET_SIZE + 1; n <= 256; n += 3) {
        uint8_t *buf = NULL;
        size_t len = 0;
        TEST_ASSERT_EQUAL_MESSAGE(TP_ERR_ALPHABET, build_with_alphabet(n, &buf, &len),
                                  "an unreadable dictionary was built");
        TEST_ASSERT_NULL(buf);
    }
}

static void test_alphabet_error_has_a_message(void)
{
    TEST_ASSERT_EQUAL_STRING("keys use too many distinct byte values",
                             tp_result_str(TP_ERR_ALPHABET));
}

/* ── Value ownership ─────────────────────────────────────────────────── */

/* A zero-length blob used to keep the caller's pointer while still being
   freed with the encoder, so the caller's buffer was freed twice. */
static void test_zero_length_blob_does_not_take_caller_memory(void)
{
    uint8_t *payload = malloc(1);
    TEST_ASSERT_NOT_NULL(payload);
    payload[0] = 0xAB;

    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    tp_value empty = tp_value_blob(payload, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "empty", &empty));
    tp_value one = tp_value_blob(payload, 1);
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "one", &one));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    tp_encoder_destroy(&enc);

    /* Still ours to read and free. */
    TEST_ASSERT_EQUAL_UINT8(0xAB, payload[0]);
    free(payload);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    tp_value got;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "empty", &got));
    TEST_ASSERT_EQUAL(TP_BLOB, got.type);
    TEST_ASSERT_EQUAL_size_t(0, got.data.blob_val.len);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "one", &got));
    TEST_ASSERT_EQUAL(TP_BLOB, got.type);
    TEST_ASSERT_EQUAL_size_t(1, got.data.blob_val.len);
    TEST_ASSERT_EQUAL_UINT8(0xAB, got.data.blob_val.data[0]);

    tp_dict_close(&dict);
    free(buf);
}

static void test_empty_string_value_round_trips(void)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    tp_value empty = tp_value_string("");
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, "k", &empty));

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    tp_value got;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "k", &got));
    TEST_ASSERT_EQUAL(TP_STRING, got.type);
    TEST_ASSERT_EQUAL_size_t(0, got.data.string_val.str_len);
    tp_dict_close(&dict);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_alphabet_at_limit_builds_and_reads);
    RUN_TEST(test_alphabet_over_limit_is_refused);
    RUN_TEST(test_alphabet_error_has_a_message);
    RUN_TEST(test_zero_length_blob_does_not_take_caller_memory);
    RUN_TEST(test_empty_string_value_round_trips);
    return UNITY_END();
}
