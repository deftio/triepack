/* test_core_values.c — value encode/decode roundtrip tests via encoder/dict */
#include "triepack/triepack.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Helper: encode a single key with given value, open dict, look it up */
static tp_value roundtrip_value(const char *key, const tp_value *in)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, key, in);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    tp_result rc = tp_dict_lookup(dict, key, &out);
    TEST_ASSERT_EQUAL(TP_OK, rc);

    /* We need to keep buf alive, but for simple types the value doesn't
       reference it. For string/blob it does (zero-copy). We'll copy
       what we need before cleanup. */
    tp_value result = out;

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
    return result;
}

/* ── Null ──────────────────────────────────────────────────────────── */

void test_value_null(void)
{
    tp_value in = tp_value_null();
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_NULL, out.type);
}

/* ── Bool ──────────────────────────────────────────────────────────── */

void test_value_bool_true(void)
{
    tp_value in = tp_value_bool(true);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_BOOL, out.type);
    TEST_ASSERT_TRUE(out.data.bool_val);
}

void test_value_bool_false(void)
{
    tp_value in = tp_value_bool(false);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_BOOL, out.type);
    TEST_ASSERT_FALSE(out.data.bool_val);
}

/* ── Int ───────────────────────────────────────────────────────────── */

void test_value_int_zero(void)
{
    tp_value in = tp_value_int(0);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(0, out.data.int_val);
}

void test_value_int_positive(void)
{
    tp_value in = tp_value_int(127);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_INT64(127, out.data.int_val);
}

void test_value_int_negative(void)
{
    tp_value in = tp_value_int(-128);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_INT64(-128, out.data.int_val);
}

void test_value_int_max(void)
{
    tp_value in = tp_value_int(INT64_MAX);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_INT64(INT64_MAX, out.data.int_val);
}

void test_value_int_min(void)
{
    tp_value in = tp_value_int(INT64_MIN);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, out.data.int_val);
}

/* ── Uint ──────────────────────────────────────────────────────────── */

void test_value_uint_zero(void)
{
    tp_value in = tp_value_uint(0);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_UINT, out.type);
    TEST_ASSERT_EQUAL_UINT64(0, out.data.uint_val);
}

void test_value_uint_large(void)
{
    tp_value in = tp_value_uint(UINT64_MAX);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, out.data.uint_val);
}

void test_value_uint_one(void)
{
    tp_value in = tp_value_uint(1);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL_UINT64(1, out.data.uint_val);
}

/* ── Float32 ───────────────────────────────────────────────────────── */

void test_value_float32_zero(void)
{
    tp_value in = tp_value_float32(0.0f);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_FLOAT32, out.type);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, out.data.float32_val);
}

void test_value_float32_pi(void)
{
    tp_value in = tp_value_float32(3.14159f);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, 3.14159f, out.data.float32_val);
}

void test_value_float32_negative(void)
{
    tp_value in = tp_value_float32(-3.14f);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.14f, out.data.float32_val);
}

/* ── Float64 ───────────────────────────────────────────────────────── */

void test_value_float64_zero(void)
{
    tp_value in = tp_value_float64(0.0);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_EQUAL(TP_FLOAT64, out.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, out.data.float64_val);
}

void test_value_float64_precise(void)
{
    tp_value in = tp_value_float64(2.718281828459045);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 2.718281828459045, out.data.float64_val);
}

void test_value_float64_huge(void)
{
    tp_value in = tp_value_float64(1.7976931348623157e+308);
    tp_value out = roundtrip_value("k", &in);
    TEST_ASSERT_DOUBLE_WITHIN(1e+293, 1.7976931348623157e+308, out.data.float64_val);
}

/* ── String ────────────────────────────────────────────────────────── */

void test_value_string_empty(void)
{
    tp_value in = tp_value_string("");
    /* String values reference the underlying buffer, so we test via
       the full encode-decode pipeline */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "k", &in);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "k", &out));
    TEST_ASSERT_EQUAL(TP_STRING, out.type);
    TEST_ASSERT_EQUAL(0, out.data.string_val.str_len);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_value_string_ascii(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value in = tp_value_string("hello world");
    tp_encoder_add(enc, "k", &in);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "k", &out));
    TEST_ASSERT_EQUAL(TP_STRING, out.type);
    TEST_ASSERT_EQUAL(11, out.data.string_val.str_len);
    if (out.data.string_val.str) {
        TEST_ASSERT_EQUAL_MEMORY("hello world", out.data.string_val.str, 11);
    }

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

/* ── Blob ──────────────────────────────────────────────────────────── */

void test_value_blob_empty(void)
{
    tp_value in = tp_value_blob(NULL, 0);
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "k", &in);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "k", &out));
    TEST_ASSERT_EQUAL(TP_BLOB, out.type);
    TEST_ASSERT_EQUAL(0, out.data.blob_val.len);

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_value_blob_binary(void)
{
    uint8_t data[] = {0x00, 0x01, 0xFF, 0x42, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
    tp_value in = tp_value_blob(data, sizeof(data));

    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "k", &in);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);

    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);

    tp_value out;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "k", &out));
    TEST_ASSERT_EQUAL(TP_BLOB, out.type);
    TEST_ASSERT_EQUAL(sizeof(data), out.data.blob_val.len);
    if (out.data.blob_val.data) {
        TEST_ASSERT_EQUAL_MEMORY(data, out.data.blob_val.data, sizeof(data));
    }

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_value_null);
    RUN_TEST(test_value_bool_true);
    RUN_TEST(test_value_bool_false);
    RUN_TEST(test_value_int_zero);
    RUN_TEST(test_value_int_positive);
    RUN_TEST(test_value_int_negative);
    RUN_TEST(test_value_int_max);
    RUN_TEST(test_value_int_min);
    RUN_TEST(test_value_uint_zero);
    RUN_TEST(test_value_uint_large);
    RUN_TEST(test_value_uint_one);
    RUN_TEST(test_value_float32_zero);
    RUN_TEST(test_value_float32_pi);
    RUN_TEST(test_value_float32_negative);
    RUN_TEST(test_value_float64_zero);
    RUN_TEST(test_value_float64_precise);
    RUN_TEST(test_value_float64_huge);
    RUN_TEST(test_value_string_empty);
    RUN_TEST(test_value_string_ascii);
    RUN_TEST(test_value_blob_empty);
    RUN_TEST(test_value_blob_binary);
    return UNITY_END();
}
