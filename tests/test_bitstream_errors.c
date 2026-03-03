/**
 * @file test_bitstream_errors.c
 * @brief Tests for bitstream.c error paths.
 */

#include "triepack/triepack_bitstream.h"
#include "triepack/triepack_common.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── tp_result_str coverage ─────────────────────────────────────────── */

void test_result_str_all_codes(void)
{
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_OK));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_EOF));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_ALLOC));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_INVALID_PARAM));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_INVALID_POSITION));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_NOT_ALIGNED));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_OVERFLOW));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_INVALID_UTF8));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_BAD_MAGIC));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_VERSION));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_CORRUPT));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_NOT_FOUND));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_TRUNCATED));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_JSON_SYNTAX));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_JSON_DEPTH));
    TEST_ASSERT_NOT_NULL(tp_result_str(TP_ERR_JSON_TYPE));
    /* Unknown code → should still return a string */
    TEST_ASSERT_NOT_NULL(tp_result_str((tp_result)9999));
}

/* ── Writer/reader create with NULL params ──────────────────────────── */

void test_writer_create_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_create(NULL, 0, 0));
}

void test_reader_create_null_out(void)
{
    uint8_t buf[1] = {0};
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_reader_create(NULL, buf, 8));
}

void test_reader_create_null_buf(void)
{
    /* API allows NULL buffer (creates reader with null internal buf) */
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL_INT(TP_OK, tp_bs_reader_create(&r, NULL, 8));
    tp_bs_reader_destroy(&r);
}

/* ── reader_create_copy NULL/zero ───────────────────────────────────── */

void test_reader_create_copy_null_out(void)
{
    uint8_t buf[1] = {0};
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_reader_create_copy(NULL, buf, 8));
}

void test_reader_create_copy_null_buf(void)
{
    /* API allows NULL buffer for create_copy (creates reader with zeroed buffer) */
    tp_bitstream_reader *r = NULL;
    tp_result rc = tp_bs_reader_create_copy(&r, NULL, 8);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    tp_bs_reader_destroy(&r);
}

/* ── Writer detach buffer ───────────────────────────────────────────── */

void test_writer_detach_buffer(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_bs_write_u8(w, 0x42);

    uint8_t *buf = NULL;
    size_t byte_len = 0;
    uint64_t bit_len = 0;
    tp_result rc = tp_bs_writer_detach_buffer(w, &buf, &byte_len, &bit_len);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_UINT8(0x42, buf[0]);
    TEST_ASSERT_EQUAL_UINT64(8, bit_len);

    /* Writer should be empty now */
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(w));

    free(buf);
    tp_bs_writer_destroy(&w);
}

void test_writer_detach_null_params(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);

    uint8_t *buf;
    size_t byte_len;
    uint64_t bit_len;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM,
                          tp_bs_writer_detach_buffer(NULL, &buf, &byte_len, &bit_len));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM,
                          tp_bs_writer_detach_buffer(w, NULL, &byte_len, &bit_len));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM,
                          tp_bs_writer_detach_buffer(w, &buf, NULL, &bit_len));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM,
                          tp_bs_writer_detach_buffer(w, &buf, &byte_len, NULL));

    tp_bs_writer_destroy(&w);
}

/* ── Read past EOF for each read function ───────────────────────────── */

void test_read_bit_eof(void)
{
    uint8_t buf[1] = {0xFF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 0); /* 0 bits available */
    uint8_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_bit(r, &out));
    tp_bs_reader_destroy(&r);
}

void test_read_bits_eof(void)
{
    uint8_t buf[1] = {0xFF};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 4); /* 4 bits available */
    uint64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_bits(r, 8, &out));
    tp_bs_reader_destroy(&r);
}

void test_read_u8_eof(void)
{
    uint8_t buf[1] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 4);
    uint8_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_u8(r, &out));
    tp_bs_reader_destroy(&r);
}

void test_read_u16_eof(void)
{
    uint8_t buf[1] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    uint16_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_u16(r, &out));
    tp_bs_reader_destroy(&r);
}

void test_read_u32_eof(void)
{
    uint8_t buf[2] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 16);
    uint32_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_u32(r, &out));
    tp_bs_reader_destroy(&r);
}

void test_read_u64_eof(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    uint64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_read_u64(r, &out));
    tp_bs_reader_destroy(&r);
}

/* ── Seek/advance past end ──────────────────────────────────────────── */

void test_seek_past_end(void)
{
    uint8_t buf[1] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_POSITION, tp_bs_reader_seek(r, 100));
    tp_bs_reader_destroy(&r);
}

void test_advance_past_end(void)
{
    uint8_t buf[1] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL_INT(TP_ERR_EOF, tp_bs_reader_advance(r, 100));
    tp_bs_reader_destroy(&r);
}

/* ── Destroy NULL ───────────────────────────────────────────────────── */

void test_writer_destroy_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_destroy(NULL));
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL_INT(TP_OK, tp_bs_writer_destroy(&w));
}

void test_reader_destroy_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_reader_destroy(NULL));
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL_INT(TP_OK, tp_bs_reader_destroy(&r));
}

/* ── Write NULL params ──────────────────────────────────────────────── */

void test_write_bits_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_bits(NULL, 0, 1));
}

void test_write_bit_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_bit(NULL, 0));
}

void test_write_u8_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_u8(NULL, 0));
}

void test_write_u16_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_u16(NULL, 0));
}

void test_write_u32_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_u32(NULL, 0));
}

void test_write_u64_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_u64(NULL, 0));
}

/* ── Read NULL params ───────────────────────────────────────────────── */

void test_read_bits_null_reader(void)
{
    uint64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bits(NULL, 1, &out));
}

void test_read_bits_null_out(void)
{
    uint8_t buf[1] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 8);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bits(r, 1, NULL));
    tp_bs_reader_destroy(&r);
}

void test_read_bit_null(void)
{
    uint8_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bit(NULL, &out));
}

/* ── Peek NULL ──────────────────────────────────────────────────────── */

void test_peek_null(void)
{
    uint64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_peek_bits(NULL, 1, &out));
}

/* ── Align to byte ──────────────────────────────────────────────────── */

void test_reader_align_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_reader_align_to_byte(NULL));
}

void test_writer_align_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_align_to_byte(NULL));
}

/* ── Writer get_buffer NULL ─────────────────────────────────────────── */

void test_writer_get_buffer_null(void)
{
    const uint8_t *buf;
    uint64_t bl;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_get_buffer(NULL, &buf, &bl));
}

/* ── Writer append_buffer NULL ──────────────────────────────────────── */

void test_writer_append_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_append_buffer(NULL, NULL, 0));
}

/* ── Writer to_reader NULL ──────────────────────────────────────────── */

void test_writer_to_reader_null(void)
{
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_writer_to_reader(NULL, &r));
}

/* ── Reader get_buffer NULL ─────────────────────────────────────────── */

void test_reader_get_buffer_null(void)
{
    const uint8_t *buf;
    uint64_t bl;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_reader_get_buffer(NULL, &buf, &bl));
}

/* ── Copy bits NULL ─────────────────────────────────────────────────── */

void test_copy_bits_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_copy_bits(NULL, NULL, 0));
}

/* ── Signed bit-level read/write ────────────────────────────────────── */

void test_read_bits_signed_null(void)
{
    int64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed(NULL, 8, &out));
}

void test_write_bits_signed_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_write_bits_signed(NULL, 0, 8));
}

/* ── Stateless ROM functions NULL ───────────────────────────────────── */

void test_read_bits_at_null(void)
{
    uint64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bits_at(NULL, 0, 8, &out));
}

void test_read_bits_signed_at_null(void)
{
    int64_t out;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_bits_signed_at(NULL, 0, 8, &out));
}

void test_read_varint_u_at_null(void)
{
    uint64_t out;
    unsigned int bits;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_bs_read_varint_u_at(NULL, 0, &out, &bits));
}

/* ── Set bit order NULL ─────────────────────────────────────────────── */

void test_reader_set_bit_order_null(void)
{
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM,
                          tp_bs_reader_set_bit_order(NULL, TP_BIT_ORDER_MSB_FIRST));
}

/* ── Reader position/remaining/length on NULL ───────────────────────── */

void test_reader_position_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(NULL));
}

void test_reader_remaining_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_remaining(NULL));
}

void test_reader_length_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_length(NULL));
}

void test_writer_position_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(NULL));
}

void test_reader_is_byte_aligned_null(void)
{
    /* API returns false for NULL reader */
    TEST_ASSERT_FALSE(tp_bs_reader_is_byte_aligned(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    /* tp_result_str */
    RUN_TEST(test_result_str_all_codes);
    /* Create NULL */
    RUN_TEST(test_writer_create_null);
    RUN_TEST(test_reader_create_null_out);
    RUN_TEST(test_reader_create_null_buf);
    RUN_TEST(test_reader_create_copy_null_out);
    RUN_TEST(test_reader_create_copy_null_buf);
    /* Detach buffer */
    RUN_TEST(test_writer_detach_buffer);
    RUN_TEST(test_writer_detach_null_params);
    /* EOF */
    RUN_TEST(test_read_bit_eof);
    RUN_TEST(test_read_bits_eof);
    RUN_TEST(test_read_u8_eof);
    RUN_TEST(test_read_u16_eof);
    RUN_TEST(test_read_u32_eof);
    RUN_TEST(test_read_u64_eof);
    /* Seek/advance past end */
    RUN_TEST(test_seek_past_end);
    RUN_TEST(test_advance_past_end);
    /* Destroy NULL */
    RUN_TEST(test_writer_destroy_null);
    RUN_TEST(test_reader_destroy_null);
    /* Write NULL */
    RUN_TEST(test_write_bits_null);
    RUN_TEST(test_write_bit_null);
    RUN_TEST(test_write_u8_null);
    RUN_TEST(test_write_u16_null);
    RUN_TEST(test_write_u32_null);
    RUN_TEST(test_write_u64_null);
    /* Read NULL */
    RUN_TEST(test_read_bits_null_reader);
    RUN_TEST(test_read_bits_null_out);
    RUN_TEST(test_read_bit_null);
    RUN_TEST(test_peek_null);
    /* Align NULL */
    RUN_TEST(test_reader_align_null);
    RUN_TEST(test_writer_align_null);
    /* Buffer access NULL */
    RUN_TEST(test_writer_get_buffer_null);
    RUN_TEST(test_writer_append_null);
    RUN_TEST(test_writer_to_reader_null);
    RUN_TEST(test_reader_get_buffer_null);
    RUN_TEST(test_copy_bits_null);
    /* Signed */
    RUN_TEST(test_read_bits_signed_null);
    RUN_TEST(test_write_bits_signed_null);
    /* ROM functions */
    RUN_TEST(test_read_bits_at_null);
    RUN_TEST(test_read_bits_signed_at_null);
    RUN_TEST(test_read_varint_u_at_null);
    /* Bit order */
    RUN_TEST(test_reader_set_bit_order_null);
    /* Position/remaining on NULL */
    RUN_TEST(test_reader_position_null);
    RUN_TEST(test_reader_remaining_null);
    RUN_TEST(test_reader_length_null);
    RUN_TEST(test_writer_position_null);
    RUN_TEST(test_reader_is_byte_aligned_null);
    return UNITY_END();
}
