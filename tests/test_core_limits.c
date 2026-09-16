/**
 * @file test_core_limits.c
 * @brief Value-ownership edges of the core encoder.
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
    RUN_TEST(test_zero_length_blob_does_not_take_caller_memory);
    RUN_TEST(test_empty_string_value_round_trips);
    return UNITY_END();
}
