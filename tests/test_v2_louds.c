/* test_v2_louds.c — LOUDS tree navigation, format v2 §6.
 *
 * Written from the specification. Every tree below is drawn in the comment,
 * its bit vector written out by hand from §6.1, and the expected navigation
 * results computed from the three formulas in §6.2:
 *
 *   child_begin(v) = (v == 0) ? 0 : select0(v - 1) + 1
 *   child_count(v) = select0(v) - child_begin(v)
 *   child(v, k)    = rank1(child_begin(v) + k) + 1
 *
 * This is the layer that replaces v1's SKIP distances. Issue #1 was the
 * decoder mis-deriving subtree extents from those distances; here extents are
 * implicit, so the failure mode this file guards against is different --
 * navigation that is off by one node -- and it is caught by walking whole
 * trees rather than probing single nodes.
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
    return bv;
}

/* Build the LOUDS bit string for a tree given each node's degree in BFS
   order -- §6.1 is exactly "1^d 0 for each node, in order". */
static tp2_bitvec *from_degrees(const int *deg, int n)
{
    char buf[512];
    int p = 0;
    for (int i = 0; i < n; i++) {
        for (int d = 0; d < deg[i]; d++)
            buf[p++] = '1';
        buf[p++] = '0';
    }
    buf[p] = 0;
    return from_string(buf);
}

/* ── The worked example from the spec ───────────────────────────────── */

/* §6.3, which every implementation must reproduce exactly:
 *
 *        0
 *       / \
 *      1   2          degrees: 2, 0, 0   ->   "11" "0" "0" "0"  =  11000
 */
void test_spec_worked_example(void)
{
    tp2_bitvec *bv = from_string("11000");

    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_begin(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(2, tp2_louds_child_count(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(1, tp2_louds_child(bv, 0, 0));
    TEST_ASSERT_EQUAL_UINT64(2, tp2_louds_child(bv, 0, 1));

    TEST_ASSERT_EQUAL_UINT64(3, tp2_louds_child_begin(bv, 1));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 1));

    TEST_ASSERT_EQUAL_UINT64(4, tp2_louds_child_begin(bv, 2));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 2));

    tp2_bv_destroy(&bv);
}

/* ── Shapes that break naive implementations ────────────────────────── */

/* A single chain: every node has exactly one child. An off-by-one in
 * child_begin walks off the end here immediately.
 *
 *   0 -> 1 -> 2 -> 3      degrees 1,1,1,0  ->  10 10 10 0
 */
void test_a_chain(void)
{
    const int deg[] = {1, 1, 1, 0};
    tp2_bitvec *bv = from_degrees(deg, 4);

    for (uint64_t v = 0; v < 3; v++) {
        TEST_ASSERT_EQUAL_UINT64(1, tp2_louds_child_count(bv, v));
        TEST_ASSERT_EQUAL_UINT64(v + 1, tp2_louds_child(bv, v, 0));
    }
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 3));
    tp2_bv_destroy(&bv);
}

/* A root with many children and nothing below: exercises a long run of 1s,
 * which is where a block-boundary bug in rank would surface. */
void test_a_wide_root(void)
{
    int deg[201];
    deg[0] = 200;
    for (int i = 1; i <= 200; i++)
        deg[i] = 0;
    tp2_bitvec *bv = from_degrees(deg, 201);

    TEST_ASSERT_EQUAL_UINT64(200, tp2_louds_child_count(bv, 0));
    for (uint64_t k = 0; k < 200; k++)
        TEST_ASSERT_EQUAL_UINT64(k + 1, tp2_louds_child(bv, 0, k));
    for (uint64_t v = 1; v <= 200; v++)
        TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, v));
    tp2_bv_destroy(&bv);
}

/* A mixed tree, hand-checked node by node.
 *
 *              0
 *            / | \
 *           1  2  3
 *          /|     |
 *         4 5     6
 *                 |
 *                 7
 *
 * BFS degrees: 0:3, 1:2, 2:0, 3:1, 4:0, 5:0, 6:1, 7:0
 */
void test_a_mixed_tree(void)
{
    const int deg[] = {3, 2, 0, 1, 0, 0, 1, 0};
    tp2_bitvec *bv = from_degrees(deg, 8);

    TEST_ASSERT_EQUAL_UINT64(3, tp2_louds_child_count(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(1, tp2_louds_child(bv, 0, 0));
    TEST_ASSERT_EQUAL_UINT64(2, tp2_louds_child(bv, 0, 1));
    TEST_ASSERT_EQUAL_UINT64(3, tp2_louds_child(bv, 0, 2));

    TEST_ASSERT_EQUAL_UINT64(2, tp2_louds_child_count(bv, 1));
    TEST_ASSERT_EQUAL_UINT64(4, tp2_louds_child(bv, 1, 0));
    TEST_ASSERT_EQUAL_UINT64(5, tp2_louds_child(bv, 1, 1));

    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 2));

    TEST_ASSERT_EQUAL_UINT64(1, tp2_louds_child_count(bv, 3));
    TEST_ASSERT_EQUAL_UINT64(6, tp2_louds_child(bv, 3, 0));

    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 4));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 5));

    TEST_ASSERT_EQUAL_UINT64(1, tp2_louds_child_count(bv, 6));
    TEST_ASSERT_EQUAL_UINT64(7, tp2_louds_child(bv, 6, 0));

    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 7));
    tp2_bv_destroy(&bv);
}

/* ── Structural properties ──────────────────────────────────────────── */

/* Walking the whole tree must visit every node exactly once, and the child
 * indices must be a permutation of 1..n-1. This catches navigation that is
 * self-consistent but wrong -- the kind that passes spot checks. */
void test_a_full_walk_visits_every_node_once(void)
{
    const int deg[] = {3, 2, 0, 1, 0, 0, 1, 0};
    const int n = 8;
    tp2_bitvec *bv = from_degrees(deg, n);

    int seen[8] = {0};
    uint64_t stack[16];
    int sp = 0;
    stack[sp++] = 0;
    seen[0]++;

    while (sp > 0) {
        uint64_t v = stack[--sp];
        uint64_t cc = tp2_louds_child_count(bv, v);
        TEST_ASSERT_EQUAL_UINT64_MESSAGE((uint64_t)deg[v], cc, "degree mismatch");
        for (uint64_t k = 0; k < cc; k++) {
            uint64_t c = tp2_louds_child(bv, v, k);
            TEST_ASSERT_TRUE_MESSAGE(c < (uint64_t)n, "child index out of range");
            seen[c]++;
            stack[sp++] = c;
        }
    }
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, seen[i], "node not visited exactly once");
    tp2_bv_destroy(&bv);
}

/* Children are contiguous in BFS order, so child(v, k+1) == child(v, k) + 1.
 * The format relies on this for the label array to be indexable. */
void test_children_are_contiguous(void)
{
    const int deg[] = {3, 2, 0, 1, 0, 0, 1, 0};
    tp2_bitvec *bv = from_degrees(deg, 8);
    for (uint64_t v = 0; v < 8; v++) {
        uint64_t cc = tp2_louds_child_count(bv, v);
        for (uint64_t k = 1; k < cc; k++)
            TEST_ASSERT_EQUAL_UINT64(tp2_louds_child(bv, v, k - 1) + 1, tp2_louds_child(bv, v, k));
    }
    tp2_bv_destroy(&bv);
}

/* A tree of one node: the root, no children. The smallest legal dictionary. */
void test_a_lone_root(void)
{
    const int deg[] = {0};
    tp2_bitvec *bv = from_degrees(deg, 1);
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_begin(bv, 0));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_louds_child_count(bv, 0));
    tp2_bv_destroy(&bv);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spec_worked_example);
    RUN_TEST(test_a_chain);
    RUN_TEST(test_a_wide_root);
    RUN_TEST(test_a_mixed_tree);
    RUN_TEST(test_a_full_walk_visits_every_node_once);
    RUN_TEST(test_children_are_contiguous);
    RUN_TEST(test_a_lone_root);
    return UNITY_END();
}
