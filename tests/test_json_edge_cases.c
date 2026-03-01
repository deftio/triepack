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
    return UNITY_END();
}
