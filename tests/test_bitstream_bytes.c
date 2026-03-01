/**
 * @file test_bitstream_bytes.c
 * @brief Tests for byte-level read/write operations.
 */

#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Byte roundtrips ────────────────────────────────────────────────── */

void test_u8_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u8(w, 0x00));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u8(w, 0xFF));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u8(w, 0x42));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint8_t v;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u8(r, &v));
    TEST_ASSERT_EQUAL_UINT8(0x00, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u8(r, &v));
    TEST_ASSERT_EQUAL_UINT8(0xFF, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u8(r, &v));
    TEST_ASSERT_EQUAL_UINT8(0x42, v);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_u16_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u16(w, 0x0000));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u16(w, 0xFFFF));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u16(w, 0xABCD));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint16_t v;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u16(r, &v));
    TEST_ASSERT_EQUAL_UINT16(0x0000, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u16(r, &v));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u16(r, &v));
    TEST_ASSERT_EQUAL_UINT16(0xABCD, v);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_u32_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u32(w, 0x00000000));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u32(w, 0xFFFFFFFF));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u32(w, 0xDEADBEEF));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint32_t v;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u32(r, &v));
    TEST_ASSERT_EQUAL_UINT32(0x00000000, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u32(r, &v));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u32(r, &v));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, v);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_u64_roundtrip(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u64(w, 0));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u64(w, 0xFFFFFFFFFFFFFFFFULL));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u64(w, 0xDEADBEEFCAFEBABEULL));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t v;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u64(r, &v));
    TEST_ASSERT_EQUAL_UINT64(0, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u64(r, &v));
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, v);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u64(r, &v));
    TEST_ASSERT_EQUAL_UINT64(0xDEADBEEFCAFEBABEULL, v);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Multi-byte read/write ──────────────────────────────────────────── */

void test_read_write_bytes(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bytes(w, data, 4));

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint8_t out[4] = {0};
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bytes(r, out, 4));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, out, 4);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_read_write_bytes_empty(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_bytes(w, NULL, 0));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(w));
    tp_bs_writer_destroy(&w);
}

void test_write_bytes_null_buf(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_write_bytes(w, NULL, 5));
    tp_bs_writer_destroy(&w);
}

void test_read_bytes_null(void)
{
    uint8_t buf[] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, tp_bs_read_bytes(r, NULL, 1));
    tp_bs_reader_destroy(&r);
}

/* ── Direct pointer ─────────────────────────────────────────────────── */

void test_reader_direct_ptr(void)
{
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 32);

    const uint8_t *ptr;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_direct_ptr(r, &ptr, 2));
    TEST_ASSERT_EQUAL_UINT8(0xDE, ptr[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, ptr[1]);
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_reader_position(r));

    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_direct_ptr(r, &ptr, 2));
    TEST_ASSERT_EQUAL_UINT8(0xBE, ptr[0]);

    tp_bs_reader_destroy(&r);
}

void test_reader_direct_ptr_not_aligned(void)
{
    uint8_t data[] = {0xFF, 0x00};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 16);
    tp_bs_reader_advance(r, 3);

    const uint8_t *ptr;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_ALIGNED, tp_bs_reader_direct_ptr(r, &ptr, 1));

    tp_bs_reader_destroy(&r);
}

void test_reader_direct_ptr_eof(void)
{
    uint8_t data[] = {0xFF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 8);

    const uint8_t *ptr;
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_reader_direct_ptr(r, &ptr, 2));

    tp_bs_reader_destroy(&r);
}

/* ── Endianness: big-endian (MSB first) ─────────────────────────────── */

void test_big_endian_byte_order(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_u16(w, 0x0102);

    const uint8_t *buf;
    uint64_t bit_len;
    tp_bs_writer_get_buffer(w, &buf, &bit_len);
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buf[1]);

    tp_bs_writer_destroy(&w);
}

/* ── Mixed bit + byte ───────────────────────────────────────────────── */

void test_mixed_bit_and_byte(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0x07, 3);
    tp_bs_write_u8(w, 0xAB);
    tp_bs_write_bits(w, 0x05, 5);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t v;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 3, &v));
    TEST_ASSERT_EQUAL_UINT64(0x07, v);
    uint8_t u8;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u8(r, &u8));
    TEST_ASSERT_EQUAL_UINT8(0xAB, u8);
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 5, &v));
    TEST_ASSERT_EQUAL_UINT64(0x05, v);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_non_aligned_byte_read(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_bits(w, 0x01, 1);
    tp_bs_write_u8(w, 0xFF);
    tp_bs_write_u16(w, 0x1234);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    uint64_t v;
    tp_bs_read_bits(r, 1, &v);
    TEST_ASSERT_EQUAL_UINT64(1, v);
    uint8_t u8;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u8(r, &u8));
    TEST_ASSERT_EQUAL_UINT8(0xFF, u8);
    uint16_t u16;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_u16(r, &u16));
    TEST_ASSERT_EQUAL_UINT16(0x1234, u16);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_read_bytes_eof(void)
{
    uint8_t data[] = {0xFF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, data, 8);
    uint8_t out[4];
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_bs_read_bytes(r, out, 4));
    tp_bs_reader_destroy(&r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_u8_roundtrip);
    RUN_TEST(test_u16_roundtrip);
    RUN_TEST(test_u32_roundtrip);
    RUN_TEST(test_u64_roundtrip);
    RUN_TEST(test_read_write_bytes);
    RUN_TEST(test_read_write_bytes_empty);
    RUN_TEST(test_write_bytes_null_buf);
    RUN_TEST(test_read_bytes_null);
    RUN_TEST(test_reader_direct_ptr);
    RUN_TEST(test_reader_direct_ptr_not_aligned);
    RUN_TEST(test_reader_direct_ptr_eof);
    RUN_TEST(test_big_endian_byte_order);
    RUN_TEST(test_mixed_bit_and_byte);
    RUN_TEST(test_non_aligned_byte_read);
    RUN_TEST(test_read_bytes_eof);
    return UNITY_END();
}
