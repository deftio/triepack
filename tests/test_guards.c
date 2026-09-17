/* test_guards.c — parameter guards, one operand at a time.
 *
 * Most guards here are compound: `if (!r || !out || n == 0 || n > 64)`. A
 * single NULL argument proves the guard fires but leaves every other operand
 * unevaluated, so each one needs its own call. That is what this file is: one
 * case per operand, for every function that has such a guard.
 */
#include "triepack/triepack.h"
#include "triepack/triepack_bitstream.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define INVALID(expr) TEST_ASSERT_EQUAL(TP_ERR_INVALID_PARAM, (expr))

/* ── Stateless readers ─────────────────────────────────────────────── */

void test_read_bits_at_rejects_each_bad_argument(void)
{
    const uint8_t buf[8] = {0xFF, 0, 0, 0, 0, 0, 0, 0};
    uint64_t out = 0;

    INVALID(tp_bs_read_bits_at(NULL, 0, 8, &out)); /* no buffer */
    INVALID(tp_bs_read_bits_at(buf, 0, 8, NULL));  /* nowhere to put it */
    INVALID(tp_bs_read_bits_at(buf, 0, 0, &out));  /* zero width */
    INVALID(tp_bs_read_bits_at(buf, 0, 65, &out)); /* wider than a u64 */

    /* The boundaries themselves are fine. */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 0, 1, &out));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_at(buf, 0, 64, &out));
}

void test_read_bits_signed_at_rejects_each_bad_argument(void)
{
    const uint8_t buf[8] = {0xFF, 0, 0, 0, 0, 0, 0, 0};
    int64_t out = 0;

    INVALID(tp_bs_read_bits_signed_at(NULL, 0, 8, &out));
    INVALID(tp_bs_read_bits_signed_at(buf, 0, 8, NULL));
    INVALID(tp_bs_read_bits_signed_at(buf, 0, 0, &out));
    INVALID(tp_bs_read_bits_signed_at(buf, 0, 65, &out));

    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed_at(buf, 0, 1, &out));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits_signed_at(buf, 0, 64, &out));
}

/* ── Reader guards ─────────────────────────────────────────────────── */

void test_reader_bit_reads_reject_each_bad_argument(void)
{
    const uint8_t buf[8] = {0xAB, 0xCD, 0, 0, 0, 0, 0, 0};
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create(&r, buf, sizeof(buf) * 8));

    uint64_t u = 0;
    int64_t s = 0;
    uint32_t u32 = 0;
    uint8_t bit = 0;

    INVALID(tp_bs_read_bits(NULL, 8, &u));
    INVALID(tp_bs_read_bits(r, 8, NULL));
    INVALID(tp_bs_read_bits(r, 0, &u));
    INVALID(tp_bs_read_bits(r, 65, &u));

    INVALID(tp_bs_read_bits_signed(NULL, 8, &s));
    INVALID(tp_bs_read_bits_signed(r, 8, NULL));
    INVALID(tp_bs_read_bits_signed(r, 0, &s));
    INVALID(tp_bs_read_bits_signed(r, 65, &s));

    INVALID(tp_bs_peek_bits(NULL, 8, &u));
    INVALID(tp_bs_peek_bits(r, 8, NULL));
    INVALID(tp_bs_peek_bits(r, 0, &u));
    INVALID(tp_bs_peek_bits(r, 65, &u));

    /* read_bits32 caps at 32, not 64. */
    INVALID(tp_bs_read_bits32(NULL, 8, &u32));
    INVALID(tp_bs_read_bits32(r, 8, NULL));
    INVALID(tp_bs_read_bits32(r, 0, &u32));
    INVALID(tp_bs_read_bits32(r, 33, &u32));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits32(r, 32, &u32));

    INVALID(tp_bs_read_bit(NULL, &bit));
    INVALID(tp_bs_read_bit(r, NULL));

    const uint8_t *ptr = NULL;
    INVALID(tp_bs_reader_direct_ptr(NULL, &ptr, 1));
    INVALID(tp_bs_reader_direct_ptr(r, NULL, 1));

    const uint8_t *gb = NULL;
    uint64_t glen = 0;
    INVALID(tp_bs_reader_get_buffer(NULL, &gb, &glen));
    INVALID(tp_bs_reader_get_buffer(r, NULL, &glen));
    INVALID(tp_bs_reader_get_buffer(r, &gb, NULL));

    tp_bs_reader_destroy(&r);
}

/* Accessors on a NULL reader answer rather than crash. */
void test_reader_accessors_tolerate_null(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_position(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_remaining(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_length(NULL));
    TEST_ASSERT_FALSE(tp_bs_reader_is_byte_aligned(NULL));

    /* remaining() also saturates once the cursor is at or past the end. */
    const uint8_t buf[2] = {0, 0};
    tp_bitstream_reader *r = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create(&r, buf, 16));
    TEST_ASSERT_EQUAL_UINT64(16, tp_bs_reader_remaining(r));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_seek(r, 16));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_remaining(r));
    tp_bs_reader_destroy(&r);
}

void test_reader_create_rejects_each_bad_argument(void)
{
    const uint8_t buf[4] = {1, 2, 3, 4};
    tp_bitstream_reader *r = NULL;

    INVALID(tp_bs_reader_create(NULL, buf, 32));
    INVALID(tp_bs_reader_create_copy(NULL, buf, 32));

    /* A NULL source is a documented way to get a reader over zeroed bits. */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create_copy(&r, NULL, 32));
    uint64_t bits = 1;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_read_bits(r, 32, &bits));
    TEST_ASSERT_EQUAL_UINT64(0, bits); /* zeroed, not whatever was on the heap */
    tp_bs_reader_destroy(&r);

    /* A zero-length copy is legal and owns nothing. */
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create_copy(&r, buf, 0));
    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_reader_length(r));
    tp_bs_reader_destroy(&r);
}

/* ── Writer guards ─────────────────────────────────────────────────── */

void test_writer_guards_reject_each_bad_argument(void)
{
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_create(&w, 16, 0));

    INVALID(tp_bs_writer_create(NULL, 16, 0));

    const uint8_t *buf = NULL;
    size_t byte_len = 0;
    uint64_t bit_len = 0;
    INVALID(tp_bs_writer_get_buffer(NULL, &buf, &bit_len));
    INVALID(tp_bs_writer_get_buffer(w, NULL, &bit_len));
    INVALID(tp_bs_writer_get_buffer(w, &buf, NULL));

    uint8_t *dbuf = NULL;
    INVALID(tp_bs_writer_detach_buffer(NULL, &dbuf, &byte_len, &bit_len));
    INVALID(tp_bs_writer_detach_buffer(w, NULL, &byte_len, &bit_len));
    INVALID(tp_bs_writer_detach_buffer(w, &dbuf, NULL, &bit_len));
    INVALID(tp_bs_writer_detach_buffer(w, &dbuf, &byte_len, NULL));

    /* append_buffer: a NULL buffer is only an error if it claims bits. */
    INVALID(tp_bs_writer_append_buffer(NULL, (const uint8_t *)"x", 8));
    INVALID(tp_bs_writer_append_buffer(w, NULL, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_append_buffer(w, NULL, 0));

    tp_bitstream_reader *r = NULL;
    INVALID(tp_bs_writer_to_reader(NULL, &r));
    INVALID(tp_bs_writer_to_reader(w, NULL));

    TEST_ASSERT_EQUAL_UINT64(0, tp_bs_writer_position(NULL));

    tp_bs_writer_destroy(&w);
}

/* A writer grows by a fixed step when one is given, and doubles otherwise.
 * Both paths have to reach the same bytes. */
void test_writer_growth_modes_agree(void)
{
    for (size_t growth = 0; growth <= 8; growth += 4) {
        tp_bitstream_writer *w = NULL;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_create(&w, 1, growth));

        for (unsigned i = 0; i < 200; i++)
            TEST_ASSERT_EQUAL(TP_OK, tp_bs_write_u8(w, (uint8_t)(i & 0xFF)));

        const uint8_t *buf = NULL;
        uint64_t bit_len = 0;
        TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_get_buffer(w, &buf, &bit_len));
        TEST_ASSERT_EQUAL_UINT64(200 * 8, bit_len);
        for (unsigned i = 0; i < 200; i++)
            TEST_ASSERT_EQUAL_UINT8((uint8_t)(i & 0xFF), buf[i]);

        tp_bs_writer_destroy(&w);
    }
}

void test_copy_bits_rejects_each_bad_argument(void)
{
    const uint8_t src[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    tp_bitstream_reader *r = NULL;
    tp_bitstream_writer *w = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_reader_create(&r, src, 32));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_writer_create(&w, 8, 0));

    INVALID(tp_bs_copy_bits(NULL, w, 8));
    INVALID(tp_bs_copy_bits(r, NULL, 8));
    TEST_ASSERT_EQUAL(TP_OK, tp_bs_copy_bits(r, w, 32));

    tp_bs_writer_destroy(&w);
    tp_bs_reader_destroy(&r);
}

/* ── Values ────────────────────────────────────────────────────────── */

/* tp_value_string measures the string itself; a NULL one is empty, not a
 * crash, so callers can pass an optional field straight through. */
void test_value_string_accepts_null(void)
{
    tp_value v = tp_value_string(NULL);
    TEST_ASSERT_EQUAL(TP_STRING, v.type);
    TEST_ASSERT_NULL(v.data.string_val.str);
    TEST_ASSERT_EQUAL_size_t(0, v.data.string_val.str_len);

    v = tp_value_string("abc");
    TEST_ASSERT_EQUAL_size_t(3, v.data.string_val.str_len);
}

/* ── Header magic ──────────────────────────────────────────────────── */

/* The magic check is four comparisons joined by ||. Only a buffer that is
 * right up to the last byte reaches the final one. */
void test_each_magic_byte_is_checked(void)
{
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    tp_value v = tp_value_int(1);
    tp_encoder_add(enc, "k", &v);
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_encoder_build(enc, &buf, &len);
    tp_encoder_destroy(&enc);

    for (int i = 0; i < 4; i++) {
        uint8_t *bad = malloc(len);
        TEST_ASSERT_NOT_NULL(bad);
        memcpy(bad, buf, len);
        bad[i] ^= 0xFF; /* only this byte is wrong */

        tp_dict *dict = NULL;
        TEST_ASSERT_EQUAL(TP_ERR_BAD_MAGIC, tp_dict_open_unchecked(&dict, bad, len));
        TEST_ASSERT_NULL(dict);
        free(bad);
    }

    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_bits_at_rejects_each_bad_argument);
    RUN_TEST(test_read_bits_signed_at_rejects_each_bad_argument);
    RUN_TEST(test_reader_bit_reads_reject_each_bad_argument);
    RUN_TEST(test_reader_accessors_tolerate_null);
    RUN_TEST(test_reader_create_rejects_each_bad_argument);
    RUN_TEST(test_writer_guards_reject_each_bad_argument);
    RUN_TEST(test_writer_growth_modes_agree);
    RUN_TEST(test_copy_bits_rejects_each_bad_argument);
    RUN_TEST(test_value_string_accepts_null);
    RUN_TEST(test_each_magic_byte_is_checked);
    return UNITY_END();
}
