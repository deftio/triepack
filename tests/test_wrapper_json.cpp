/**
 * @file test_wrapper_json.cpp
 * @brief C++ wrapper tests for JSON encode/decode via C API.
 */

extern "C" {
#include "triepack/triepack_json.h"
#include "unity.h"
}

#include "triepack/json.hpp"

#include <cstdlib>
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

/* ── Tests ───────────────────────────────────────────────────────────── */

void test_json_encode_c_api(void)
{
    const char *json = "{\"x\":1}";
    uint8_t *buf = nullptr;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, std::strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN(0, buf_len);
    std::free(buf);
}

void test_json_roundtrip_c_api(void)
{
    const char *json_in = "{\"a\":42,\"b\":\"hello\"}";
    uint8_t *buf = nullptr;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json_in, std::strlen(json_in), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    char *json_out = nullptr;
    size_t json_out_len = 0;
    rc = tp_json_decode(buf, buf_len, &json_out, &json_out_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(json_out);
    TEST_ASSERT_NOT_NULL(std::strstr(json_out, "42"));
    TEST_ASSERT_NOT_NULL(std::strstr(json_out, "\"hello\""));

    std::free(json_out);
    std::free(buf);
}

void test_json_dom_from_cpp(void)
{
    const char *json = "{\"key\":\"value\"}";
    uint8_t *buf = nullptr;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, std::strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = nullptr;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "key", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);

    tp_json_close(&j);
    std::free(buf);
}

void test_json_wrapper_move(void)
{
    triepack::Json j1;
    triepack::Json j2(static_cast<triepack::Json &&>(j1));
    /* Move should transfer handle (even if null initially) */
    TEST_ASSERT_NULL(j1.handle());
}

void test_json_wrapper_encode_decode(void)
{
    triepack::Json jw;
    const uint8_t *data = nullptr;
    size_t size = 0;
    int rc = jw.encode("{\"n\":100}", &data, &size);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(data);

    const char *json_out = nullptr;
    rc = jw.decode(data, size, &json_out);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(json_out);
    TEST_ASSERT_NOT_NULL(std::strstr(json_out, "100"));

    std::free(const_cast<uint8_t *>(data));
    std::free(const_cast<char *>(json_out));
}

/* ── Coverage additions ──────────────────────────────────────────── */

void test_json_wrapper_move_assign(void)
{
    triepack::Json j1;
    triepack::Json j2;
    j2 = static_cast<triepack::Json &&>(j1);
    TEST_ASSERT_NULL(j1.handle());
}

void test_json_wrapper_move_assign_self(void)
{
    triepack::Json j;
    triepack::Json &ref = j;
    j = static_cast<triepack::Json &&>(ref);
    /* Self-assign should not crash */
}

void test_json_encode_null_params(void)
{
    triepack::Json j;
    const uint8_t *data = nullptr;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, j.encode(nullptr, &data, &size));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, j.encode("{}", nullptr, &size));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, j.encode("{}", &data, nullptr));
}

void test_json_decode_null_params(void)
{
    triepack::Json j;
    const char *out = nullptr;
    uint8_t dummy[] = {0};
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, j.decode(nullptr, 1, &out));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, j.decode(dummy, 1, nullptr));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_json_encode_c_api);
    RUN_TEST(test_json_roundtrip_c_api);
    RUN_TEST(test_json_dom_from_cpp);
    RUN_TEST(test_json_wrapper_move);
    RUN_TEST(test_json_wrapper_encode_decode);
    /* Coverage additions */
    RUN_TEST(test_json_wrapper_move_assign);
    RUN_TEST(test_json_wrapper_move_assign_self);
    RUN_TEST(test_json_encode_null_params);
    RUN_TEST(test_json_decode_null_params);
    return UNITY_END();
}
