/**
 * @file test_bitstream_bits.c
 * @brief Tests for bitstream lifecycle, cursor, bit-level read/write, and
 *        buffer access operations.
 */

#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Reader lifecycle ───────────────────────────────────────────────── */

void test_reader_create(void)
{
    uint8_t buf[] = {0xAB, 0xCD};
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create(&r, buf, 16));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_reader_length(r));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(r));
    tp_bs_reader_destroy(&r);
    TEST_ASSERT_NULL(r);
}

void test_reader_create_null_out(void)
{
    uint8_t buf[] = {0};
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_create(NULL, buf, 8));
}

void test_reader_create_copy(void)
{
    uint8_t buf[] = {0xFF, 0x00};
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create_copy(&r, buf, 16));
    TEST_ASSERT_NOT_NULL(r);
    buf[0] = 0x00;
    uint8_t bit;
    tp_bs_read_bit(r, &bit);
    TEST_ASSERT_EQUAL_UINT8(1, bit);
    tp_bs_reader_destroy(&r);
}

void test_reader_create_copy_null_out(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_create_copy(NULL, NULL, 0));
}

void test_reader_destroy_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_destroy(NULL));
}

void test_reader_destroy_already_null(void)
{
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_destroy(&r));
}

/* ── Writer lifecycle ───────────────────────────────────────────────── */

void test_writer_create_default(void)
{
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_create(&w, 0, 0));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);
    TEST_ASSERT_NULL(w);
}

void test_writer_create_custom_cap(void)
{
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_create(&w, 64, 32));
    TEST_ASSERT_NOT_NULL(w);
    tp_bs_writer_destroy(&w);
}

void test_writer_create_null_out(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_create(NULL, 0, 0));
}

void test_writer_destroy_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_destroy(NULL));
}

void test_writer_destroy_already_null(void)
{
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_destroy(&w));
}

/* ── Reader cursor ──────────────────────────────────────────────────── */

void test_reader_position_remaining_length(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(r));
    TEST_ASSERT_EQUAL_UINT64(32, tp_bs_reader_remaining(r));
    TEST_ASSERT_EQUAL_UINT64(32, tp_bs_reader_length(r));
    tp_bs_reader_advance(r, 10);
    TEST_ASSERT_EQUAL_UINT64(10, tp_bs_reader_position(r));
    TEST_ASSERT_EQUAL_UINT64(22, tp_bs_reader_remaining(r));
    tp_bs_reader_destroy(&r);
}

void test_reader_null_cursor(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_remaining(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_length(NULL));
}

void test_reader_seek(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_seek(r, 16));
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_reader_position(r));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_seek(r, 32));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_POSITION, tp_bs_reader_seek(r, 33));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_seek(NULL, 0));
    tp_bs_reader_destroy(&r);
}

void test_reader_advance(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_advance(r, 16));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_advance(r, 16));
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_reader_advance(r, 1));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_advance(NULL, 0));
    tp_bs_reader_destroy(&r);
}

void test_reader_align_to_byte(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    TEST_ASSERT_TRUE(tp_bs_reader_is_byte_aligned(r));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_align_to_byte(r));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(r));
    tp_bs_reader_advance(r, 3);
    TEST_ASSERT_FALSE(tp_bs_reader_is_byte_aligned(r));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_align_to_byte(r));
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_reader_position(r));
    TEST_ASSERT_TRUE(tp_bs_reader_is_byte_aligned(r));

    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_reader_align_to_byte(NULL));
    TEST_ASSERT_FALSE(tp_bs_reader_is_byte_aligned(NULL));
    tp_bs_reader_destroy(&r);
}

/* ── Bit read/write ─────────────────────────────────────────────────── */

void test_write_read_single_bit(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bit(w, 1));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bit(w, 0));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bit(w, 1));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint8_t bit;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bit(r, &bit));
    TEST_ASSERT_EQUAL_UINT8(1, bit);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bit(r, &bit));
    TEST_ASSERT_EQUAL_UINT8(0, bit);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bit(r, &bit));
    TEST_ASSERT_EQUAL_UINT8(1, bit);
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_read_bit(r, &bit));

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_write_read_bits_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0x05, 3));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0xFF, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0x01, 1));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0x00, 4));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0xDEAD, 16));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 3, &val));
    TEST_ASSERT_EQUAL_UINT64(0x05, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 8, &val));
    TEST_ASSERT_EQUAL_UINT64(0xFF, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 1, &val));
    TEST_ASSERT_EQUAL_UINT64(0x01, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 4, &val));
    TEST_ASSERT_EQUAL_UINT64(0x00, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 16, &val));
    TEST_ASSERT_EQUAL_UINT64(0xDEAD, val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_write_read_64_bits(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    uint64_t big = 0xDEADBEEFCAFEBABEULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, big, 64));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 64, &val));
    TEST_ASSERT_EQUAL_UINT64(big, val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_read_bits_signed(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0xFF, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0x0D, 4));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0x05, 4));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    int64_t sval;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 8, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 4, &sval));
    TEST_ASSERT_EQUAL_INT64(-3, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 4, &sval));
    TEST_ASSERT_EQUAL_INT64(5, sval);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_write_read_bits_signed_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);

    /* 5-bit signed: range -16 to +15 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -3, 5));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 7, 5));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -1, 5));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 0, 5));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -16, 5));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 15, 5));

    /* 8-bit signed: range -128 to +127 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -128, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 127, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -1, 8));

    /* 1-bit signed: range -1 to 0 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 0, 1));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -1, 1));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    int64_t sval;

    /* 5-bit round-trips */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(-3, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(7, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(0, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(-16, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 5, &sval));
    TEST_ASSERT_EQUAL_INT64(15, sval);

    /* 8-bit round-trips */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 8, &sval));
    TEST_ASSERT_EQUAL_INT64(-128, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 8, &sval));
    TEST_ASSERT_EQUAL_INT64(127, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 8, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);

    /* 1-bit round-trips */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 1, &sval));
    TEST_ASSERT_EQUAL_INT64(0, sval);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, 1, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_write_bits_signed_null(void)
{
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits_signed(NULL, 0, 1));

    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits_signed(w, 0, 0));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits_signed(w, 0, 65));
    tp_bs_writer_destroy(&w);
}

void test_peek_bits(void)
{
    uint8_t buf[] = {0xAB};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_peek_bits(r, 4, &val));
    TEST_ASSERT_EQUAL_UINT64(0x0A, val);
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(r));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 4, &val));
    TEST_ASSERT_EQUAL_UINT64(0x0A, val);
    TEST_ASSERT_EQUAL_UINT64(4, tp_bs_reader_position(r));

    tp_bs_reader_destroy(&r);
}

void test_read_bits32(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0xDEADBEEF, 32);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t val32;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits32(r, 32, &val32));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, val32);

    tp_bs_reader_seek(r, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits32(r, 33, &val32));

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Edge cases ─────────────────────────────────────────────────────── */

void test_read_bits_zero(void)
{
    uint8_t buf[] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits(r, 0, &val));
    tp_bs_reader_destroy(&r);
}

void test_read_bits_over_64(void)
{
    uint8_t buf[16] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 128);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits(r, 65, &val));
    tp_bs_reader_destroy(&r);
}

void test_write_bits_zero(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits(w, 0, 0));
    tp_bs_writer_destroy(&w);
}

void test_write_bits_over_64(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits(w, 0, 65));
    tp_bs_writer_destroy(&w);
}

void test_read_eof(void)
{
    uint8_t buf[] = {0xAB};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    tp_bs_reader_advance(r, 8);
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_read_bits(r, 1, &val));
    tp_bs_reader_destroy(&r);
}

void test_null_params(void)
{
    uint64_t val;
    int64_t sval;
    uint8_t bit;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits(NULL, 1, &val));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed(NULL, 1, &sval));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bit(NULL, &bit));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_peek_bits(NULL, 1, &val));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bits(NULL, 0, 1));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bit(NULL, 0));
}

/* ── Writer alignment ───────────────────────────────────────────────── */

void test_writer_align_to_byte(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0x07, 3);
    TEST_ASSERT_EQUAL_UINT64(3, tp_bs_writer_position(w));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_align_to_byte(w));
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_writer_position(w));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_align_to_byte(w));
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_writer_position(w));

    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_align_to_byte(NULL));
    tp_bs_writer_destroy(&w);
}

/* ── Buffer access ──────────────────────────────────────────────────── */

void test_writer_get_buffer(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0xFF, 8);

    const uint8_t *buf;
    uint64_t bit_len;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_get_buffer(w, &buf, &bit_len));
    TEST_ASSERT_EQUAL_UINT64(8, bit_len);
    TEST_ASSERT_EQUAL_UINT8(0xFF, buf[0]);

    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_get_buffer(NULL, &buf, &bit_len));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_get_buffer(w, NULL, &bit_len));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_writer_get_buffer(w, &buf, NULL));

    tp_bs_writer_destroy(&w);
}

void test_writer_detach_buffer(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0xABCD, 16);

    uint8_t *buf;
    size_t byte_len;
    uint64_t bit_len;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_detach_buffer(w, &buf, &byte_len, &bit_len));
    TEST_ASSERT_EQUAL_UINT64(16, bit_len);
    TEST_ASSERT_EQUAL(2, (int)byte_len);
    TEST_ASSERT_EQUAL_UINT8(0xAB, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0xCD, buf[1]);
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(w));
    free(buf);
    tp_bs_writer_destroy(&w);
}

void test_writer_to_reader(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0xAB, 8);

    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_to_reader(w, &r));
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 8, &val));
    TEST_ASSERT_EQUAL_UINT64(0xAB, val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_reader_get_buffer(void)
{
    uint8_t data[] = {0xDE, 0xAD};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 16);

    const uint8_t *buf;
    uint64_t bit_len;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_get_buffer(r, &buf, &bit_len));
    TEST_ASSERT_EQUAL_UINT64(16, bit_len);
    TEST_ASSERT_EQUAL_UINT8(0xDE, buf[0]);

    tp_bs_reader_destroy(&r);
}

/* ── Copy bits ──────────────────────────────────────────────────────── */

void test_copy_bits(void)
{
    tp_bitstream_writer *w1 = NULL;
    tp_bs_writer_create(&w1, 0, 0);
    tp_bs_write_bits(w1, 0xABCD, 16);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w1, &r);

    tp_bitstream_writer *w2 = NULL;
    tp_bs_writer_create(&w2, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_copy_bits(r, w2, 12));
    TEST_ASSERT_EQUAL_UINT64(12, tp_bs_writer_position(w2));

    tp_bitstream_reader *r2 = NULL;
    tp_bs_writer_to_reader(w2, &r2);
    uint64_t val;
    tp_bs_read_bits(r2, 12, &val);
    TEST_ASSERT_EQUAL_UINT64(0xABC, val);

    tp_bs_reader_destroy(&r2);
    tp_bs_writer_destroy(&w2);
    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w1);
}

/* ── Append buffer ──────────────────────────────────────────────────── */

void test_writer_append_buffer(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0x0F, 4);

    uint8_t src[] = {0xA0};
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_append_buffer(w, src, 4));
    TEST_ASSERT_EQUAL_UINT64(8, tp_bs_writer_position(w));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t val;
    tp_bs_read_bits(r, 8, &val);
    TEST_ASSERT_EQUAL_UINT64(0xFA, val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Result string ──────────────────────────────────────────────────── */

void test_result_str(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", tp_result_str(TP_OK));
    TEST_ASSERT_EQUAL_STRING("read past end of stream", tp_result_str(TP_ERR_EOF));
    TEST_ASSERT_EQUAL_STRING("invalid parameter", tp_result_str(TP_ERR_INVALID_PARAM));
    TEST_ASSERT_EQUAL_STRING("unknown error", tp_result_str((tp_result)999));
}

/* ── Bit order ──────────────────────────────────────────────────────── */

void test_reader_set_bit_order(void)
{
    uint8_t buf[] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_set_bit_order(r, TP_BIT_ORDER_LSB_FIRST));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM,
                      tp_bs_reader_set_bit_order(NULL, TP_BIT_ORDER_MSB_FIRST));
    tp_bs_reader_destroy(&r);
}

/* ── Writer position NULL ───────────────────────────────────────────── */

void test_writer_position_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(NULL));
}

/* ── Weird bit widths (unsigned) ────────────────────────────────────── */

/**
 * Round-trip test for unusual unsigned bit widths: 1, 2, 3, 7, 9, 13,
 * 17, 23, 31, 33, 48, 63, 64.  For each width, write the maximum
 * representable value and a smaller known value, then read them back.
 */
void test_unsigned_weird_widths(void)
{
    static const unsigned int widths[] = {1, 2, 3, 7, 9, 13, 17, 23, 31, 33, 48, 63, 64};
    static const size_t n_widths = sizeof(widths) / sizeof(widths[0]);

    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 256, 0);

    /* Write max-value for each width, then a "small" known value (42 if
     * it fits, else 1). */
    for (size_t i = 0; i < n_widths; i++) {
        unsigned int bits = widths[i];
        uint64_t max_val;
        if (bits == 64)
            max_val = UINT64_MAX;
        else
            max_val = ((uint64_t)1 << bits) - 1;

        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, max_val, bits));

        uint64_t small_val = (bits >= 6) ? 42 : 1;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, small_val, bits));
    }

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    for (size_t i = 0; i < n_widths; i++) {
        unsigned int bits = widths[i];
        uint64_t max_val;
        if (bits == 64)
            max_val = UINT64_MAX;
        else
            max_val = ((uint64_t)1 << bits) - 1;

        uint64_t val;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, bits, &val));
        TEST_ASSERT_EQUAL_UINT64(max_val, val);

        uint64_t small_val = (bits >= 6) ? 42 : 1;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, bits, &val));
        TEST_ASSERT_EQUAL_UINT64(small_val, val);
    }

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Weird bit widths (signed) ─────────────────────────────────────── */

/**
 * Round-trip test for unusual signed bit widths: 1, 2, 3, 7, 9, 13,
 * 17, 23, 31, 33, 48, 63, 64.  For each width, write:
 *   - the most-negative value  -(2^(n-1))
 *   - the most-positive value  2^(n-1)-1
 *   - zero
 *   - -1
 * then read them back with sign extension and verify.
 */
void test_signed_weird_widths(void)
{
    static const unsigned int widths[] = {1, 2, 3, 7, 9, 13, 17, 23, 31, 33, 48, 63, 64};
    static const size_t n_widths = sizeof(widths) / sizeof(widths[0]);

    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 512, 0);

    for (size_t i = 0; i < n_widths; i++) {
        unsigned int bits = widths[i];
        int64_t min_val, max_val;

        if (bits == 64) {
            min_val = INT64_MIN;
            max_val = INT64_MAX;
        } else {
            min_val = -((int64_t)1 << (bits - 1));
            max_val = ((int64_t)1 << (bits - 1)) - 1;
        }

        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, min_val, bits));
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, max_val, bits));
        if (bits > 1) {
            TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, 0, bits));
        }
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits_signed(w, -1, bits));
    }

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    for (size_t i = 0; i < n_widths; i++) {
        unsigned int bits = widths[i];
        int64_t min_val, max_val;

        if (bits == 64) {
            min_val = INT64_MIN;
            max_val = INT64_MAX;
        } else {
            min_val = -((int64_t)1 << (bits - 1));
            max_val = ((int64_t)1 << (bits - 1)) - 1;
        }

        int64_t val;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, bits, &val));
        TEST_ASSERT_EQUAL_INT64(min_val, val);

        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, bits, &val));
        TEST_ASSERT_EQUAL_INT64(max_val, val);

        if (bits > 1) {
            TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, bits, &val));
            TEST_ASSERT_EQUAL_INT64(0, val);
        }

        TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed(r, bits, &val));
        TEST_ASSERT_EQUAL_INT64(-1, val);
    }

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Mixed-width sequential writes ─────────────────────────────────── */

/**
 * Write a sequence of values at different widths back-to-back, then
 * read them all back. This tests that non-byte-aligned boundaries
 * work correctly when width changes between writes.
 */
void test_mixed_width_sequence(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 256, 0);

    /* Write an interleaved sequence of unsigned and signed fields at
     * various widths.  The total bit count is deliberately not a
     * multiple of 8. */
    tp_bs_write_bits(w, 0x07, 3);          /*  3 bits  unsigned  7 */
    tp_bs_write_bits_signed(w, -5, 4);     /*  4 bits  signed   -5 */
    tp_bs_write_bits(w, 0x1234, 13);       /* 13 bits  unsigned  */
    tp_bs_write_bits_signed(w, -1000, 17); /* 17 bits  signed    */
    tp_bs_write_bits(w, 1, 1);             /*  1 bit   unsigned  */
    tp_bs_write_bits_signed(w, 0, 9);      /*  9 bits  signed    */
    tp_bs_write_bits(w, 0xDEAD, 16);       /* 16 bits  unsigned  */
    /* Total: 3+4+13+17+1+9+16 = 63 bits */

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    uint64_t u;
    int64_t s;

    tp_bs_read_bits(r, 3, &u);
    TEST_ASSERT_EQUAL_UINT64(0x07, u);

    tp_bs_read_bits_signed(r, 4, &s);
    TEST_ASSERT_EQUAL_INT64(-5, s);

    tp_bs_read_bits(r, 13, &u);
    TEST_ASSERT_EQUAL_UINT64(0x1234, u);

    tp_bs_read_bits_signed(r, 17, &s);
    TEST_ASSERT_EQUAL_INT64(-1000, s);

    tp_bs_read_bits(r, 1, &u);
    TEST_ASSERT_EQUAL_UINT64(1, u);

    tp_bs_read_bits_signed(r, 9, &s);
    TEST_ASSERT_EQUAL_INT64(0, s);

    tp_bs_read_bits(r, 16, &u);
    TEST_ASSERT_EQUAL_UINT64(0xDEAD, u);

    TEST_ASSERT_EQUAL_UINT64(63, tp_bs_reader_position(r));

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Writer growth ──────────────────────────────────────────────────── */

void test_writer_growth(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 4, 4);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bits(w, 0xFF, 8));
    }
    TEST_ASSERT_EQUAL_UINT64(512, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reader_create);
    RUN_TEST(test_reader_create_null_out);
    RUN_TEST(test_reader_create_copy);
    RUN_TEST(test_reader_create_copy_null_out);
    RUN_TEST(test_reader_destroy_null);
    RUN_TEST(test_reader_destroy_already_null);
    RUN_TEST(test_writer_create_default);
    RUN_TEST(test_writer_create_custom_cap);
    RUN_TEST(test_writer_create_null_out);
    RUN_TEST(test_writer_destroy_null);
    RUN_TEST(test_writer_destroy_already_null);
    RUN_TEST(test_reader_position_remaining_length);
    RUN_TEST(test_reader_null_cursor);
    RUN_TEST(test_reader_seek);
    RUN_TEST(test_reader_advance);
    RUN_TEST(test_reader_align_to_byte);
    RUN_TEST(test_write_read_single_bit);
    RUN_TEST(test_write_read_bits_roundtrip);
    RUN_TEST(test_write_read_64_bits);
    RUN_TEST(test_read_bits_signed);
    RUN_TEST(test_write_read_bits_signed_roundtrip);
    RUN_TEST(test_write_bits_signed_null);
    RUN_TEST(test_peek_bits);
    RUN_TEST(test_read_bits32);
    RUN_TEST(test_read_bits_zero);
    RUN_TEST(test_read_bits_over_64);
    RUN_TEST(test_write_bits_zero);
    RUN_TEST(test_write_bits_over_64);
    RUN_TEST(test_read_eof);
    RUN_TEST(test_null_params);
    RUN_TEST(test_writer_align_to_byte);
    RUN_TEST(test_writer_get_buffer);
    RUN_TEST(test_writer_detach_buffer);
    RUN_TEST(test_writer_to_reader);
    RUN_TEST(test_reader_get_buffer);
    RUN_TEST(test_copy_bits);
    RUN_TEST(test_writer_append_buffer);
    RUN_TEST(test_result_str);
    RUN_TEST(test_reader_set_bit_order);
    RUN_TEST(test_writer_position_null);
    RUN_TEST(test_writer_growth);
    RUN_TEST(test_unsigned_weird_widths);
    RUN_TEST(test_signed_weird_widths);
    RUN_TEST(test_mixed_width_sequence);
    return UNITY_END();
}
