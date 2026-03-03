/**
 * @file test_json_decode.c
 * @brief Tests for JSON decode path (json_decode.c — .trp → JSON text).
 */

#include "triepack/triepack_json.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── NULL parameter tests ───────────────────────────────────────────── */

void test_decode_null_buf(void)
{
    char *out = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(NULL, 0, &out, &len));
}

void test_decode_null_out_str(void)
{
    uint8_t dummy[4] = {0};
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(dummy, 4, NULL, &len));
}

void test_decode_null_out_len(void)
{
    uint8_t dummy[4] = {0};
    char *out = NULL;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(dummy, 4, &out, NULL));
}

void test_decode_pretty_null_indent(void)
{
    uint8_t dummy[4] = {0};
    char *out = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, NULL, &out, &len));
}

void test_decode_pretty_null_out(void)
{
    uint8_t dummy[4] = {0};
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, "  ", NULL, &len));
}

void test_decode_pretty_null_len(void)
{
    uint8_t dummy[4] = {0};
    char *out = NULL;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, "  ", &out, NULL));
}

/* ── Helper: encode JSON and return .trp buffer ─────────────────────── */

static uint8_t *encode_json(const char *json, size_t *out_len)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    *out_len = len;
    return buf;
}

/* ── Flat object roundtrip ──────────────────────────────────────────── */

void test_flat_object_roundtrip(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":null}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"a\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hello\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "true"));
    TEST_ASSERT_NOT_NULL(strstr(out, "null"));

    free(out);
    free(buf);
}

/* ── Nested object ──────────────────────────────────────────────────── */

void test_nested_object_roundtrip(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":{\"y\":{\"z\":42}}}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "42"));

    free(out);
    free(buf);
}

/* ── Array roundtrip ────────────────────────────────────────────────── */

void test_array_roundtrip(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"arr\":[10,20,30]}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "10"));
    TEST_ASSERT_NOT_NULL(strstr(out, "20"));
    TEST_ASSERT_NOT_NULL(strstr(out, "30"));

    free(out);
    free(buf);
}

/* ── Mixed nested objects + arrays ──────────────────────────────────── */

void test_mixed_nested(void)
{
    /* Use a simpler nested structure: object containing object + array */
    size_t buf_len;
    uint8_t *buf =
        encode_json("{\"config\":{\"name\":\"test\",\"debug\":true},\"items\":[1,2,3]}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    /* Output reconstructs from flattened trie; verify some content present */
    TEST_ASSERT_TRUE(out_len > 0);

    free(out);
    free(buf);
}

/* ── Top-level array ────────────────────────────────────────────────── */

void test_top_level_array_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("[1,2,3]", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "1"));
    TEST_ASSERT_NOT_NULL(strstr(out, "2"));
    TEST_ASSERT_NOT_NULL(strstr(out, "3"));

    free(out);
    free(buf);
}

/* ── Empty object ───────────────────────────────────────────────────── */

void test_empty_object_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "{}"));

    free(out);
    free(buf);
}

/* ── Empty array ────────────────────────────────────────────────────── */

void test_empty_array_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"arr\":[]}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    /* Empty array might show as empty object or arr not present */
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

/* ── Strings with escape chars ──────────────────────────────────────── */

void test_escape_chars_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":\"a\\nb\\tc\\\\d\\\"e\"}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    /* Check that escaped chars are present in JSON output */
    TEST_ASSERT_NOT_NULL(out);
    /* The decoded JSON should have proper escaping */
    TEST_ASSERT_NOT_NULL(strstr(out, "\\n"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\\t"));

    free(out);
    free(buf);
}

/* ── Backspace, form-feed, carriage return escapes ──────────────────── */

void test_control_char_escapes_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":\"\\b\\f\\r\"}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

/* ── Float64 value ──────────────────────────────────────────────────── */

void test_float64_decode(void)
{
    /* Use an exactly-representable float to avoid %.17g rounding artifacts */
    size_t buf_len;
    uint8_t *buf = encode_json("{\"val\":2.5}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "2.5"));

    free(out);
    free(buf);
}

/* ── Float32 via core encoder ───────────────────────────────────────── */

void test_float32_via_core(void)
{
    /* Build .trp directly with float32 value using core API */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_float32(2.5f);
    tp_encoder_add(enc, "f32", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    /* Open as JSON and decode — float32 should render as a number */
    tp_json *j = NULL;
    tp_result rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "f32", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT32, val.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, val.data.float32_val);

    tp_json_close(&j);

    /* Also test JSON text decode to cover the float32 branch in json_decode.c */
    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "2.5"));

    free(out);
    free(buf);
}

/* ── UINT value via core encoder ────────────────────────────────────── */

void test_uint_via_core(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_uint(999);
    tp_encoder_add(enc, "u", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "999"));

    free(out);
    free(buf);
}

/* ── Blob value → null in JSON ──────────────────────────────────────── */

void test_blob_renders_null(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    uint8_t blob_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    tp_value v = tp_value_blob(blob_data, 4);
    tp_encoder_add(enc, "bin", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "null"));

    free(out);
    free(buf);
}

/* ── Pretty-print with indentation verification ─────────────────────── */

void test_pretty_print_indentation(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"a\":1,\"b\":2}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode_pretty(buf, buf_len, "    ", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "    "));
    /* Should have newlines */
    TEST_ASSERT_NOT_NULL(strstr(out, "\n"));

    free(out);
    free(buf);
}

/* ── Pretty-print nested structure ──────────────────────────────────── */

void test_pretty_nested(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"a\":{\"b\":[1,2,3]},\"c\":\"hello\"}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode_pretty(buf, buf_len, "  ", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_GREATER_THAN(0, out_len);

    free(out);
    free(buf);
}

/* ── Large structure (>16 top-level keys → segment realloc) ─────────── */

void test_large_structure_decode(void)
{
    /* Build JSON with 20 top-level keys */
    char json[4096];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - (size_t)pos, "{");
    for (int i = 0; i < 20; i++) {
        if (i > 0) {
            pos += snprintf(json + pos, sizeof(json) - (size_t)pos, ",");
        }
        pos += snprintf(json + pos, sizeof(json) - (size_t)pos, "\"key%d\":%d", i, i * 10);
    }
    pos += snprintf(json + pos, sizeof(json) - (size_t)pos, "}");

    size_t buf_len;
    uint8_t *buf = encode_json(json, &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    /* Verify output is non-empty and looks like JSON */
    TEST_ASSERT_TRUE(out_len > 2);
    TEST_ASSERT_EQUAL_CHAR('{', out[0]);

    free(out);
    free(buf);
}

/* ── Corrupted/truncated buffer ─────────────────────────────────────── */

void test_decode_truncated_buffer(void)
{
    /* Too short for header + CRC */
    uint8_t small[8] = {0x54, 0x52, 0x50, 0x00, 1, 0, 0, 0};
    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(small, sizeof(small), &out, &out_len);
    TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
}

void test_decode_corrupted_crc(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"a\":1}", &buf_len);

    /* Flip a data byte (not magic) to corrupt CRC */
    buf[10] ^= 0xFF;

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_NOT_EQUAL(TP_OK, rc);

    free(buf);
}

/* ── Boolean false value ────────────────────────────────────────────── */

void test_bool_false_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":false}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "false"));

    free(out);
    free(buf);
}

/* ── Negative integer ───────────────────────────────────────────────── */

void test_negative_int_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":-42}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "-42"));

    free(out);
    free(buf);
}

/* ── String value with unicode ──────────────────────────────────────── */

void test_unicode_string_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"x\":\"caf\\u00E9\"}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    /* NULL params */
    RUN_TEST(test_decode_null_buf);
    RUN_TEST(test_decode_null_out_str);
    RUN_TEST(test_decode_null_out_len);
    RUN_TEST(test_decode_pretty_null_indent);
    RUN_TEST(test_decode_pretty_null_out);
    RUN_TEST(test_decode_pretty_null_len);
    /* Roundtrip tests */
    RUN_TEST(test_flat_object_roundtrip);
    RUN_TEST(test_nested_object_roundtrip);
    RUN_TEST(test_array_roundtrip);
    RUN_TEST(test_mixed_nested);
    RUN_TEST(test_top_level_array_decode);
    RUN_TEST(test_empty_object_decode);
    RUN_TEST(test_empty_array_decode);
    RUN_TEST(test_escape_chars_decode);
    RUN_TEST(test_control_char_escapes_decode);
    RUN_TEST(test_float64_decode);
    RUN_TEST(test_float32_via_core);
    RUN_TEST(test_uint_via_core);
    RUN_TEST(test_blob_renders_null);
    /* Pretty-print */
    RUN_TEST(test_pretty_print_indentation);
    RUN_TEST(test_pretty_nested);
    /* Large structure */
    RUN_TEST(test_large_structure_decode);
    /* Error paths */
    RUN_TEST(test_decode_truncated_buffer);
    RUN_TEST(test_decode_corrupted_crc);
    /* Additional value types */
    RUN_TEST(test_bool_false_decode);
    RUN_TEST(test_negative_int_decode);
    RUN_TEST(test_unicode_string_decode);
    return UNITY_END();
}
