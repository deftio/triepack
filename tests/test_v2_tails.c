/* test_v2_tails.c — suffix-merged tail pool, format v2 §8.1.
 *
 * Written from the specification. §8.1 is *normative*: it is not "the encoder
 * deduplicates suffixes" but a procedure with a defined order and defined tie
 * breaking, because ten implementations pursuing a goal produce ten different
 * files while ten implementations of an algorithm produce one
 * (North Star §4.2).
 *
 * So the tests here fall into two groups:
 *
 *   - the worked example from the spec, byte for byte; and
 *   - properties that must hold for *any* input, above all that the output
 *     does not depend on the order the tails arrive in. That is the property
 *     byte-identity actually rests on, and it is the one an implementation
 *     can accidentally violate while passing every example.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */
#include "triepack/v2_tails.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Build a pool from NUL-terminated C strings, which keeps the cases
   readable. Real tails are arbitrary bytes; test_embedded_nul covers that. */
static tp2_tailpool *build(const char *const *strs, uint64_t n)
{
    const uint8_t **bytes = malloc(n ? n * sizeof(*bytes) : 1);
    uint32_t *lens = malloc(n ? n * sizeof(*lens) : 1);
    for (uint64_t i = 0; i < n; i++) {
        bytes[i] = (const uint8_t *)strs[i];
        lens[i] = (uint32_t)strlen(strs[i]);
    }
    tp2_tailpool *tp = tp2_tailpool_build(bytes, lens, n);
    free(bytes);
    free(lens);
    TEST_ASSERT_NOT_NULL(tp);
    return tp;
}

/* Every reference must resolve to exactly the tail it came from. This is the
   correctness property; everything else is about size and determinism. */
static void assert_all_refs_resolve(tp2_tailpool *tp, const char *const *strs, uint64_t n)
{
    const uint8_t *pool = tp2_tailpool_bytes(tp);
    for (uint64_t i = 0; i < n; i++) {
        tp2_tail_ref r = tp2_tailpool_ref(tp, i);
        uint32_t len = (uint32_t)strlen(strs[i]);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(len, r.length, "reference length");
        TEST_ASSERT_TRUE_MESSAGE(r.offset + r.length <= tp2_tailpool_size(tp),
                                 "reference runs past the pool");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(strs[i], pool + r.offset, len, "reference contents");
    }
}

/* ── The worked example from the spec ───────────────────────────────── */

/* §8.1, which every implementation must reproduce byte for byte:
 *
 *   reversed:  gni  gnir  gnirts
 *   sorted:    ing  ring  string
 *   walk last->first:
 *     string  emit at 0, length 6   pool = "string"
 *     ring    suffix of "string" -> (2, 4)
 *     ing     suffix of "string" -> (3, 3)
 */
void test_spec_worked_example(void)
{
    static const char *const in[] = {"ing", "string", "ring"};
    tp2_tailpool *tp = build(in, 3);

    TEST_ASSERT_EQUAL_UINT64(6, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_MEMORY("string", tp2_tailpool_bytes(tp), 6);
    TEST_ASSERT_EQUAL_UINT64(3, tp2_tailpool_distinct(tp));

    tp2_tail_ref ing = tp2_tailpool_ref(tp, 0);
    tp2_tail_ref str = tp2_tailpool_ref(tp, 1);
    tp2_tail_ref rng = tp2_tailpool_ref(tp, 2);

    TEST_ASSERT_EQUAL_UINT64(3, ing.offset);
    TEST_ASSERT_EQUAL_UINT32(3, ing.length);
    TEST_ASSERT_EQUAL_UINT64(0, str.offset);
    TEST_ASSERT_EQUAL_UINT32(6, str.length);
    TEST_ASSERT_EQUAL_UINT64(2, rng.offset);
    TEST_ASSERT_EQUAL_UINT32(4, rng.length);

    assert_all_refs_resolve(tp, in, 3);
    tp2_tailpool_destroy(&tp);
}

/* ── Determinism ────────────────────────────────────────────────────── */

/* The output must not depend on the order tails arrive in. §8.1 sorts before
   emitting precisely so that it does not, and this is the property that
   byte-identical output across ten implementations rests on. */
void test_input_order_does_not_change_the_pool(void)
{
    static const char *const a[] = {"ing", "string", "ring", "king", "sting"};
    static const char *const b[] = {"sting", "king", "ring", "string", "ing"};
    static const char *const c[] = {"string", "ing", "sting", "ring", "king"};

    tp2_tailpool *pa = build(a, 5);
    tp2_tailpool *pb = build(b, 5);
    tp2_tailpool *pc = build(c, 5);

    uint64_t n = tp2_tailpool_size(pa);
    TEST_ASSERT_EQUAL_UINT64(n, tp2_tailpool_size(pb));
    TEST_ASSERT_EQUAL_UINT64(n, tp2_tailpool_size(pc));
    TEST_ASSERT_EQUAL_MEMORY(tp2_tailpool_bytes(pa), tp2_tailpool_bytes(pb), n);
    TEST_ASSERT_EQUAL_MEMORY(tp2_tailpool_bytes(pa), tp2_tailpool_bytes(pc), n);

    /* and every reference still resolves, whatever the order */
    assert_all_refs_resolve(pa, a, 5);
    assert_all_refs_resolve(pb, b, 5);
    assert_all_refs_resolve(pc, c, 5);

    tp2_tailpool_destroy(&pa);
    tp2_tailpool_destroy(&pb);
    tp2_tailpool_destroy(&pc);
}

/* Repeating a tail must not repeat its bytes, and both occurrences must
   point at the same place. */
void test_duplicates_collapse(void)
{
    static const char *const in[] = {"ing", "ing", "ing"};
    tp2_tailpool *tp = build(in, 3);

    TEST_ASSERT_EQUAL_UINT64(3, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_UINT64(1, tp2_tailpool_distinct(tp));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_UINT64(0, tp2_tailpool_ref(tp, (uint64_t)i).offset);
        TEST_ASSERT_EQUAL_UINT32(3, tp2_tailpool_ref(tp, (uint64_t)i).length);
    }
    assert_all_refs_resolve(tp, in, 3);
    tp2_tailpool_destroy(&tp);
}

/* ── Shapes ─────────────────────────────────────────────────────────── */

/* A chain where each tail is a suffix of the next: everything collapses into
   the longest. */
void test_a_suffix_chain_collapses_completely(void)
{
    static const char *const in[] = {"a", "ba", "cba", "dcba"};
    tp2_tailpool *tp = build(in, 4);

    TEST_ASSERT_EQUAL_UINT64(4, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_MEMORY("dcba", tp2_tailpool_bytes(tp), 4);
    assert_all_refs_resolve(tp, in, 4);
    tp2_tailpool_destroy(&tp);
}

/* Nothing shares a suffix, so nothing can merge; the pool is the
   concatenation and the only question is that the order is deterministic. */
void test_no_sharing_available(void)
{
    static const char *const in[] = {"abc", "def", "ghi"};
    tp2_tailpool *tp = build(in, 3);

    TEST_ASSERT_EQUAL_UINT64(9, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_UINT64(3, tp2_tailpool_distinct(tp));
    assert_all_refs_resolve(tp, in, 3);
    tp2_tailpool_destroy(&tp);
}

/* A tail that is a suffix of an *earlier* emitted tail but not of `prev` is
   stored again. The spec says so explicitly -- it claims determinism and
   linear-after-sort cost, not minimal packing -- so the test pins the
   specified behaviour rather than an aspiration. */
void test_the_algorithm_is_not_optimal_and_says_so(void)
{
    static const char *const in[] = {"ing", "xing", "zzz"};
    tp2_tailpool *tp = build(in, 3);

    /* Sorted by reversed bytes: ing("gni"), xing("gnix"), zzz("zzz").
       Walk last->first: zzz emitted, xing not a suffix of zzz so emitted,
       ing is a suffix of xing so merged. Pool = "zzz" + "xing". */
    TEST_ASSERT_EQUAL_UINT64(7, tp2_tailpool_size(tp));
    assert_all_refs_resolve(tp, in, 3);
    tp2_tailpool_destroy(&tp);
}

/* ── Arbitrary bytes ────────────────────────────────────────────────── */

/* Tails are byte strings, not C strings: a NUL is an ordinary byte and a
   0xFF is an ordinary byte. An implementation reaching for strcmp or strlen
   fails here and nowhere else. */
void test_embedded_nul_and_high_bytes(void)
{
    const uint8_t t0[] = {0x00, 0xFF, 0x41};
    const uint8_t t1[] = {0xFF, 0x41};
    const uint8_t t2[] = {0x41};
    const uint8_t *bytes[] = {t0, t1, t2};
    const uint32_t lens[] = {3, 2, 1};

    tp2_tailpool *tp = tp2_tailpool_build(bytes, lens, 3);
    TEST_ASSERT_NOT_NULL(tp);

    /* t2 is a suffix of t1 which is a suffix of t0, so all three collapse. */
    TEST_ASSERT_EQUAL_UINT64(3, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_MEMORY(t0, tp2_tailpool_bytes(tp), 3);

    const uint8_t *pool = tp2_tailpool_bytes(tp);
    for (uint64_t i = 0; i < 3; i++) {
        tp2_tail_ref r = tp2_tailpool_ref(tp, i);
        TEST_ASSERT_EQUAL_UINT32(lens[i], r.length);
        TEST_ASSERT_EQUAL_MEMORY(bytes[i], pool + r.offset, lens[i]);
    }
    tp2_tailpool_destroy(&tp);
}

/* Comparison is by unsigned byte value. If an implementation compares with a
   signed char, 0x80 sorts below 0x01 and the pool comes out in a different
   order -- silently, and only for non-ASCII data. */
void test_bytes_compare_as_unsigned(void)
{
    const uint8_t hi[] = {0x80};
    const uint8_t lo[] = {0x01};
    const uint8_t *fwd[] = {hi, lo};
    const uint8_t *rev[] = {lo, hi};
    const uint32_t lens[] = {1, 1};

    tp2_tailpool *a = tp2_tailpool_build(fwd, lens, 2);
    tp2_tailpool *b = tp2_tailpool_build(rev, lens, 2);

    TEST_ASSERT_EQUAL_UINT64(2, tp2_tailpool_size(a));
    TEST_ASSERT_EQUAL_UINT64(2, tp2_tailpool_size(b));
    /* 0x01 sorts before 0x80, so the walk emits 0x80 first either way. */
    TEST_ASSERT_EQUAL_MEMORY(tp2_tailpool_bytes(a), tp2_tailpool_bytes(b), 2);
    TEST_ASSERT_EQUAL_UINT8(0x80, tp2_tailpool_bytes(a)[0]);

    tp2_tailpool_destroy(&a);
    tp2_tailpool_destroy(&b);
}

/* ── Degenerate input ───────────────────────────────────────────────── */

void test_no_tails(void)
{
    tp2_tailpool *tp = tp2_tailpool_build(NULL, NULL, 0);
    TEST_ASSERT_NOT_NULL(tp);
    TEST_ASSERT_EQUAL_UINT64(0, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_tailpool_distinct(tp));
    tp2_tailpool_destroy(&tp);
}

void test_a_single_tail(void)
{
    static const char *const in[] = {"solo"};
    tp2_tailpool *tp = build(in, 1);
    TEST_ASSERT_EQUAL_UINT64(4, tp2_tailpool_size(tp));
    TEST_ASSERT_EQUAL_UINT64(0, tp2_tailpool_ref(tp, 0).offset);
    assert_all_refs_resolve(tp, in, 1);
    tp2_tailpool_destroy(&tp);
}

void test_destroy_is_null_safe(void)
{
    tp2_tailpool *tp = NULL;
    tp2_tailpool_destroy(&tp);
    tp2_tailpool_destroy(NULL);
}

/* ── Scale ──────────────────────────────────────────────────────────── */

/* Sharing has to actually happen at size, not just in three-element
   examples: 1,000 words ending in one of four suffixes must store those
   endings a handful of times, not a thousand. */
void test_sharing_happens_at_scale(void)
{
    enum { N = 1000 };
    static const char *const endings[] = {"ing", "tion", "ment", "ness"};
    char storage[N][32];
    const char *strs[N];
    for (int i = 0; i < N; i++) {
        snprintf(storage[i], sizeof(storage[i]), "%d%s", i, endings[i % 4]);
        strs[i] = storage[i];
    }

    tp2_tailpool *tp = build(strs, N);
    uint64_t raw = 0;
    for (int i = 0; i < N; i++)
        raw += strlen(strs[i]);

    TEST_ASSERT_TRUE_MESSAGE(tp2_tailpool_size(tp) < raw, "pool should be smaller than the input");
    assert_all_refs_resolve(tp, strs, N);
    tp2_tailpool_destroy(&tp);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spec_worked_example);
    RUN_TEST(test_input_order_does_not_change_the_pool);
    RUN_TEST(test_duplicates_collapse);
    RUN_TEST(test_a_suffix_chain_collapses_completely);
    RUN_TEST(test_no_sharing_available);
    RUN_TEST(test_the_algorithm_is_not_optimal_and_says_so);
    RUN_TEST(test_embedded_nul_and_high_bytes);
    RUN_TEST(test_bytes_compare_as_unsigned);
    RUN_TEST(test_no_tails);
    RUN_TEST(test_a_single_tail);
    RUN_TEST(test_destroy_is_null_safe);
    RUN_TEST(test_sharing_happens_at_scale);
    return UNITY_END();
}
