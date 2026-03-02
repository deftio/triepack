/* test_cross_language.c — verify .trp fixture files for cross-language interop */
#include "triepack/triepack.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Helpers ───────────────────────────────────────────────────────── */

static uint8_t *read_fixture(const char *name, size_t *out_len)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/tests/fixtures/%s", FIXTURE_DIR, name);

    FILE *f = fopen(path, "rb");
    if (!f) {
        /* Try relative path */
        snprintf(path, sizeof(path), "tests/fixtures/%s", name);
        f = fopen(path, "rb");
    }
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (nread != (size_t)sz) {
        free(buf);
        return NULL;
    }
    *out_len = (size_t)sz;
    return buf;
}

/* ── Tests ─────────────────────────────────────────────────────────── */

void test_fixture_empty(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("empty.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read empty.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(0, tp_dict_count(dict));

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_single_null(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("single_null.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read single_null.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "hello", &val));
    TEST_ASSERT_EQUAL(TP_NULL, val.type);

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_single_int(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("single_int.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read single_int.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(1, tp_dict_count(dict));

    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "key", &val));
    TEST_ASSERT_EQUAL(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(42, val.data.uint_val);

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_multi_mixed(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("multi_mixed.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read multi_mixed.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(5, tp_dict_count(dict));

    tp_value val;

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "bool", &val));
    TEST_ASSERT_EQUAL(TP_BOOL, val.type);
    TEST_ASSERT_TRUE(val.data.bool_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "f64", &val));
    TEST_ASSERT_EQUAL(TP_FLOAT64, val.type);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.14159, val.data.float64_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "int", &val));
    TEST_ASSERT_EQUAL(TP_INT, val.type);
    TEST_ASSERT_EQUAL_INT64(-100, val.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "str", &val));
    TEST_ASSERT_EQUAL(TP_STRING, val.type);
    TEST_ASSERT_EQUAL(5, val.data.string_val.str_len);
    TEST_ASSERT_EQUAL_STRING_LEN("hello", val.data.string_val.str, 5);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "uint", &val));
    TEST_ASSERT_EQUAL(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(200, val.data.uint_val);

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_shared_prefix(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("shared_prefix.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read shared_prefix.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(3, tp_dict_count(dict));

    tp_value val;

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abc", &val));
    TEST_ASSERT_EQUAL(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(10, val.data.uint_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "abd", &val));
    TEST_ASSERT_EQUAL(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(20, val.data.uint_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, "xyz", &val));
    TEST_ASSERT_EQUAL(TP_UINT, val.type);
    TEST_ASSERT_EQUAL_UINT64(30, val.data.uint_val);

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_large(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("large.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read large.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(100, tp_dict_count(dict));

    /* Spot-check a few entries */
    for (int i = 0; i < 100; i += 17) {
        char key[16];
        snprintf(key, sizeof(key), "key_%04d", i);
        tp_value val;
        TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, key, &val));
        TEST_ASSERT_EQUAL(TP_UINT, val.type);
        TEST_ASSERT_EQUAL_UINT64((uint64_t)i, val.data.uint_val);
    }

    tp_dict_close(&dict);
    free(buf);
}

void test_fixture_keys_only(void)
{
    size_t len = 0;
    uint8_t *buf = read_fixture("keys_only.trp", &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Cannot read keys_only.trp");

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    TEST_ASSERT_EQUAL_UINT32(3, tp_dict_count(dict));

    bool found;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "apple", &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "banana", &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "cherry", &found));
    TEST_ASSERT_TRUE(found);

    TEST_ASSERT_EQUAL(TP_OK, tp_dict_contains(dict, "grape", &found));
    TEST_ASSERT_FALSE(found);

    tp_dict_close(&dict);
    free(buf);
}

/* ── Runner ────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixture_empty);
    RUN_TEST(test_fixture_single_null);
    RUN_TEST(test_fixture_single_int);
    RUN_TEST(test_fixture_multi_mixed);
    RUN_TEST(test_fixture_shared_prefix);
    RUN_TEST(test_fixture_large);
    RUN_TEST(test_fixture_keys_only);
    return UNITY_END();
}
