/**
 * @file test_json_dom.c
 * @brief DOM-style JSON access tests.
 */

#include "triepack/triepack_json.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Shared buffer for several tests */
static uint8_t *g_buf = NULL;
static size_t g_buf_len = 0;

static void encode_test_json(void)
{
    if (g_buf)
        return;
    const char *json = "{\"name\":\"triepack\",\"version\":1,\"nested\":{\"key\":\"val\"}}";
    tp_result rc = tp_json_encode(json, strlen(json), &g_buf, &g_buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
}

/* ── Tests ───────────────────────────────────────────────────────────── */

void test_open_close(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_result rc = tp_json_open(&j, g_buf, g_buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(j);

    rc = tp_json_close(&j);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NULL(j);
}

void test_lookup_string(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_value val;
    tp_result rc = tp_json_lookup_path(j, "name", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);
    TEST_ASSERT_EQUAL_INT(8, val.data.string_val.str_len);

    tp_json_close(&j);
}

void test_lookup_int(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_value val;
    tp_result rc = tp_json_lookup_path(j, "version", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_INT, val.type);
    TEST_ASSERT_EQUAL_INT64(1, val.data.int_val);

    tp_json_close(&j);
}

void test_lookup_nested(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_value val;
    tp_result rc = tp_json_lookup_path(j, "nested.key", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_STRING, val.type);

    tp_json_close(&j);
}

void test_root_type_object(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_value_type type;
    tp_result rc = tp_json_root_type(j, &type);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_DICT, type);

    tp_json_close(&j);
}

void test_count(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    uint32_t count = tp_json_count(j);
    /* 3 leaf keys (name, version, nested.key) stored in trie */
    TEST_ASSERT_GREATER_THAN(0, count);

    tp_json_close(&j);
}

void test_missing_path(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_value val;
    tp_result rc = tp_json_lookup_path(j, "nonexistent", &val);
    TEST_ASSERT_EQUAL_INT(TP_ERR_NOT_FOUND, rc);

    tp_json_close(&j);
}

void test_array_index_lookup(void)
{
    const char *json = "{\"items\":[10,20,30]}";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value val;
    rc = tp_json_lookup_path(j, "items[0]", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_INT, val.type);
    TEST_ASSERT_EQUAL_INT64(10, val.data.int_val);

    rc = tp_json_lookup_path(j, "items[2]", &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT64(30, val.data.int_val);

    tp_json_close(&j);
    free(buf);
}

void test_null_params(void)
{
    tp_result rc = tp_json_open(NULL, NULL, 0);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, rc);

    rc = tp_json_close(NULL);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, rc);

    rc = tp_json_lookup_path(NULL, "x", NULL);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, rc);

    tp_value_type type;
    rc = tp_json_root_type(NULL, &type);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, rc);

    TEST_ASSERT_EQUAL_UINT32(0, tp_json_count(NULL));
}

void test_iterate_null(void)
{
    tp_result rc = tp_json_iterate(NULL, NULL);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, rc);
}

void test_iterate_basic(void)
{
    encode_test_json();

    tp_json *j = NULL;
    tp_json_open(&j, g_buf, g_buf_len);

    tp_iterator *it = NULL;
    tp_result rc = tp_json_iterate(j, &it);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(it);

    tp_iter_destroy(&it);
    tp_json_close(&j);
}

void test_root_type_array(void)
{
    const char *json = "[10,20,30]";
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_json_encode(json, strlen(json), &buf, &buf_len);

    tp_json *j = NULL;
    tp_json_open(&j, buf, buf_len);

    tp_value_type type;
    tp_result rc = tp_json_root_type(j, &type);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_ARRAY, type);

    tp_json_close(&j);
    free(buf);
}

void test_open_corrupted_buffer(void)
{
    uint8_t bad[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tp_json *j = NULL;
    tp_result rc = tp_json_open(&j, bad, sizeof(bad));
    TEST_ASSERT_NOT_EQUAL(TP_OK, rc);
}

/* ── Root type stored as INT (covers the TP_INT branch in json_dom.c) ── */

void test_root_type_via_int_metadata(void)
{
    /* Build a .trp manually with root type stored as TP_INT instead of TP_UINT */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "x", &v);

    /* Store root type as INT (not UINT) to cover the else-if branch
       TP_JSON_META_ROOT = "\x01root", TP_JSON_ROOT_ARRAY = 2 */
    tp_value root_meta = tp_value_int(2);
    tp_encoder_add(enc, "\x01root", &root_meta);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_encoder_build(enc, &buf, &buf_len);
    tp_encoder_destroy(&enc);

    tp_json *j = NULL;
    tp_result rc = tp_json_open(&j, buf, buf_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_value_type type;
    rc = tp_json_root_type(j, &type);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(TP_ARRAY, type);

    tp_json_close(&j);
    free(buf);
}

/* ── Close with already-null pointer ───────────────────────────────── */

void test_close_already_null(void)
{
    tp_json *j = NULL;
    TEST_ASSERT_EQUAL_INT(TP_OK, tp_json_close(&j));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_close);
    RUN_TEST(test_lookup_string);
    RUN_TEST(test_lookup_int);
    RUN_TEST(test_lookup_nested);
    RUN_TEST(test_root_type_object);
    RUN_TEST(test_count);
    RUN_TEST(test_missing_path);
    RUN_TEST(test_array_index_lookup);
    RUN_TEST(test_null_params);
    RUN_TEST(test_iterate_null);
    RUN_TEST(test_iterate_basic);
    RUN_TEST(test_root_type_array);
    RUN_TEST(test_open_corrupted_buffer);
    RUN_TEST(test_root_type_via_int_metadata);
    RUN_TEST(test_close_already_null);

    free(g_buf);
    return UNITY_END();
}
