/**
 * @file test_core_internal.c
 * @brief Tests for internal header.c and value.c functions.
 */

#include "core_internal.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Header write/read NULL params ──────────────────────────────────── */

void test_header_write_null_writer(void)
{
    tp_header h;
    memset(&h, 0, sizeof(h));
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_header_write(NULL, &h));
}

void test_header_write_null_header(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_header_write(w, NULL));
    tp_bs_writer_destroy(&w);
}

void test_header_read_null_reader(void)
{
    tp_header h;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_header_read(NULL, &h));
}

void test_header_read_null_header(void)
{
    uint8_t buf[TP_HEADER_SIZE] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, TP_HEADER_SIZE * 8);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_header_read(r, NULL));
    tp_bs_reader_destroy(&r);
}

/* ── Header read truncated buffer ───────────────────────────────────── */

void test_header_read_truncated(void)
{
    /* Only 6 bytes: magic + partial version */
    uint8_t buf[6] = {TP_MAGIC_0, TP_MAGIC_1, TP_MAGIC_2, TP_MAGIC_3, 1, 0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 6 * 8);

    tp_header h;
    tp_result rc = tp_header_read(r, &h);
    TEST_ASSERT_NOT_EQUAL(TP_OK, rc);

    tp_bs_reader_destroy(&r);
}

/* ── Header round-trip ──────────────────────────────────────────────── */

void test_header_roundtrip(void)
{
    tp_header h_in;
    memset(&h_in, 0, sizeof(h_in));
    h_in.magic[0] = TP_MAGIC_0;
    h_in.magic[1] = TP_MAGIC_1;
    h_in.magic[2] = TP_MAGIC_2;
    h_in.magic[3] = TP_MAGIC_3;
    h_in.version_major = TP_FORMAT_VERSION_MAJOR;
    h_in.version_minor = TP_FORMAT_VERSION_MINOR;
    h_in.flags = TP_FLAG_HAS_VALUES;
    h_in.num_keys = 42;
    h_in.trie_data_offset = 100;
    h_in.value_store_offset = 200;
    h_in.suffix_table_offset = 0;
    h_in.total_data_bits = 1024;
    h_in.reserved = 0;

    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_result rc = tp_header_write(w, &h_in);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    tp_header h_out;
    rc = tp_header_read(r, &h_out);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    TEST_ASSERT_EQUAL_UINT8(TP_MAGIC_0, h_out.magic[0]);
    TEST_ASSERT_EQUAL_UINT8(TP_MAGIC_1, h_out.magic[1]);
    TEST_ASSERT_EQUAL_UINT8(TP_MAGIC_2, h_out.magic[2]);
    TEST_ASSERT_EQUAL_UINT8(TP_MAGIC_3, h_out.magic[3]);
    TEST_ASSERT_EQUAL_UINT8(TP_FORMAT_VERSION_MAJOR, h_out.version_major);
    TEST_ASSERT_EQUAL_UINT8(TP_FORMAT_VERSION_MINOR, h_out.version_minor);
    TEST_ASSERT_EQUAL_UINT16(TP_FLAG_HAS_VALUES, h_out.flags);
    TEST_ASSERT_EQUAL_UINT32(42, h_out.num_keys);
    TEST_ASSERT_EQUAL_UINT32(100, h_out.trie_data_offset);
    TEST_ASSERT_EQUAL_UINT32(200, h_out.value_store_offset);
    TEST_ASSERT_EQUAL_UINT32(0, h_out.suffix_table_offset);
    TEST_ASSERT_EQUAL_UINT32(1024, h_out.total_data_bits);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

/* ── Value encode/decode NULL params ────────────────────────────────── */

void test_value_encode_null_writer(void)
{
    tp_value v = tp_value_null();
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_encode(NULL, &v));
}

void test_value_encode_null_value(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_encode(w, NULL));
    tp_bs_writer_destroy(&w);
}

void test_value_decode_null_reader(void)
{
    tp_value v;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_decode(NULL, &v, NULL));
}

void test_value_decode_null_value(void)
{
    uint8_t buf[4] = {0};
    tp_bitstream_reader *r = NULL;
    tp_bs_reader_create(&r, buf, 32);
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_decode(r, NULL, NULL));
    tp_bs_reader_destroy(&r);
}

/* ── Value encode ARRAY/DICT → error ────────────────────────────────── */

void test_value_encode_array_error(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);

    tp_value v;
    v.type = TP_ARRAY;
    v.data.array_val.elements = NULL;
    v.data.array_val.count = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_encode(w, &v));

    tp_bs_writer_destroy(&w);
}

void test_value_encode_dict_error(void)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);

    tp_value v;
    v.type = TP_DICT;
    v.data.dict_val.dict_buf = NULL;
    v.data.dict_val.dict_len = 0;
    TEST_ASSERT_EQUAL_INT(TP_ERR_INVALID_PARAM, tp_value_encode(w, &v));

    tp_bs_writer_destroy(&w);
}

/* ── tp_value_string_n constructor ──────────────────────────────────── */

void test_value_string_n(void)
{
    tp_value v = tp_value_string_n("hello world", 5);
    TEST_ASSERT_EQUAL_INT(TP_STRING, v.type);
    TEST_ASSERT_EQUAL_INT(5, v.data.string_val.str_len);
    TEST_ASSERT_EQUAL_MEMORY("hello", v.data.string_val.str, 5);
}

/* ── Value round-trip for each type ─────────────────────────────────── */

static void roundtrip_value(const tp_value *val_in)
{
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_result rc = tp_value_encode(w, val_in);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);

    /* Get the buffer base for string/blob pointer resolution */
    const uint8_t *base;
    uint64_t bit_len;
    tp_bs_reader_get_buffer(r, &base, &bit_len);

    tp_value val_out;
    rc = tp_value_decode(r, &val_out, base);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_INT(val_in->type, val_out.type);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_null(void)
{
    tp_value v = tp_value_null();
    roundtrip_value(&v);
}

void test_value_roundtrip_bool_true(void)
{
    tp_value v = tp_value_bool(true);
    roundtrip_value(&v);
}

void test_value_roundtrip_bool_false(void)
{
    tp_value v = tp_value_bool(false);
    roundtrip_value(&v);
}

void test_value_roundtrip_int(void)
{
    tp_value v = tp_value_int(-42);
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_INT, out.type);
    TEST_ASSERT_EQUAL_INT64(-42, out.data.int_val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_uint(void)
{
    tp_value v = tp_value_uint(999);
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_UINT, out.type);
    TEST_ASSERT_EQUAL_UINT64(999, out.data.uint_val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_float32(void)
{
    tp_value v = tp_value_float32(2.5f);
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT32, out.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, out.data.float32_val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_float64(void)
{
    tp_value v = tp_value_float64(3.14159);
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_FLOAT64, out.type);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.14159, out.data.float64_val);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_string(void)
{
    tp_value v = tp_value_string("hello");
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_STRING, out.type);
    TEST_ASSERT_EQUAL_INT(5, out.data.string_val.str_len);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

void test_value_roundtrip_blob(void)
{
    uint8_t blob_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    tp_value v = tp_value_blob(blob_data, 4);
    tp_bitstream_writer *w = NULL;
    tp_bs_writer_create(&w, 0, 0);
    tp_value_encode(w, &v);

    tp_bitstream_reader *r = NULL;
    tp_bs_writer_to_reader(w, &r);
    const uint8_t *base;
    uint64_t bl;
    tp_bs_reader_get_buffer(r, &base, &bl);

    tp_value out;
    tp_value_decode(r, &out, base);
    TEST_ASSERT_EQUAL_INT(TP_BLOB, out.type);
    TEST_ASSERT_EQUAL_INT(4, out.data.blob_val.len);

    tp_bs_reader_destroy(&r);
    tp_bs_writer_destroy(&w);
}

int main(void)
{
    UNITY_BEGIN();
    /* Header NULL params */
    RUN_TEST(test_header_write_null_writer);
    RUN_TEST(test_header_write_null_header);
    RUN_TEST(test_header_read_null_reader);
    RUN_TEST(test_header_read_null_header);
    /* Header truncated */
    RUN_TEST(test_header_read_truncated);
    /* Header round-trip */
    RUN_TEST(test_header_roundtrip);
    /* Value NULL params */
    RUN_TEST(test_value_encode_null_writer);
    RUN_TEST(test_value_encode_null_value);
    RUN_TEST(test_value_decode_null_reader);
    RUN_TEST(test_value_decode_null_value);
    /* Value ARRAY/DICT error */
    RUN_TEST(test_value_encode_array_error);
    RUN_TEST(test_value_encode_dict_error);
    /* String_n constructor */
    RUN_TEST(test_value_string_n);
    /* Value round-trips */
    RUN_TEST(test_value_roundtrip_null);
    RUN_TEST(test_value_roundtrip_bool_true);
    RUN_TEST(test_value_roundtrip_bool_false);
    RUN_TEST(test_value_roundtrip_int);
    RUN_TEST(test_value_roundtrip_uint);
    RUN_TEST(test_value_roundtrip_float32);
    RUN_TEST(test_value_roundtrip_float64);
    RUN_TEST(test_value_roundtrip_string);
    RUN_TEST(test_value_roundtrip_blob);
    return UNITY_END();
}
