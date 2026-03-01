/**
 * @file test_bitstream_varint.c
 * @brief Tests for VarInt (LEB128 / zigzag) read/write operations.
 */

#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* Helper: write then read unsigned varint */
static void roundtrip_u(uint64_t value)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_varint_u(w, value));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t out;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &out));
    TEST_ASSERT_EQUAL_UINT64(value, out);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* Helper: write then read signed varint */
static void roundtrip_s(int64_t value)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_varint_s(w, value));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    int64_t out;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_s(r, &out));
    TEST_ASSERT_EQUAL_INT64(value, out);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Unsigned roundtrips ────────────────────────────────────────────── */

void test_varint_u_zero(void)
{
    roundtrip_u(0);
}
void test_varint_u_one(void)
{
    roundtrip_u(1);
}
void test_varint_u_127(void)
{
    roundtrip_u(127);
}
void test_varint_u_128(void)
{
    roundtrip_u(128);
}
void test_varint_u_16383(void)
{
    roundtrip_u(16383);
}
void test_varint_u_16384(void)
{
    roundtrip_u(16384);
}
void test_varint_u_large(void)
{
    roundtrip_u(0x7FFFFFFFFFFFFFFFULL);
}
void test_varint_u_max(void)
{
    roundtrip_u(UINT64_MAX);
}

/* ── Signed roundtrips ──────────────────────────────────────────────── */

void test_varint_s_zero(void)
{
    roundtrip_s(0);
}
void test_varint_s_one(void)
{
    roundtrip_s(1);
}
void test_varint_s_neg_one(void)
{
    roundtrip_s(-1);
}
void test_varint_s_63(void)
{
    roundtrip_s(63);
}
void test_varint_s_neg_64(void)
{
    roundtrip_s(-64);
}
void test_varint_s_64(void)
{
    roundtrip_s(64);
}
void test_varint_s_neg_65(void)
{
    roundtrip_s(-65);
}
void test_varint_s_min(void)
{
    roundtrip_s(INT64_MIN);
}
void test_varint_s_max(void)
{
    roundtrip_s(INT64_MAX);
}

/* ── Zigzag verification ────────────────────────────────────────────── */

void test_zigzag_mapping(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);

    /* Write signed, read as unsigned to verify zigzag mapping */
    tp_bs_write_varint_s(w, 0);  /* -> 0 */
    tp_bs_write_varint_s(w, -1); /* -> 1 */
    tp_bs_write_varint_s(w, 1);  /* -> 2 */
    tp_bs_write_varint_s(w, -2); /* -> 3 */

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t raw;
    tp_bs_read_varint_u(r, &raw);
    TEST_ASSERT_EQUAL_UINT64(0, raw);
    tp_bs_read_varint_u(r, &raw);
    TEST_ASSERT_EQUAL_UINT64(1, raw);
    tp_bs_read_varint_u(r, &raw);
    TEST_ASSERT_EQUAL_UINT64(2, raw);
    tp_bs_read_varint_u(r, &raw);
    TEST_ASSERT_EQUAL_UINT64(3, raw);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Multi-byte encoding size ───────────────────────────────────────── */

void test_varint_encoding_size(void)
{
    tp_bitstream_writer *w = NULL;

    /* 0 takes 1 byte (8 bits) */
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 0);
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);

    /* 127 takes 1 byte */
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 127);
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);

    /* 128 takes 2 bytes */
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 128);
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);

    /* 16383 takes 2 bytes */
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 16383);
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);

    /* 16384 takes 3 bytes */
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 16384);
    TEST_ASSERT_EQUAL_UINT64(24, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);
}

/* ── Overflow (too many continuation bytes) ─────────────────────────── */

void test_varint_overflow(void)
{
    /* Manually create a stream with 11 continuation bytes */
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    for (int i = 0; i < 11; i++) {
        tp_bs_write_u8(w, 0x80); /* continuation byte with value 0 */
    }

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_OVERFLOW, tp_bs_read_varint_u(r, &val));

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── EOF mid-varint ─────────────────────────────────────────────────── */

void test_varint_eof_mid(void)
{
    /* 1 continuation byte but no terminator */
    uint8_t data[] = {0x80};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 8);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_read_varint_u(r, &val));
    tp_bs_reader_destroy(&r);
}

/* ── NULL params ────────────────────────────────────────────────────── */

void test_varint_null_params(void)
{
    uint64_t uval;
    int64_t sval;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u(NULL, &uval));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_s(NULL, &sval));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_varint_u(NULL, 0));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_varint_s(NULL, 0));

    uint8_t buf[] = {0x00};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u(r, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_s(r, NULL));
    tp_bs_reader_destroy(&r);
}

/* ── Multiple varints in sequence ───────────────────────────────────── */

void test_varint_sequence(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 0);
    tp_bs_write_varint_u(w, 300);
    tp_bs_write_varint_s(w, -100);
    tp_bs_write_varint_u(w, 1000000);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    uint64_t uval;
    int64_t sval;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &uval));
    TEST_ASSERT_EQUAL_UINT64(0, uval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &uval));
    TEST_ASSERT_EQUAL_UINT64(300, uval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_s(r, &sval));
    TEST_ASSERT_EQUAL_INT64(-100, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &uval));
    TEST_ASSERT_EQUAL_UINT64(1000000, uval);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Non-aligned varint ─────────────────────────────────────────────── */

void test_varint_non_aligned(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0x05, 3); /* offset by 3 bits */
    tp_bs_write_varint_u(w, 300);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t v;
    tp_bs_read_bits(r, 3, &v);
    TEST_ASSERT_EQUAL_UINT64(0x05, v);
    uint64_t uval;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &uval));
    TEST_ASSERT_EQUAL_UINT64(300, uval);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

int main(void)
{
    UNITY_BEGIN();
    /* Unsigned roundtrips */
    RUN_TEST(test_varint_u_zero);
    RUN_TEST(test_varint_u_one);
    RUN_TEST(test_varint_u_127);
    RUN_TEST(test_varint_u_128);
    RUN_TEST(test_varint_u_16383);
    RUN_TEST(test_varint_u_16384);
    RUN_TEST(test_varint_u_large);
    RUN_TEST(test_varint_u_max);
    /* Signed roundtrips */
    RUN_TEST(test_varint_s_zero);
    RUN_TEST(test_varint_s_one);
    RUN_TEST(test_varint_s_neg_one);
    RUN_TEST(test_varint_s_63);
    RUN_TEST(test_varint_s_neg_64);
    RUN_TEST(test_varint_s_64);
    RUN_TEST(test_varint_s_neg_65);
    RUN_TEST(test_varint_s_min);
    RUN_TEST(test_varint_s_max);
    /* Zigzag */
    RUN_TEST(test_zigzag_mapping);
    /* Encoding sizes */
    RUN_TEST(test_varint_encoding_size);
    /* Error cases */
    RUN_TEST(test_varint_overflow);
    RUN_TEST(test_varint_eof_mid);
    RUN_TEST(test_varint_null_params);
    /* Sequences */
    RUN_TEST(test_varint_sequence);
    RUN_TEST(test_varint_non_aligned);
    return UNITY_END();
}
