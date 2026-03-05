/**
 * @file test_json_decode.c
 * @brief Tests for JSON decode path (json_decode.c — .trp → JSON text).
 */

#include "core_internal.h"
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

/* ── Control character in string (\u00xx path in json_decode.c) ──── */

void test_control_char_in_string(void)
{
    /* Encode a string containing control characters (0x01, 0x1F)
       via core encoder, then decode to JSON to hit the \u00xx escape path */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    /* String with control chars: SOH (0x01), US (0x1F) */
    char ctrl_str[3] = {0x01, 0x1F, 0};
    tp_value v = tp_value_string_n(ctrl_str, 2);
    tp_encoder_add(enc, "ctrl", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    /* Should contain \u0001 and \u001f escapes */
    TEST_ASSERT_NOT_NULL(strstr(out, "\\u00"));

    free(out);
    free(buf);
}

/* ── Float32 that renders as integer (needs .0 append) ─────────────── */

void test_float32_integer_like(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    /* 1.0f renders as "1" with %g, triggering the .0 append path */
    tp_value v = tp_value_float32(1.0f);
    tp_encoder_add(enc, "f", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

/* ── Empty top-level array ─────────────────────────────────────────── */

void test_empty_top_level_array(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("[]", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "[]"));

    free(out);
    free(buf);
}

/* ── String value with NULL pointer ────────────────────────────────── */

void test_null_string_pointer_decode(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    /* String with NULL pointer and 0 length */
    tp_value v;
    v.type = TP_STRING;
    v.data.string_val.str = NULL;
    v.data.string_val.str_len = 0;
    tp_encoder_add(enc, "ns", &v);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    /* NULL string should render as "" */
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

/* ── Pretty print top-level array ──────────────────────────────────── */

void test_pretty_top_level_array(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("[1,2,3]", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode_pretty(buf, buf_len, "  ", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\n"));

    free(out);
    free(buf);
}

/* ── Nested arrays and objects ─────────────────────────────────────── */

void test_nested_array_in_array(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json("{\"matrix\":[[1,2],[3,4]]}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);

    free(out);
    free(buf);
}

/* ── Many value types in single object ─────────────────────────────── */

void test_all_value_types_decode(void)
{
    size_t buf_len;
    uint8_t *buf = encode_json(
        "{\"a\":null,\"b\":true,\"c\":false,\"d\":42,\"e\":-7,\"f\":3.14,\"g\":\"hello\"}",
        &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "null"));
    TEST_ASSERT_NOT_NULL(strstr(out, "true"));
    TEST_ASSERT_NOT_NULL(strstr(out, "false"));
    TEST_ASSERT_NOT_NULL(strstr(out, "42"));
    TEST_ASSERT_NOT_NULL(strstr(out, "-7"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hello\""));

    free(out);
    free(buf);
}

/* ── Pretty print with objects and arrays ──────────────────────────── */

void test_pretty_mixed_structure(void)
{
    size_t buf_len;
    uint8_t *buf =
        encode_json("{\"users\":[{\"name\":\"alice\"},{\"name\":\"bob\"}],\"count\":2}", &buf_len);

    char *out = NULL;
    size_t out_len = 0;
    tp_result rc = tp_json_decode_pretty(buf, buf_len, "\t", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\t"));

    free(out);
    free(buf);
}

/* ── Progressive corruption of .trp trie data for json_decode ──────── */
/* Corrupts trie data bytes and recalculates CRC so tp_dict_open passes
   but the DFS walker hits error paths in json_decode.c (lines 629, 663,
   667, 681, 707, 711, 739, 743, 746, 770-774) */

static void fix_crc(uint8_t *buf, size_t len)
{
    /* CRC is last 4 bytes, computed over buf[0..len-5] */
    if (len < 4)
        return;
    size_t crc_data_len = len - 4;
    uint32_t crc = tp_crc32(buf, crc_data_len);
    buf[crc_data_len] = (uint8_t)(crc >> 24);
    buf[crc_data_len + 1] = (uint8_t)(crc >> 16);
    buf[crc_data_len + 2] = (uint8_t)(crc >> 8);
    buf[crc_data_len + 3] = (uint8_t)(crc);
}

void test_json_decode_progressive_truncation(void)
{
    /* Build a complex .trp with mixed types to hit many decode paths */
    const char *json = "{\"alpha\":\"hello\",\"beta\":42,\"gamma\":[1,2,3],"
                       "\"delta\":{\"nested\":true}}";
    size_t json_len = strlen(json);
    uint8_t *orig_buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, json_len, &orig_buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    /* Corrupt each trie byte individually and try decoding.
       Keep header intact (first 32 bytes), corrupt data region. */
    uint8_t *buf = malloc(buf_len);
    TEST_ASSERT_NOT_NULL(buf);

    for (size_t i = 40; i < buf_len - 4; i++) {
        memcpy(buf, orig_buf, buf_len);
        buf[i] = (uint8_t)(~buf[i]); /* flip all bits */
        fix_crc(buf, buf_len);

        char *out = NULL;
        size_t out_len = 0;
        rc = tp_json_decode(buf, buf_len, &out, &out_len);
        if (rc == TP_OK) {
            free(out);
        }
    }

    /* Also zero out progressively larger trie regions */
    for (size_t end = 41; end < buf_len - 4; end++) {
        memcpy(buf, orig_buf, buf_len);
        memset(buf + 40, 0xFF, end - 40);
        fix_crc(buf, buf_len);

        char *out = NULL;
        size_t out_len = 0;
        rc = tp_json_decode(buf, buf_len, &out, &out_len);
        if (rc == TP_OK) {
            free(out);
        }
    }

    free(buf);
    free(orig_buf);
}

/* ── Corruption test with BRANCH(1) trie structure ─────────────────── */
/* Keys "a" and "ab" produce BRANCH(1) nodes, targeting json_decode.c
   line 746 (child_count==1 branch) and other DFS walker paths */

void test_json_decode_branch1_corruption(void)
{
    const char *json = "{\"a\":1,\"ab\":2}";
    size_t json_len = strlen(json);
    uint8_t *orig_buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, json_len, &orig_buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    uint8_t *buf = malloc(buf_len);
    TEST_ASSERT_NOT_NULL(buf);

    /* Strategy 1: Flip each byte individually */
    for (size_t i = 36; i < buf_len - 4; i++) {
        memcpy(buf, orig_buf, buf_len);
        buf[i] = (uint8_t)(~buf[i]);
        fix_crc(buf, buf_len);

        char *out = NULL;
        size_t out_len = 0;
        rc = tp_json_decode(buf, buf_len, &out, &out_len);
        if (rc == TP_OK)
            free(out);
    }

    /* Strategy 2: Set all bytes from position i to end to 0x80
       (varint continuation flag) so varint reads hit EOF */
    for (size_t i = 36; i < buf_len - 4; i++) {
        memcpy(buf, orig_buf, buf_len);
        memset(buf + i, 0x80, buf_len - 4 - i);
        fix_crc(buf, buf_len);

        char *out = NULL;
        size_t out_len = 0;
        rc = tp_json_decode(buf, buf_len, &out, &out_len);
        if (rc == TP_OK)
            free(out);
    }

    free(buf);
    free(orig_buf);
}

/* ── Corruption of complex JSON with nested objects + arrays ───────── */

void test_json_decode_complex_corruption(void)
{
    const char *json = "{\"x\":{\"y\":1},\"z\":[2,3],\"w\":\"hello\"}";
    size_t json_len = strlen(json);
    uint8_t *orig_buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, json_len, &orig_buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    uint8_t *buf = malloc(buf_len);
    TEST_ASSERT_NOT_NULL(buf);

    /* Varint-extension corruption: fill remaining bytes with continuation */
    for (size_t i = 36; i < buf_len - 4; i++) {
        memcpy(buf, orig_buf, buf_len);
        memset(buf + i, 0x80, buf_len - 4 - i);
        fix_crc(buf, buf_len);

        char *out = NULL;
        size_t out_len = 0;
        rc = tp_json_decode(buf, buf_len, &out, &out_len);
        if (rc == TP_OK)
            free(out);
    }

    free(buf);
    free(orig_buf);
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
    /* New coverage tests */
    RUN_TEST(test_control_char_in_string);
    RUN_TEST(test_float32_integer_like);
    RUN_TEST(test_empty_top_level_array);
    RUN_TEST(test_null_string_pointer_decode);
    RUN_TEST(test_pretty_top_level_array);
    RUN_TEST(test_nested_array_in_array);
    RUN_TEST(test_all_value_types_decode);
    /* Truncated buffer DFS walker */
    RUN_TEST(test_json_decode_progressive_truncation);
    RUN_TEST(test_json_decode_branch1_corruption);
    RUN_TEST(test_json_decode_complex_corruption);
    return UNITY_END();
}
