/**
 * @file test_bitstream_rom.c
 * @brief Tests for stateless ROM functions (no object, no cursor).
 */

#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── read_bits_at ───────────────────────────────────────────────────── */

void test_read_bits_at_basic(void)
{
    uint8_t buf[] = {0xAB, 0xCD}; /* 10101011 11001101 */
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 0, 8, &val));
    TEST_ASSERT_EQUAL_UINT64(0xAB, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 8, 8, &val));
    TEST_ASSERT_EQUAL_UINT64(0xCD, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 0, 16, &val));
    TEST_ASSERT_EQUAL_UINT64(0xABCD, val);
}

void test_read_bits_at_cross_byte(void)
{
    uint8_t buf[] = {0xAB, 0xCD}; /* 10101011 11001101 */
    uint64_t val;
    /* Read 4 bits starting at bit 4: 1011 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 4, 4, &val));
    TEST_ASSERT_EQUAL_UINT64(0x0B, val);
    /* Read 8 bits starting at bit 4: 10111100 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 4, 8, &val));
    TEST_ASSERT_EQUAL_UINT64(0xBC, val);
}

void test_read_bits_at_single_bit(void)
{
    uint8_t buf[] = {0x80}; /* 10000000 */
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 0, 1, &val));
    TEST_ASSERT_EQUAL_UINT64(1, val);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 1, 1, &val));
    TEST_ASSERT_EQUAL_UINT64(0, val);
}

void test_read_bits_at_errors(void)
{
    uint64_t val;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_at(NULL, 0, 8, &val));
    uint8_t buf[] = {0};
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_at(buf, 0, 8, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_at(buf, 0, 0, &val));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_at(buf, 0, 65, &val));
}

/* ── read_bits_signed_at ────────────────────────────────────────────── */

void test_read_bits_signed_at(void)
{
    uint8_t buf[] = {0xFF}; /* 11111111 */
    int64_t sval;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed_at(buf, 0, 8, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);

    /* 4 bits from offset 0: 1111 = -1 in 4 bits */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed_at(buf, 0, 4, &sval));
    TEST_ASSERT_EQUAL_INT64(-1, sval);

    /* Positive: 0111 in 4 bits from byte 0x70 */
    uint8_t buf2[] = {0x70}; /* 01110000 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed_at(buf2, 0, 4, &sval));
    TEST_ASSERT_EQUAL_INT64(7, sval);
}

void test_read_bits_signed_at_errors(void)
{
    int64_t sval;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed_at(NULL, 0, 8, &sval));
    uint8_t buf[] = {0};
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed_at(buf, 0, 8, NULL));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed_at(buf, 0, 0, &sval));
}

/* ── read_varint_u_at ───────────────────────────────────────────────── */

void test_varint_u_at_single_byte(void)
{
    uint8_t buf[] = {42}; /* 0x2A = 42, no continuation */
    uint64_t val;
    unsigned int bits_read;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u_at(buf, 0, &val, &bits_read));
    TEST_ASSERT_EQUAL_UINT64(42, val);
    TEST_ASSERT_EQUAL_UINT(8, bits_read);
}

void test_varint_u_at_two_bytes(void)
{
    /* 300 = 0x012C. LEB128: 0xAC 0x02 */
    uint8_t buf[] = {0xAC, 0x02};
    uint64_t val;
    unsigned int bits_read;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u_at(buf, 0, &val, &bits_read));
    TEST_ASSERT_EQUAL_UINT64(300, val);
    TEST_ASSERT_EQUAL_UINT(16, bits_read);
}

void test_varint_u_at_offset(void)
{
    /* Put a varint at bit offset 8 (byte 1) */
    uint8_t buf[] = {0xFF, 42}; /* byte 0 = garbage, byte 1 = varint 42 */
    uint64_t val;
    unsigned int bits_read;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u_at(buf, 8, &val, &bits_read));
    TEST_ASSERT_EQUAL_UINT64(42, val);
    TEST_ASSERT_EQUAL_UINT(8, bits_read);
}

void test_varint_u_at_errors(void)
{
    uint64_t val;
    unsigned int bits_read;
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u_at(NULL, 0, &val, &bits_read));
    uint8_t buf[] = {0};
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u_at(buf, 0, NULL, &bits_read));
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u_at(buf, 0, &val, NULL));
}

/* ── Consistency: ROM vs reader ─────────────────────────────────────── */

void test_rom_matches_reader(void)
{
    /* Write some data and verify ROM reads match reader reads */
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0xABCD, 16);
    tp_bs_write_bits(w, 0x1234, 16);

    const uint8_t *buf;
    uint64_t bit_len;
    tp_bs_writer_get_buffer(w, &buf, &bit_len);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    /* Compare at various offsets */
    uint64_t rom_val, reader_val;

    tp_bs_read_bits_at(buf, 0, 16, &rom_val);
    tp_bs_read_bits(r, 16, &reader_val);
    TEST_ASSERT_EQUAL_UINT64(rom_val, reader_val);

    tp_bs_read_bits_at(buf, 16, 16, &rom_val);
    tp_bs_read_bits(r, 16, &reader_val);
    TEST_ASSERT_EQUAL_UINT64(rom_val, reader_val);

    /* Cross-byte at offset 4 */
    tp_bs_reader_seek(r, 4);
    tp_bs_read_bits_at(buf, 4, 12, &rom_val);
    tp_bs_read_bits(r, 12, &reader_val);
    TEST_ASSERT_EQUAL_UINT64(rom_val, reader_val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_rom_varint_matches_reader(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_varint_u(w, 300);

    const uint8_t *buf;
    uint64_t bit_len;
    tp_bs_writer_get_buffer(w, &buf, &bit_len);

    /* ROM read */
    uint64_t rom_val;
    unsigned int bits_read;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u_at(buf, 0, &rom_val, &bits_read));

    /* Reader read */
    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t reader_val;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_varint_u(r, &reader_val));

    TEST_ASSERT_EQUAL_UINT64(rom_val, reader_val);
    TEST_ASSERT_EQUAL_UINT64(tp_bs_reader_position(r), bits_read);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Various bit positions crossing 2-3 bytes ───────────────────────── */

void test_cross_byte_reads(void)
{
    uint8_t buf[] = {0xAB, 0xCD, 0xEF}; /* 10101011 11001101 11101111 */
    uint64_t val;

    /* 12 bits starting at bit 2: 101011 110011 = 0xAB3 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 2, 12, &val));
    TEST_ASSERT_EQUAL_UINT64(0xAF3, val);

    /* 16 bits starting at bit 4: 1011 11001101 1110 */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 4, 16, &val));
    TEST_ASSERT_EQUAL_UINT64(0xBCDE, val);
}

int main(void)
{
    UNITY_BEGIN();
    /* read_bits_at */
    RUN_TEST(test_read_bits_at_basic);
    RUN_TEST(test_read_bits_at_cross_byte);
    RUN_TEST(test_read_bits_at_single_bit);
    RUN_TEST(test_read_bits_at_errors);
    /* read_bits_signed_at */
    RUN_TEST(test_read_bits_signed_at);
    RUN_TEST(test_read_bits_signed_at_errors);
    /* read_varint_u_at */
    RUN_TEST(test_varint_u_at_single_byte);
    RUN_TEST(test_varint_u_at_two_bytes);
    RUN_TEST(test_varint_u_at_offset);
    RUN_TEST(test_varint_u_at_errors);
    /* Consistency */
    RUN_TEST(test_rom_matches_reader);
    RUN_TEST(test_rom_varint_matches_reader);
    /* Cross-byte */
    RUN_TEST(test_cross_byte_reads);
    return UNITY_END();
}
