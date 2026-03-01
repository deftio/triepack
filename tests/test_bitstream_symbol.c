/**
 * @file test_bitstream_symbol.c
 * @brief Tests for fixed-width symbol and UTF-8 codepoint read/write.
 */

#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Fixed-width symbol roundtrips ──────────────────────────────────── */

void test_symbol_bps5(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    for (uint32_t i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_symbol(w, i, 5));
    }

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t out;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_symbol(r, 5, &out));
        TEST_ASSERT_EQUAL_UINT32(i, out);
    }

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_symbol_bps8(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_symbol(w, 0, 8);
    tp_bs_write_symbol(w, 255, 8);
    tp_bs_write_symbol(w, 128, 8);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t out;
    tp_bs_read_symbol(r, 8, &out);
    TEST_ASSERT_EQUAL_UINT32(0, out);
    tp_bs_read_symbol(r, 8, &out);
    TEST_ASSERT_EQUAL_UINT32(255, out);
    tp_bs_read_symbol(r, 8, &out);
    TEST_ASSERT_EQUAL_UINT32(128, out);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_symbol_bps1(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_symbol(w, 0, 1);
    tp_bs_write_symbol(w, 1, 1);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t out;
    tp_bs_read_symbol(r, 1, &out);
    TEST_ASSERT_EQUAL_UINT32(0, out);
    tp_bs_read_symbol(r, 1, &out);
    TEST_ASSERT_EQUAL_UINT32(1, out);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_symbol_bps32(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_symbol(w, 0xDEADBEEF, 32);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t out;
    tp_bs_read_symbol(r, 32, &out);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, out);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Symbol boundary errors ─────────────────────────────────────────── */

void test_symbol_bps0_error(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_symbol(w, 0, 0));
    tp_bs_writer_destroy(&w);

    uint8_t buf[] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    uint32_t out;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_symbol(r, 0, &out));
    tp_bs_reader_destroy(&r);
}

void test_symbol_bps33_error(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_symbol(w, 0, 33));
    tp_bs_writer_destroy(&w);

    uint8_t buf[8] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 64);
    uint32_t out;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_symbol(r, 33, &out));
    tp_bs_reader_destroy(&r);
}

/* ── UTF-8 1-byte (ASCII) ──────────────────────────────────────────── */

void test_utf8_ascii(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x00));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x41)); /* 'A' */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x7F));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x00, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x41, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x7F, cp);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 2-byte ───────────────────────────────────────────────────── */

void test_utf8_2byte(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x80));   /* min 2-byte */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x00E9)); /* e-acute */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x07FF)); /* max 2-byte */

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x80, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x00E9, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x07FF, cp);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 3-byte ───────────────────────────────────────────────────── */

void test_utf8_3byte(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x0800)); /* min 3-byte */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x4E16)); /* CJK: '世' */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0xFFFF)); /* max 3-byte */

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x0800, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x4E16, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0xFFFF, cp);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 4-byte ───────────────────────────────────────────────────── */

void test_utf8_4byte(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x10000));  /* min 4-byte */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x1F600));  /* grinning face */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_utf8(w, 0x10FFFF)); /* max unicode */

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x10000, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x1F600, cp);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_utf8(r, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x10FFFF, cp);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 reject overlong ──────────────────────────────────────────── */

void test_utf8_reject_overlong_2byte(void)
{
    /* 0xC0 0x80 = overlong encoding of U+0000 */
    uint8_t data[] = {0xC0, 0x80};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 16);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

void test_utf8_reject_overlong_3byte(void)
{
    /* 0xE0 0x80 0x80 = overlong encoding of U+0000 */
    uint8_t data[] = {0xE0, 0x80, 0x80};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 24);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

void test_utf8_reject_overlong_4byte(void)
{
    /* 0xF0 0x80 0x80 0x80 = overlong encoding of U+0000 */
    uint8_t data[] = {0xF0, 0x80, 0x80, 0x80};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 32);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

/* ── UTF-8 reject surrogates ────────────────────────────────────────── */

void test_utf8_reject_surrogate_d800(void)
{
    /* Write U+D800 surrogate manually as UTF-8 bytes: ED A0 80 */
    uint8_t data[] = {0xED, 0xA0, 0x80};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 24);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

void test_utf8_reject_surrogate_dfff(void)
{
    /* U+DFFF: ED BF BF */
    uint8_t data[] = {0xED, 0xBF, 0xBF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 24);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

void test_utf8_write_reject_surrogate(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_write_utf8(w, 0xD800));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_write_utf8(w, 0xDFFF));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_write_utf8(w, 0xDB00));
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 reject > 0x10FFFF ────────────────────────────────────────── */

void test_utf8_write_reject_too_large(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_write_utf8(w, 0x110000));
    tp_bs_writer_destroy(&w);
}

/* ── UTF-8 reject bad continuation ──────────────────────────────────── */

void test_utf8_bad_continuation(void)
{
    /* 2-byte start (0xC2) followed by non-continuation byte */
    uint8_t data[] = {0xC2, 0x00};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 16);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

/* ── UTF-8 reject bad lead byte ─────────────────────────────────────── */

void test_utf8_bad_lead_byte(void)
{
    uint8_t data_fe[] = {0xFE};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data_fe, 8);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);

    uint8_t data_ff[] = {0xFF};
    tp_bs_reader_create(&r, data_ff, 8);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

/* ── UTF-8 reject unexpected continuation ───────────────────────────── */

void test_utf8_unexpected_continuation(void)
{
    uint8_t data[] = {0x80}; /* standalone continuation byte */
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 8);
    uint32_t cp;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_UTF8, tp_bs_read_utf8(r, &cp));
    tp_bs_reader_destroy(&r);
}

/* ── NULL params ────────────────────────────────────────────────────── */

void test_symbol_null_params(void)
{
    uint32_t out;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_symbol(NULL, 5, &out));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_symbol(NULL, 0, 5));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_utf8(NULL, &out));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_utf8(NULL, 0));

    uint8_t buf[] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_symbol(r, 5, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_utf8(r, NULL));
    tp_bs_reader_destroy(&r);
}

int main(void)
{
    UNITY_BEGIN();
    /* Fixed-width symbols */
    RUN_TEST(test_symbol_bps5);
    RUN_TEST(test_symbol_bps8);
    RUN_TEST(test_symbol_bps1);
    RUN_TEST(test_symbol_bps32);
    RUN_TEST(test_symbol_bps0_error);
    RUN_TEST(test_symbol_bps33_error);
    /* UTF-8 byte ranges */
    RUN_TEST(test_utf8_ascii);
    RUN_TEST(test_utf8_2byte);
    RUN_TEST(test_utf8_3byte);
    RUN_TEST(test_utf8_4byte);
    /* UTF-8 rejection */
    RUN_TEST(test_utf8_reject_overlong_2byte);
    RUN_TEST(test_utf8_reject_overlong_3byte);
    RUN_TEST(test_utf8_reject_overlong_4byte);
    RUN_TEST(test_utf8_reject_surrogate_d800);
    RUN_TEST(test_utf8_reject_surrogate_dfff);
    RUN_TEST(test_utf8_write_reject_surrogate);
    RUN_TEST(test_utf8_write_reject_too_large);
    RUN_TEST(test_utf8_bad_continuation);
    RUN_TEST(test_utf8_bad_lead_byte);
    RUN_TEST(test_utf8_unexpected_continuation);
    /* NULL params */
    RUN_TEST(test_symbol_null_params);
    return UNITY_END();
}
