/* test_core_integrity.c — integrity and CRC-32 tests for triepack core */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── CRC-32 known test vectors ─────────────────────────────────────── */

/* We can't call tp_crc32 directly since it's internal, but we can test
   CRC through the encode/decode pipeline. */

void test_valid_crc_opens_successfully(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_corrupted_data_byte(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Flip a bit in the data area (after header, before CRC) */
    if (len > TP_HEADER_SIZE + 4) {
        buf[TP_HEADER_SIZE + 1] ^= 0x01;
    }

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_CORRUPT, tp_dict_open(&dict, buf, len));

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_corrupted_crc_bytes(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Corrupt the last 4 bytes (CRC) */
    buf[len - 1] ^= 0xFF;

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_ERR_CORRUPT, tp_dict_open(&dict, buf, len));

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_corrupted_header_magic(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_encoder_add(enc, "test", NULL);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Corrupt magic bytes */
    buf[2] = 0xFF;

    tp_dict *dict = NULL;
    /* Will fail on CRC (checked first) or magic */
    tp_result rc = tp_dict_open(&dict, buf, len);
    TEST_ASSERT_TRUE(rc == TP_ERR_BAD_MAGIC || rc == TP_ERR_CORRUPT);

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_unchecked_with_corrupted_data(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Corrupt data in the trie area */
    if (len > TP_HEADER_SIZE + 4) {
        buf[TP_HEADER_SIZE + 1] ^= 0x01;
    }

    /* open_unchecked should succeed (no CRC check) */
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open_unchecked(&dict, buf, len));
    /* But lookups may fail or return wrong data */

    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);
}

void test_truncated_at_various_points(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "test", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    tp_dict *dict = NULL;

    /* Truncated to 0 bytes */
    TEST_ASSERT_EQUAL(TP_ERR_TRUNCATED, tp_dict_open(&dict, buf, 0));

    /* Truncated to 16 bytes (half header) */
    TEST_ASSERT_EQUAL(TP_ERR_TRUNCATED, tp_dict_open(&dict, buf, 16));

    /* Truncated to exactly header size */
    TEST_ASSERT_EQUAL(TP_ERR_TRUNCATED, tp_dict_open(&dict, buf, 31));

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_multiple_builds_all_have_valid_crc(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    /* Build multiple dicts and verify they all open */
    for (int round = 0; round < 5; round++) {
        tp_encoder_reset(enc);
        char key[16];
        for (int i = 0; i < 10; i++) {
            snprintf(key, sizeof(key), "r%d_k%d", round, i);
            tp_value v = tp_value_int(round * 100 + i);
            tp_encoder_add(enc, key, &v);
        }

        uint8_t *buf = NULL;
        size_t len = 0;
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

        tp_dict *dict = NULL;
        TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
        TEST_ASSERT_EQUAL_UINT32(10, tp_dict_count(dict));

        tp_dict_close(&dict);
        free(buf);
    }

    tp_encoder_destroy(&enc);
}

void test_corrupted_single_bit_flip(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(42);
    tp_encoder_add(enc, "hello", &v);
    tp_encoder_add(enc, "world", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Try flipping each bit in the data (not header magic/version).
       The CRC check should catch all of them. */
    int corrupt_detected = 0;
    int total_flips = 0;

    /* Only flip bits in the data region (after header) up to CRC */
    size_t data_start = TP_HEADER_SIZE;
    size_t crc_start = len - 4;

    for (size_t byte = data_start; byte < crc_start && byte < data_start + 8; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            /* Make a copy */
            uint8_t *copy = malloc(len);
            memcpy(copy, buf, len);

            /* Flip one bit */
            copy[byte] ^= (uint8_t)(1 << bit);

            tp_dict *dict = NULL;
            tp_result rc = tp_dict_open(&dict, copy, len);
            total_flips++;
            if (rc != TP_OK) {
                corrupt_detected++;
            } else {
                tp_dict_close(&dict);
            }
            free(copy);
        }
    }

    /* CRC-32 should detect all single-bit errors */
    TEST_ASSERT_EQUAL(total_flips, corrupt_detected);

    tp_encoder_destroy(&enc);
    free(buf);
}

void test_empty_dict_crc(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));

    /* Valid CRC even for empty dict */
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));
    tp_dict_close(&dict);

    /* Corrupt it */
    if (len > 4) {
        buf[len - 2] ^= 0x42;
        tp_dict *dict2 = NULL;
        TEST_ASSERT_EQUAL(TP_ERR_CORRUPT, tp_dict_open(&dict2, buf, len));
    }

    tp_encoder_destroy(&enc);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_crc_opens_successfully);
    RUN_TEST(test_corrupted_data_byte);
    RUN_TEST(test_corrupted_crc_bytes);
    RUN_TEST(test_corrupted_header_magic);
    RUN_TEST(test_unchecked_with_corrupted_data);
    RUN_TEST(test_truncated_at_various_points);
    RUN_TEST(test_multiple_builds_all_have_valid_crc);
    RUN_TEST(test_corrupted_single_bit_flip);
    RUN_TEST(test_empty_dict_crc);
    return UNITY_END();
}
