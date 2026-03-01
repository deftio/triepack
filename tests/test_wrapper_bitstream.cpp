/**
 * @file test_wrapper_bitstream.cpp
 * @brief C++ wrapper tests for BitstreamReader/BitstreamWriter.
 */

extern "C" {
#include "triepack/triepack_bitstream.h"
#include "unity.h"
}

#include "triepack/bitstream.hpp"

#include <cstring>

void setUp(void) {}
void tearDown(void) {}

/* ── Tests ───────────────────────────────────────────────────────────── */

void test_writer_create(void)
{
    triepack::BitstreamWriter w;
    TEST_ASSERT_NOT_NULL(w.handle());
    TEST_ASSERT_EQUAL(0, w.position());
}

void test_writer_write_and_read(void)
{
    triepack::BitstreamWriter w;
    w.write(0xAB, 8);
    w.write(0x05, 4);

    TEST_ASSERT_EQUAL(12, w.position());
    TEST_ASSERT_EQUAL(2, w.size()); /* 12 bits -> 2 bytes */
    TEST_ASSERT_NOT_NULL(w.data());
}

void test_reader_create(void)
{
    uint8_t data[] = {0xAB, 0xCD};
    triepack::BitstreamReader r(data, sizeof(data));
    TEST_ASSERT_NOT_NULL(r.handle());
    TEST_ASSERT_EQUAL(0, r.position());
}

void test_roundtrip(void)
{
    triepack::BitstreamWriter w;
    w.write(42, 8);
    w.write(7, 4);
    w.write(255, 8);

    triepack::BitstreamReader r(w.data(), w.size());

    TEST_ASSERT_EQUAL_UINT32(42, r.read(8));
    TEST_ASSERT_EQUAL_UINT32(7, r.read(4));
    TEST_ASSERT_EQUAL_UINT32(255, r.read(8));
}

void test_move_writer(void)
{
    triepack::BitstreamWriter w1;
    w1.write(123, 8);

    triepack::BitstreamWriter w2(static_cast<triepack::BitstreamWriter &&>(w1));
    TEST_ASSERT_NULL(w1.handle());
    TEST_ASSERT_NOT_NULL(w2.handle());
    TEST_ASSERT_EQUAL(8, w2.position());
}

void test_move_reader(void)
{
    uint8_t data[] = {0xFF};
    triepack::BitstreamReader r1(data, 1);

    triepack::BitstreamReader r2(static_cast<triepack::BitstreamReader &&>(r1));
    TEST_ASSERT_NULL(r1.handle());
    TEST_ASSERT_NOT_NULL(r2.handle());
    TEST_ASSERT_EQUAL_UINT32(0xFF, r2.read(8));
}

void test_varint_via_c_api(void)
{
    /* Write a varint using C API through the wrapper handle */
    triepack::BitstreamWriter w;
    tp_bs_write_varint_u(w.handle(), 300);

    triepack::BitstreamReader r(w.data(), w.size());
    uint64_t val = 0;
    tp_result rc = tp_bs_read_varint_u(r.handle(), &val);
    TEST_ASSERT_EQUAL_INT(TP_OK, rc);
    TEST_ASSERT_EQUAL_UINT64(300, val);
}

void test_multi_bit_widths(void)
{
    triepack::BitstreamWriter w;
    w.write(1, 1);
    w.write(3, 2);
    w.write(15, 4);
    w.write(0, 1);

    triepack::BitstreamReader r(w.data(), w.size());
    TEST_ASSERT_EQUAL_UINT32(1, r.read(1));
    TEST_ASSERT_EQUAL_UINT32(3, r.read(2));
    TEST_ASSERT_EQUAL_UINT32(15, r.read(4));
    TEST_ASSERT_EQUAL_UINT32(0, r.read(1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_writer_create);
    RUN_TEST(test_writer_write_and_read);
    RUN_TEST(test_reader_create);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_move_writer);
    RUN_TEST(test_move_reader);
    RUN_TEST(test_varint_via_c_api);
    RUN_TEST(test_multi_bit_widths);
    return UNITY_END();
}
