/* test_v2_bitvec.c — the ranked bit vector, format v2 §5.
 *
 * These tests are written from the specification, not from an implementation.
 * Every expected value below is hand-computed from the definitions in
 * docs/internals/format-spec-v2.md §5.2 and §5.3, so the test is the
 * authority and the code has to conform. That ordering is the point: this
 * primitive carries the tree, the terminal map and the tail map
 * (North Star §5), so a bug here is ten mysterious failures at once.
 *
 * Boundaries worth their own cases, because they are where an implementation
 * that "works" starts disagreeing with nine others:
 *   - 0 bits, and a vector of one bit
 *   - all zeros / all ones
 *   - exactly one block (256) and exactly one superblock (2048)
 *   - one past each of those, where the index gains an entry
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */
#include "triepack/v2_bitvec.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Build a vector from a string of '0'/'1', which keeps the expectations in
   the test readable rather than hidden behind hex. */
static tp2_bitvec *from_string(const char *s)
{
    uint64_t n = (uint64_t)strlen(s);
    uint8_t *bits = calloc((n + 7) / 8 + 1, 1);
    TEST_ASSERT_NOT_NULL(bits);
    for (uint64_t i = 0; i < n; i++)
        if (s[i] == '1')
            bits[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
    tp2_bitvec *bv = tp2_bv_create(bits, n);
    free(bits);
    TEST_ASSERT_NOT_NULL(bv);
    return bv;
}

/* ── Bit access ─────────────────────────────────────────────────────── */

/* §5.1: bit i is byte[i>>3] & (0x80 >> (i&7)) — most significant bit first.
   Getting this backwards is the classic way two implementations diverge. */
void test_bits_are_most_significant_first(void)
{
    const uint8_t byte[1] = {0xA0}; /* 1010 0000 */
    tp2_bitvec *bv = tp2_bv_create(byte, 8);
    const int expect[8] = {1, 0, 1, 0, 0, 0, 0, 0};
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE(expect[i], tp2_bv_get(bv, (uint64_t)i), "bit order");
    tp2_bv_destroy(&bv);
}

/* ── rank ───────────────────────────────────────────────────────────── */

/* §5.2: rank1(i) counts 1 bits *strictly before* i, so rank1(0) is always 0
   and rank1(n) is the population count. */
void test_rank1_counts_bits_strictly_before(void)
{
    tp2_bitvec *bv = from_string("11010");
    const uint64_t expect[6] = {0, 1, 2, 2, 3, 3};
    for (uint64_t i = 0; i <= 5; i++)
        TEST_ASSERT_EQUAL_UINT64(expect[i], tp2_bv_rank1(bv, i));
    TEST_ASSERT_EQUAL_UINT64(3, tp2_bv_rank1(bv, tp2_bv_size(bv)));
    tp2_bv_destroy(&bv);
}

void test_rank0_is_the_complement(void)
{
    tp2_bitvec *bv = from_string("11010");
    for (uint64_t i = 0; i <= 5; i++)
        TEST_ASSERT_EQUAL_UINT64(i - tp2_bv_rank1(bv, i), tp2_bv_rank0(bv, i));
    tp2_bv_destroy(&bv);
}

void test_rank_on_uniform_vectors(void)
{
    tp2_bitvec *zeros = from_string("00000000");
    tp2_bitvec *ones = from_string("11111111");
    for (uint64_t i = 0; i <= 8; i++) {
        TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_rank1(zeros, i));
        TEST_ASSERT_EQUAL_UINT64(i, tp2_bv_rank0(zeros, i));
        TEST_ASSERT_EQUAL_UINT64(i, tp2_bv_rank1(ones, i));
        TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_rank0(ones, i));
    }
    tp2_bv_destroy(&zeros);
    tp2_bv_destroy(&ones);
}

/* ── select ─────────────────────────────────────────────────────────── */

/* §5.3: select is 0-indexed — select1(0) is the position of the *first* 1. */
void test_select_is_zero_indexed(void)
{
    tp2_bitvec *bv = from_string("11010");
    TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_select1(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(1, tp2_bv_select1(bv, 1));
    TEST_ASSERT_EQUAL_UINT64(3, tp2_bv_select1(bv, 2));
    TEST_ASSERT_EQUAL_UINT64(2, tp2_bv_select0(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(4, tp2_bv_select0(bv, 1));
    tp2_bv_destroy(&bv);
}

/* Asking for a bit that is not there must be answerable, not undefined:
   ten implementations cannot agree on undefined behaviour. The size of the
   vector is the defined "not found" answer. */
void test_select_past_the_end_returns_size(void)
{
    tp2_bitvec *bv = from_string("11010");
    TEST_ASSERT_EQUAL_UINT64(tp2_bv_size(bv), tp2_bv_select1(bv, 3));
    TEST_ASSERT_EQUAL_UINT64(tp2_bv_size(bv), tp2_bv_select0(bv, 2));
    TEST_ASSERT_EQUAL_UINT64(tp2_bv_size(bv), tp2_bv_select1(bv, 99));
    tp2_bv_destroy(&bv);
}

/* select and rank are inverses where both are defined. Stated as a property
   so it holds for every position rather than a chosen few. */
void test_select_and_rank_invert_each_other(void)
{
    tp2_bitvec *bv = from_string("1101000110101110001");
    uint64_t n = tp2_bv_size(bv), ones = tp2_bv_rank1(bv, n);
    for (uint64_t k = 0; k < ones; k++) {
        uint64_t p = tp2_bv_select1(bv, k);
        TEST_ASSERT_EQUAL_INT(1, tp2_bv_get(bv, p));
        TEST_ASSERT_EQUAL_UINT64(k, tp2_bv_rank1(bv, p));
    }
    for (uint64_t k = 0; k < n - ones; k++) {
        uint64_t p = tp2_bv_select0(bv, k);
        TEST_ASSERT_EQUAL_INT(0, tp2_bv_get(bv, p));
        TEST_ASSERT_EQUAL_UINT64(k, tp2_bv_rank0(bv, p));
    }
    tp2_bv_destroy(&bv);
}

/* ── Index boundaries ───────────────────────────────────────────────── */

/* §5.1 blocks every 256 bits, superblocks every 2048. An implementation that
   is off by one block boundary passes every small test and fails at 257. */
static void check_against_brute_force(uint64_t n, int period)
{
    char *s = malloc(n + 1);
    TEST_ASSERT_NOT_NULL(s);
    for (uint64_t i = 0; i < n; i++)
        s[i] = (char)(((i % (uint64_t)period) == 0) ? '1' : '0');
    s[n] = 0;

    tp2_bitvec *bv = from_string(s);
    uint64_t running = 0;
    for (uint64_t i = 0; i <= n; i++) {
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(running, tp2_bv_rank1(bv, i), "rank1 vs brute force");
        if (i < n && s[i] == '1')
            running++;
    }
    tp2_bv_destroy(&bv);
    free(s);
}

void test_rank_matches_brute_force_across_block_boundaries(void)
{
    const uint64_t sizes[] = {1, 8, 255, 256, 257, 511, 512, 2047, 2048, 2049, 4096, 4097};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        check_against_brute_force(sizes[i], 3);
        check_against_brute_force(sizes[i], 1); /* all ones */
    }
}

void test_select_matches_brute_force_across_block_boundaries(void)
{
    const uint64_t n = 5000;
    char *s = malloc(n + 1);
    for (uint64_t i = 0; i < n; i++)
        s[i] = (char)((i % 7 == 0 || i % 11 == 0) ? '1' : '0');
    s[n] = 0;

    tp2_bitvec *bv = from_string(s);
    uint64_t k = 0;
    for (uint64_t i = 0; i < n; i++) {
        if (s[i] == '1') {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(i, tp2_bv_select1(bv, k), "select1 vs brute force");
            k++;
        }
    }
    uint64_t z = 0;
    for (uint64_t i = 0; i < n; i++) {
        if (s[i] == '0') {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(i, tp2_bv_select0(bv, z), "select0 vs brute force");
            z++;
        }
    }
    tp2_bv_destroy(&bv);
    free(s);
}

/* ── Degenerate input ───────────────────────────────────────────────── */

void test_empty_vector(void)
{
    tp2_bitvec *bv = tp2_bv_create(NULL, 0);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_size(bv));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_rank1(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_bv_select1(bv, 0));
    tp2_bv_destroy(&bv);
}

void test_destroy_is_idempotent_and_null_safe(void)
{
    tp2_bitvec *bv = from_string("101");
    tp2_bv_destroy(&bv);
    TEST_ASSERT_NULL(bv);
    tp2_bv_destroy(&bv); /* must not crash */
    tp2_bv_destroy(NULL);
}

/* §5.1: the index is 64/2048 + 16/256 of the bit vector, about 9.4%. It is
   stored in the file, so its size is part of the format's cost and worth
   pinning rather than discovering later. */
void test_index_overhead_matches_the_specified_layout(void)
{
    tp2_bitvec *bv = from_string("10101010");
    /* 8 bits -> 1 superblock entry + 1 terminator, 1 block entry + 1 term. */
    TEST_ASSERT_EQUAL_UINT64(2 * 8 + 2 * 2, tp2_bv_index_bytes(bv));
    tp2_bv_destroy(&bv);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bits_are_most_significant_first);
    RUN_TEST(test_rank1_counts_bits_strictly_before);
    RUN_TEST(test_rank0_is_the_complement);
    RUN_TEST(test_rank_on_uniform_vectors);
    RUN_TEST(test_select_is_zero_indexed);
    RUN_TEST(test_select_past_the_end_returns_size);
    RUN_TEST(test_select_and_rank_invert_each_other);
    RUN_TEST(test_rank_matches_brute_force_across_block_boundaries);
    RUN_TEST(test_select_matches_brute_force_across_block_boundaries);
    RUN_TEST(test_empty_vector);
    RUN_TEST(test_destroy_is_idempotent_and_null_safe);
    RUN_TEST(test_index_overhead_matches_the_specified_layout);
    return UNITY_END();
}
