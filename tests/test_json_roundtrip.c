/**
 * @file test_json_roundtrip.c
 * @brief JSON encode/decode round-trip tests.
 */

#include "triepack/triepack_json.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void assert_roundtrip_contains(const char *json_in, const char *expected_fragment)
{
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json_in, strlen(json_in), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(buf);

    char *json_out = NULL;
    size_t json_out_len = 0;
    rc = tp_json_decode(buf, buf_len, &json_out, &json_out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(json_out);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(json_out, expected_fragment),
                                 "Expected fragment not found in output");

    free(json_out);
    free(buf);
}

/* ── Tests ───────────────────────────────────────────────────────────── */

void test_simple_object(void)
{
    const char *json = "{\"a\":1,\"b\":\"hello\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN(0, buf_len);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(out);

    TEST_ASSERT_NOT_NULL(strstr(out, "\"a\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "1"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"b\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hello\""));

    free(out);
    free(buf);
}

void test_nested_object(void)
{
    assert_roundtrip_contains("{\"x\":{\"y\":2}}", "\"y\"");
    assert_roundtrip_contains("{\"x\":{\"y\":2}}", "2");
}

void test_array(void)
{
    const char *json = "{\"items\":[1,2,3]}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

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

void test_string_value(void)
{
    assert_roundtrip_contains("{\"name\":\"triepack\"}", "\"triepack\"");
}

void test_int_value(void)
{
    assert_roundtrip_contains("{\"count\":42}", "42");
}

void test_negative_int(void)
{
    assert_roundtrip_contains("{\"val\":-99}", "-99");
}

void test_float_value(void)
{
    assert_roundtrip_contains("{\"pi\":3.14}", "3.14");
}

void test_bool_true(void)
{
    assert_roundtrip_contains("{\"flag\":true}", "true");
}

void test_bool_false(void)
{
    assert_roundtrip_contains("{\"flag\":false}", "false");
}

void test_null_value(void)
{
    assert_roundtrip_contains("{\"empty\":null}", "null");
}

void test_empty_object(void)
{
    const char *json = "{}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "{}"));

    free(out);
    free(buf);
}

void test_all_value_types(void)
{
    const char *json = "{\"bt\":true,\"bf\":false,\"f\":1.5,\"i\":42,\"n\":null,\"s\":\"hello\"}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hello\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "42"));
    TEST_ASSERT_NOT_NULL(strstr(out, "1.5"));
    TEST_ASSERT_NOT_NULL(strstr(out, "true"));
    TEST_ASSERT_NOT_NULL(strstr(out, "false"));
    TEST_ASSERT_NOT_NULL(strstr(out, "null"));

    free(out);
    free(buf);
}

void test_pretty_print(void)
{
    const char *json = "{\"a\":1}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode_pretty(buf, buf_len, "  ", &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "\n"));

    free(out);
    free(buf);
}

void test_multiple_keys(void)
{
    const char *json = "{\"alpha\":1,\"beta\":2,\"gamma\":3}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *out = NULL;
    size_t out_len = 0;
    rc = tp_json_decode(buf, buf_len, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"alpha\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"beta\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"gamma\""));

    free(out);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_simple_object);
    RUN_TEST(test_nested_object);
    RUN_TEST(test_array);
    RUN_TEST(test_string_value);
    RUN_TEST(test_int_value);
    RUN_TEST(test_negative_int);
    RUN_TEST(test_float_value);
    RUN_TEST(test_bool_true);
    RUN_TEST(test_bool_false);
    RUN_TEST(test_null_value);
    RUN_TEST(test_empty_object);
    RUN_TEST(test_all_value_types);
    RUN_TEST(test_pretty_print);
    RUN_TEST(test_multiple_keys);
    return UNITY_END();
}
