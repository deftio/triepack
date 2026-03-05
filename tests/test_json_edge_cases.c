/**
 * @file test_json_edge_cases.c
 * @brief Edge case and error handling tests for JSON library.
 */

#include "triepack/triepack_json.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── NULL parameter tests ────────────────────────────────────────────── */

void test_encode_null_json(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_encode(NULL, 0, &buf, &len));
}

void test_encode_null_buf(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_encode("{}", 2, NULL, &len));
}

void test_encode_null_len(void)
{
    uint8_t *buf;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_encode("{}", 2, &buf, NULL));
}

void test_decode_null_buf(void)
{
    char *out;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(NULL, 0, &out, &len));
}

void test_decode_null_out(void)
{
    uint8_t dummy[4] = {0};
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(dummy, 4, NULL, &len));
}

void test_decode_pretty_null_indent(void)
{
    uint8_t dummy[4] = {0};
    char *out;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, NULL, &out, &len));
}

/* ── Malformed JSON tests ────────────────────────────────────────────── */

void test_empty_string(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("", 0, &buf, &len));
}

void test_just_whitespace(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("   ", 3, &buf, &len));
}

void test_unclosed_brace(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("{\"a\":1", 6, &buf, &len));
}

void test_unclosed_bracket(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("[1,2", 4, &buf, &len));
}

void test_missing_colon(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("{\"a\" 1}", 7, &buf, &len));
}

void test_missing_quotes(void)
{
    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("{a:1}", 5, &buf, &len));
}

void test_bare_value(void)
{
    uint8_t *buf;
    size_t len;
    /* Bare number is not valid top-level JSON for our encoder (must be object/array) */
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, tp_json_encode("42", 2, &buf, &len));
}

/* ── Deep nesting ────────────────────────────────────────────────────── */

void test_deep_nesting(void)
{
    /* Build JSON with 40 levels of nesting (exceeds TP_MAX_NESTING_DEPTH=32) */
    char json[1024];
    size_t pos = 0;
    int depth = 40;

    for (int i = 0; i < depth; i++) {
        json[pos++] = '{';
        json[pos++] = '"';
        json[pos++] = 'a';
        json[pos++] = '"';
        json[pos++] = ':';
    }
    json[pos++] = '1';
    for (int i = 0; i < depth; i++) {
        json[pos++] = '}';
    }
    json[pos] = '\0';

    uint8_t *buf;
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_DEPTH, tp_json_encode(json, pos, &buf, &len));
}

/* ── Unicode ─────────────────────────────────────────────────────────── */

void test_unicode_escape(void)
{
    /* \u0041 = 'A' */
    const char *json = "{\"x\":\"\\u0041\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    /* Look up via DOM */
    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    TEST_ASSERT_EQUAL_INT(1, val.data.string_val.str_len);
    TEST_ASSERT_EQUAL_CHAR('A', val.data.string_val.str[0]);

    tp_json_close(&j);
    free(buf);
}

void test_escaped_chars(void)
{
    const char *json = "{\"x\":\"a\\nb\\tc\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    /* "a\nb\tc" = 5 bytes */
    TEST_ASSERT_EQUAL_INT(5, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

void test_whitespace_in_json(void)
{
    const char *json = "  {  \"a\"  :  1  }  ";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "a", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT64(1, val.data.int_val);

    tp_json_close(&j);
    free(buf);
}

void test_empty_array(void)
{
    const char *json = "{\"arr\":[]}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    /* Empty arrays just produce no sub-keys, which is fine */
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    free(buf);
}

void test_large_number(void)
{
    const char *json = "{\"big\":9999999999999}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "big", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT64(9999999999999LL, val.data.int_val);

    tp_json_close(&j);
    free(buf);
}

/* ── Number exceeding INT64_MAX (uint path) ──────────────────────── */

void test_number_exceeds_int64_max(void)
{
    /* 9223372036854775808 = INT64_MAX + 1 */
    const char *json = "{\"big\":9223372036854775808}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "big", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(9223372036854775808ULL, val.data.uint_val);

    tp_json_close(&j);
    free(buf);
}

/* ── Float with exponent ─────────────────────────────────────────── */

void test_float_with_exponent(void)
{
    const char *json = "{\"x\":1e10}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT64, val.type);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 1e10, val.data.float64_val);

    tp_json_close(&j);
    free(buf);
}

void test_float_with_negative_exponent(void)
{
    const char *json = "{\"x\":2.5e-3}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT64, val.type);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0025, val.data.float64_val);

    tp_json_close(&j);
    free(buf);
}

void test_float_with_positive_exponent(void)
{
    const char *json = "{\"x\":3E+2}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT64, val.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 300.0, val.data.float64_val);

    tp_json_close(&j);
    free(buf);
}

/* ── UTF-8 multi-byte encoding ───────────────────────────────────── */

void test_unicode_2byte(void)
{
    /* \u00E9 = 'é' (2-byte UTF-8: 0xC3 0xA9) */
    const char *json = "{\"x\":\"caf\\u00E9\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    /* "café" = 3 ASCII + 2 UTF-8 = 5 bytes */
    TEST_ASSERT_EQUAL_INT(5, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

void test_unicode_3byte(void)
{
    /* \u4E16 = '世' (3-byte UTF-8: 0xE4 0xB8 0x96) */
    const char *json = "{\"x\":\"\\u4E16\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    TEST_ASSERT_EQUAL_INT(3, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

void test_unicode_surrogate_pair(void)
{
    /* \uD83D\uDE00 = U+1F600 '😀' (4-byte UTF-8) */
    const char *json = "{\"x\":\"\\uD83D\\uDE00\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    TEST_ASSERT_EQUAL_INT(4, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

void test_unicode_invalid_surrogate(void)
{
    /* High surrogate followed by non-low-surrogate */
    const char *json = "{\"x\":\"\\uD800\\u0041\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

void test_unicode_lone_high_surrogate(void)
{
    /* High surrogate at end of string */
    const char *json = "{\"x\":\"\\uD800\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

void test_invalid_escape_sequence(void)
{
    const char *json = "{\"x\":\"\\q\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Top-level array ─────────────────────────────────────────────── */

void test_top_level_array(void)
{
    const char *json = "[1,2,3]";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    /* Decode back and verify array elements */
    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "1"));
    TEST_ASSERT_NOT_NULL(strstr(out, "2"));
    TEST_ASSERT_NOT_NULL(strstr(out, "3"));

    free(out);
    free(buf);
}

void test_top_level_array_dom(void)
{
    const char *json = "[10,20]";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value_type type;
    rc = tp_json_root_type(j, &type);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_ARRAY, type);

    tp_value val;
    rc = tp_json_lookup_path(j, "[0]", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT64(10, val.data.int_val);

    tp_json_close(&j);
    free(buf);
}

/* ── Pretty-print with nested structure ──────────────────────────── */

void test_pretty_print_nested(void)
{
    const char *json = "{\"a\":{\"b\":1},\"c\":[2,3]}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode_pretty(buf, buf_len, "    ", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);
    /* Should have indentation */
    TEST_ASSERT_NOT_NULL(strstr(out, "    "));

    free(out);
    free(buf);
}

/* ── Decode null out_len ─────────────────────────────────────────── */

void test_decode_null_len(void)
{
    uint8_t dummy[4] = {0};
    char *out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode(dummy, 4, &out, NULL));
}

void test_decode_pretty_null_out(void)
{
    uint8_t dummy[4] = {0};
    size_t len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, "  ", NULL, &len));
}

void test_decode_pretty_null_len(void)
{
    uint8_t dummy[4] = {0};
    char *out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_json_decode_pretty(dummy, 4, "  ", &out, NULL));
}

/* ── Mixed types in arrays ───────────────────────────────────────── */

void test_array_mixed_types(void)
{
    const char *json = "{\"arr\":[1,\"hello\",true,null,3.14]}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hello\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "true"));
    TEST_ASSERT_NOT_NULL(strstr(out, "null"));

    free(out);
    free(buf);
}

/* ── Escaped characters round-trip ───────────────────────────────── */

void test_all_escape_sequences(void)
{
    /* Test \\, \/, \b, \f, \r */
    const char *json = "{\"x\":\"a\\\\b\\/c\\bd\\fe\\r\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);

    tp_json_close(&j);
    free(buf);
}

/* ── Trailing garbage after valid JSON ────────────────────────────── */

void test_trailing_garbage(void)
{
    /* JSON encoder parses valid JSON prefix and succeeds */
    const char *json = "{}garbage";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    /* The encoder accepts the valid prefix — this is by design */
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    free(buf);
}

/* ── Deeply nested arrays ────────────────────────────────────────── */

void test_deep_nested_arrays(void)
{
    char json[1024];
    size_t pos = 0;
    int depth = 40;

    for (int i = 0; i < depth; i++) {
        json[pos++] = '[';
    }
    json[pos++] = '1';
    for (int i = 0; i < depth; i++) {
        json[pos++] = ']';
    }
    json[pos] = '\0';

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, pos, &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_DEPTH, rc);
}

/* ── Truncated string mid-parse ──────────────────────────────────── */

void test_truncated_string(void)
{
    const char *json = "{\"key\":\"unterminated";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Backslash at end of string (covers json_encode.c line 119) ───── */

void test_backslash_at_end_of_input(void)
{
    const char *json = "{\"a\":\"\\";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Truncated \u escape (covers json_encode.c line 145) ──────────── */

void test_truncated_unicode_escape(void)
{
    const char *json = "{\"a\":\"\\u00\"}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Lowercase hex in \u escape (covers json_encode.c line 153) ───── */

void test_unicode_lowercase_hex(void)
{
    /* \u00ab = U+00AB (LEFT-POINTING DOUBLE ANGLE QUOTATION MARK) */
    const char *json = "{\"x\":\"\\u00ab\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    /* U+00AB is 2-byte UTF-8 */
    TEST_ASSERT_EQUAL_INT(2, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

/* ── Bad hex digit in \u escape (covers json_encode.c line 157) ───── */

void test_unicode_bad_hex_digit(void)
{
    const char *json = "{\"a\":\"\\u00gz\"}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Surrogate pair lowercase hex (covers json_encode.c line 171) ─── */

void test_surrogate_lowercase_hex(void)
{
    /* Same emoji but with lowercase hex in both surrogates */
    const char *json = "{\"x\":\"\\ud83d\\ude00\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "x", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    TEST_ASSERT_EQUAL_INT(4, val.data.string_val.str_len);

    tp_json_close(&j);
    free(buf);
}

/* ── Bad hex in low surrogate (covers json_encode.c line 175) ──────── */

void test_surrogate_bad_low_hex(void)
{
    const char *json = "{\"a\":\"\\uD800\\uDg00\"}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Negative sign with no digits (covers json_encode.c line 236) ─── */

void test_number_negative_no_digit(void)
{
    const char *json = "{\"a\":-}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Decimal point with no fractional digit (json_encode.c line 245) ─ */

void test_number_decimal_no_digit(void)
{
    const char *json = "{\"a\":1.}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Exponent sign with no digit (covers json_encode.c line 257) ──── */

void test_number_exponent_sign_no_digit(void)
{
    const char *json = "{\"a\":1e+}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Exponent with no digit (covers json_encode.c line 257) ─────── */

void test_number_exponent_no_digit(void)
{
    const char *json = "{\"a\":1e}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Very long number (covers json_encode.c line 266) ──────────────── */

void test_number_very_long(void)
{
    /* Build a number with 70+ digits to trigger the clamping path */
    char json[256];
    strcpy(json, "{\"x\":");
    size_t pos = 5;
    for (int i = 0; i < 70; i++)
        json[pos++] = '1';
    json[pos++] = '}';
    json[pos] = '\0';

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, pos, &buf, &buf_len);
    /* Should succeed (the number is truncated but still parseable) */
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    free(buf);
}

/* ── Unknown value character (covers json_encode.c line 433) ───────── */

void test_unknown_value_character(void)
{
    const char *json = "{\"a\":@}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Unicode \u with fewer than 4 hex digits remaining (json_encode.c:145) */

void test_unicode_escape_too_short(void)
{
    /* Pass truncated length so \u has fewer than 4 chars remaining.
       String: {"a":"\u00"} (12 chars). Pass len=10 so after \u,
       pos(8) + 4 = 12 > 10, triggering json_encode.c line 145. */
    const char *json = "{\"a\":\"\\u00\"}";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, 10, &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

/* ── Missing value in object (json_encode.c:385) ──────────────────── */

void test_missing_value_in_object(void)
{
    /* Input truncated right after colon — parse_value sees
       p->pos >= p->len (json_encode.c line 385) */
    const char *json = "{\"a\":";
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &len);
    TEST_ASSERT_EQUAL_INT(TP_ERR_JSON_SYNTAX, rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_null_json);
    RUN_TEST(test_encode_null_buf);
    RUN_TEST(test_encode_null_len);
    RUN_TEST(test_decode_null_buf);
    RUN_TEST(test_decode_null_out);
    RUN_TEST(test_decode_pretty_null_indent);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_just_whitespace);
    RUN_TEST(test_unclosed_brace);
    RUN_TEST(test_unclosed_bracket);
    RUN_TEST(test_missing_colon);
    RUN_TEST(test_missing_quotes);
    RUN_TEST(test_bare_value);
    RUN_TEST(test_deep_nesting);
    RUN_TEST(test_unicode_escape);
    RUN_TEST(test_escaped_chars);
    RUN_TEST(test_whitespace_in_json);
    RUN_TEST(test_empty_array);
    RUN_TEST(test_large_number);
    /* Coverage additions */
    RUN_TEST(test_number_exceeds_int64_max);
    RUN_TEST(test_float_with_exponent);
    RUN_TEST(test_float_with_negative_exponent);
    RUN_TEST(test_float_with_positive_exponent);
    RUN_TEST(test_unicode_2byte);
    RUN_TEST(test_unicode_3byte);
    RUN_TEST(test_unicode_surrogate_pair);
    RUN_TEST(test_unicode_invalid_surrogate);
    RUN_TEST(test_unicode_lone_high_surrogate);
    RUN_TEST(test_invalid_escape_sequence);
    RUN_TEST(test_top_level_array);
    RUN_TEST(test_top_level_array_dom);
    RUN_TEST(test_pretty_print_nested);
    RUN_TEST(test_decode_null_len);
    RUN_TEST(test_decode_pretty_null_out);
    RUN_TEST(test_decode_pretty_null_len);
    RUN_TEST(test_array_mixed_types);
    RUN_TEST(test_all_escape_sequences);
    RUN_TEST(test_trailing_garbage);
    RUN_TEST(test_deep_nested_arrays);
    RUN_TEST(test_truncated_string);
    /* Encode edge-case coverage */
    RUN_TEST(test_backslash_at_end_of_input);
    RUN_TEST(test_truncated_unicode_escape);
    RUN_TEST(test_unicode_lowercase_hex);
    RUN_TEST(test_unicode_bad_hex_digit);
    RUN_TEST(test_surrogate_lowercase_hex);
    RUN_TEST(test_surrogate_bad_low_hex);
    RUN_TEST(test_number_negative_no_digit);
    RUN_TEST(test_number_decimal_no_digit);
    RUN_TEST(test_number_exponent_sign_no_digit);
    RUN_TEST(test_number_exponent_no_digit);
    RUN_TEST(test_number_very_long);
    RUN_TEST(test_unknown_value_character);
    RUN_TEST(test_unicode_escape_too_short);
    RUN_TEST(test_missing_value_in_object);
    return UNITY_END();
}
