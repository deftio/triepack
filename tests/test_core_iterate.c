/* test_core_iterate.c — iteration and prefix descent
 *
 * tp_iter_next and descend_to_prefix carry the subtree bound themselves,
 * because the trie stream never says where a subtree ends. That bookkeeping
 * is what issue #1 got wrong, so the cases here are the ones that pin it:
 * keys that outgrow the iterator's buffer, a prefix that is itself a stored
 * key, a prefix that outruns the subtree it landed in, and — through
 * tp_dict_open_unchecked — tries whose bits have been flipped underneath the
 * walk.
 */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Helpers ───────────────────────────────────────────────────────── */

/** Build a dictionary from parallel key/value arrays. NULL vals means no
 *  value store at all, which is its own path through the iterator. */
static void build(const char *const *keys, const int64_t *vals, size_t n, uint8_t **buf,
                  size_t *len, tp_dict **dict)
{
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    for (size_t i = 0; i < n; i++) {
        if (vals) {
            tp_value v = tp_value_int(vals[i]);
            TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, keys[i], &v));
        } else {
            TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, keys[i], NULL));
        }
    }
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, buf, len));
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(dict, *buf, *len));
    tp_encoder_destroy(&enc);
}

/** Collect every key an iterator yields, newline-joined, for easy asserts. */
static void collect(tp_iterator *it, char *out, size_t out_cap)
{
    const char *key;
    size_t key_len;
    tp_value val;
    size_t used = 0;
    out[0] = '\0';
    while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
        TEST_ASSERT_TRUE(used + key_len + 2 < out_cap);
        if (used)
            out[used++] = '\n';
        memcpy(out + used, key, key_len);
        used += key_len;
        out[used] = '\0';
    }
}

/* ── Iteration without a value store ───────────────────────────────── */

/* Every value is null, so the encoder writes no value store and the iterator
 * must hand back nulls without trying to read one. */
void test_iterate_keys_without_values(void)
{
    static const char *const keys[] = {"alpha", "beta", "gamma"};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, NULL, 3, &buf, &len, &dict);

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(dict, &info));
    TEST_ASSERT_FALSE(info.has_values);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(dict, &it));

    const char *key;
    size_t key_len;
    tp_value val;
    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(TP_OK, tp_iter_next(it, &key, &key_len, &val));
        TEST_ASSERT_EQUAL_size_t(strlen(keys[i]), key_len);
        TEST_ASSERT_EQUAL_MEMORY(keys[i], key, key_len);
        TEST_ASSERT_EQUAL(TP_NULL, val.type);
    }
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* ── Keys longer than the iterator's initial buffer ────────────────── */

/* The key buffer starts at 256 bytes and doubles. A key past that boundary
 * is the only thing that exercises the growth. */
void test_iterate_key_outgrows_initial_buffer(void)
{
    char long_a[1200];
    char long_b[1200];
    memset(long_a, 'a', sizeof(long_a) - 1);
    long_a[sizeof(long_a) - 1] = '\0';
    memcpy(long_b, long_a, sizeof(long_b));
    long_b[600] = 'b'; /* diverges past the first doubling */

    const char *keys[] = {long_a, long_b, "short"};
    const int64_t vals[] = {1, 2, 3};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 3, &buf, &len, &dict);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(dict, &it));

    const char *key;
    size_t key_len;
    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_iter_next(it, &key, &key_len, &val));
    TEST_ASSERT_EQUAL_size_t(strlen(long_a), key_len);
    TEST_ASSERT_EQUAL_MEMORY(long_a, key, key_len);
    TEST_ASSERT_EQUAL_INT64(1, val.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_iter_next(it, &key, &key_len, &val));
    TEST_ASSERT_EQUAL_size_t(strlen(long_b), key_len);
    TEST_ASSERT_EQUAL_MEMORY(long_b, key, key_len);
    TEST_ASSERT_EQUAL_INT64(2, val.data.int_val);

    TEST_ASSERT_EQUAL(TP_OK, tp_iter_next(it, &key, &key_len, &val));
    TEST_ASSERT_EQUAL_MEMORY("short", key, 5);
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* A prefix longer than the initial buffer has to be seeded into it too. */
void test_find_prefix_outgrows_initial_buffer(void)
{
    char long_key[1000];
    memset(long_key, 'z', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';

    char long_prefix[700];
    memcpy(long_prefix, long_key, sizeof(long_prefix) - 1);
    long_prefix[sizeof(long_prefix) - 1] = '\0';

    const char *keys[] = {long_key, "other"};
    const int64_t vals[] = {7, 8};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 2, &buf, &len, &dict);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, long_prefix, &it));

    const char *key;
    size_t key_len;
    tp_value val;
    TEST_ASSERT_EQUAL(TP_OK, tp_iter_next(it, &key, &key_len, &val));
    TEST_ASSERT_EQUAL_size_t(strlen(long_key), key_len);
    TEST_ASSERT_EQUAL_MEMORY(long_key, key, key_len);
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* ── Prefix descent through a key that is also a prefix ────────────── */

/* "he" is a stored key *and* the road to "hello". Descending past it means
 * stepping over its terminal and the BRANCH that follows. */
void test_find_prefix_descends_through_stored_key(void)
{
    static const char *const keys[] = {"he", "hello", "help", "hen"};
    static const int64_t vals[] = {1, 2, 3, 4};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 4, &buf, &len, &dict);

    char got[256];
    tp_iterator *it = NULL;

    /* Past the terminal, into the subtree below it. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "hel", &it));
    collect(it, got, sizeof(got));
    TEST_ASSERT_EQUAL_STRING("hello\nhelp", got);
    tp_iter_destroy(&it);

    /* Landing exactly on the terminal yields it and everything under it. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "he", &it));
    collect(it, got, sizeof(got));
    TEST_ASSERT_EQUAL_STRING("he\nhello\nhelp\nhen", got);
    tp_iter_destroy(&it);

    /* A full key with no children below it. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "hen", &it));
    collect(it, got, sizeof(got));
    TEST_ASSERT_EQUAL_STRING("hen", got);
    tp_iter_destroy(&it);

    tp_dict_close(&dict);
    free(buf);
}

/* A prefix that runs past the end of the only key sharing its start. */
void test_find_prefix_outruns_the_subtree(void)
{
    static const char *const keys[] = {"he", "zebra"};
    static const int64_t vals[] = {1, 2};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 2, &buf, &len, &dict);

    const char *key;
    size_t key_len;
    tp_value val;
    tp_iterator *it = NULL;

    /* "he" has no children, so "hell" cannot continue. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "hell", &it));
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));
    tp_iter_destroy(&it);

    /* Longer than any key in the dictionary at all. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "zebras-and-more", &it));
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));
    tp_iter_destroy(&it);

    tp_dict_close(&dict);
    free(buf);
}

/* A prefix that diverges from the key on a literal byte partway down. */
void test_find_prefix_diverges_mid_key(void)
{
    static const char *const keys[] = {"hello", "world"};
    static const int64_t vals[] = {1, 2};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 2, &buf, &len, &dict);

    const char *key;
    size_t key_len;
    tp_value val;
    tp_iterator *it = NULL;

    /* Shares "hel", then 'p' where the trie has 'l'. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "help", &it));
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));
    tp_iter_destroy(&it);

    /* Diverges on the very first byte of a multi-child branch. */
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "m", &it));
    TEST_ASSERT_EQUAL(TP_ERR_EOF, tp_iter_next(it, &key, &key_len, &val));
    tp_iter_destroy(&it);

    tp_dict_close(&dict);
    free(buf);
}

/* Prefix iteration is restartable, same as plain iteration. */
void test_find_prefix_reset_replays(void)
{
    static const char *const keys[] = {"car", "cart", "cat", "dog"};
    static const int64_t vals[] = {1, 2, 3, 4};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 4, &buf, &len, &dict);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_find_prefix(dict, "car", &it));

    char first[128];
    collect(it, first, sizeof(first));
    TEST_ASSERT_EQUAL_STRING("car\ncart", first);

    TEST_ASSERT_EQUAL(TP_OK, tp_iter_reset(it));
    char second[128];
    collect(it, second, sizeof(second));
    TEST_ASSERT_EQUAL_STRING(first, second);

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* ── Iteration over corrupted tries ────────────────────────────────── */

/* Open bypasses the CRC, so the walk itself is the only thing standing
 * between a flipped bit and a bad read. Every single-bit corruption of the
 * body has to end in an error or a finite key list — never a crash, a hang,
 * or a read past the buffer. Run under ASan this is the real check. */
void test_iterate_survives_single_bit_corruption(void)
{
    static const char *const keys[] = {"apple",  "application", "apply",
                                       "banana", "band",        "bandana"};
    static const int64_t vals[] = {1, 2, 3, 4, 5, 6};
    /* "band" is both a key and the road to "bandana", so the probes below
       exercise descent through a terminal as well as descent to one. */
    uint8_t *clean = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 6, &clean, &len, &dict);
    tp_dict_close(&dict);

    uint8_t *scratch = malloc(len);
    TEST_ASSERT_NOT_NULL(scratch);

    /* Past the 4-byte magic — the header's own offsets and flags are as
       able to mislead the walk as the trie bits are — and before the CRC. */
    const size_t body_start = 4;
    const size_t body_end = len - 4;
    TEST_ASSERT_TRUE(body_end > body_start);

    unsigned long walked = 0;
    for (size_t byte = body_start; byte < body_end; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            memcpy(scratch, clean, len);
            scratch[byte] ^= (uint8_t)(1u << bit);

            tp_dict *bad = NULL;
            if (tp_dict_open_unchecked(&bad, scratch, len) != TP_OK)
                continue;

            tp_iterator *it = NULL;
            if (tp_dict_iterate(bad, &it) == TP_OK) {
                const char *key;
                size_t key_len;
                tp_value val;
                /* A corrupt trie may describe far more keys than were
                   encoded; the cap turns a hang into a failure. */
                unsigned steps = 0;
                while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
                    TEST_ASSERT_TRUE_MESSAGE(++steps < 100000, "iteration did not terminate");
                    walked++;
                }
                tp_iter_destroy(&it);
            }

            /* Prefix descent over the same corruption. The prefixes differ
               in how far they get: one stops at a branch, one descends
               through a key that is also a prefix, one runs past every key. */
            static const char *const probes[] = {"app", "band", "b", "bandanas", "z"};
            for (size_t p = 0; p < sizeof(probes) / sizeof(probes[0]); p++) {
                tp_iterator *pit = NULL;
                if (tp_dict_find_prefix(bad, probes[p], &pit) != TP_OK || !pit)
                    continue;
                const char *key;
                size_t key_len;
                tp_value val;
                unsigned steps = 0;
                while (tp_iter_next(pit, &key, &key_len, &val) == TP_OK)
                    TEST_ASSERT_TRUE_MESSAGE(++steps < 100000, "prefix walk did not terminate");
                tp_iter_destroy(&pit);
            }

            tp_dict_close(&bad);
        }
    }

    /* Some corruptions are survivable, so the sweep should have produced
       real keys rather than bailing out on the first flip every time. */
    TEST_ASSERT_TRUE(walked > 0);

    free(scratch);
    free(clean);
}

/* Truncating the buffer leaves the walk reading off the end of the trie. */
void test_iterate_truncated_body(void)
{
    static const char *const keys[] = {"alpha", "alphabet", "beta", "betamax"};
    static const int64_t vals[] = {1, 2, 3, 4};
    uint8_t *clean = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 4, &clean, &len, &dict);
    tp_dict_close(&dict);

    for (size_t cut = 33; cut < len; cut += 3) {
        tp_dict *bad = NULL;
        if (tp_dict_open_unchecked(&bad, clean, cut) != TP_OK)
            continue;

        tp_iterator *it = NULL;
        if (tp_dict_iterate(bad, &it) == TP_OK) {
            const char *key;
            size_t key_len;
            tp_value val;
            unsigned steps = 0;
            while (tp_iter_next(it, &key, &key_len, &val) == TP_OK)
                TEST_ASSERT_TRUE_MESSAGE(++steps < 100000, "iteration did not terminate");
            tp_iter_destroy(&it);
        }
        tp_dict_close(&bad);
    }

    free(clean);
}

/* ── Nesting deeper than the iterator's frame stack ────────────────── */

/* The walk keeps one frame per open branch, capped at TP_ITER_MAX_DEPTH
 * (256). A chain of keys each extending the last nests one branch per key,
 * so past that depth iteration has to stop with TP_ERR_OVERFLOW rather than
 * run off the end of the stack. Lookup is not frame-bound and still works. */
void test_iterate_nesting_deeper_than_frame_stack(void)
{
    enum { DEPTH = 300 };
    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    char key[DEPTH + 1];
    for (int n = 1; n <= DEPTH; n++) {
        memset(key, 'a', (size_t)n);
        key[n] = '\0';
        tp_value v = tp_value_int(n);
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, &v));
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    /* Every key is still reachable by lookup, including past the cap. */
    memset(key, 'a', DEPTH);
    key[DEPTH] = '\0';
    tp_value got;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, key, &got));
    TEST_ASSERT_EQUAL_INT64(DEPTH, got.data.int_val);

    /* Iteration yields what fits on the stack, then says why it stopped. */
    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(dict, &it));
    const char *k;
    size_t kl;
    tp_value val;
    int walked = 0;
    tp_result rc;
    while ((rc = tp_iter_next(it, &k, &kl, &val)) == TP_OK) {
        TEST_ASSERT_EQUAL_size_t((size_t)(walked + 1), kl);
        walked++;
    }
    TEST_ASSERT_EQUAL(TP_ERR_OVERFLOW, rc);
    TEST_ASSERT_EQUAL_INT(256, walked);

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

/* ── Header that disagrees with the trie ───────────────────────────── */

/* The header's HAS_VALUES flag and the trie's END_VAL terminals have to
 * agree. Clearing the flag on a dictionary that has values leaves the walk
 * meeting terminals that name a value store the header says is not there;
 * it must report nulls rather than read from it. The flag is bit 0 of the
 * u16 at offset 6, so it lives in byte 7. */
void test_iterate_header_claims_no_values(void)
{
    static const char *const keys[] = {"one", "two", "three"};
    static const int64_t vals[] = {1, 2, 3};
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_dict *dict = NULL;
    build(keys, vals, 3, &buf, &len, &dict);
    tp_dict_close(&dict);

    tp_dict_info info;
    buf[7] &= (uint8_t)~1u; /* clear HAS_VALUES */

    tp_dict *lying = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open_unchecked(&lying, buf, len));
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(lying, &info));
    TEST_ASSERT_FALSE(info.has_values);

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(lying, &it));

    const char *key;
    size_t key_len;
    tp_value val;
    unsigned seen = 0;
    while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
        TEST_ASSERT_EQUAL(TP_NULL, val.type);
        seen++;
    }
    TEST_ASSERT_EQUAL_UINT(3, seen);

    tp_iter_destroy(&it);
    tp_dict_close(&lying);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_iterate_keys_without_values);
    RUN_TEST(test_iterate_key_outgrows_initial_buffer);
    RUN_TEST(test_find_prefix_outgrows_initial_buffer);
    RUN_TEST(test_find_prefix_descends_through_stored_key);
    RUN_TEST(test_find_prefix_outruns_the_subtree);
    RUN_TEST(test_find_prefix_diverges_mid_key);
    RUN_TEST(test_find_prefix_reset_replays);
    RUN_TEST(test_iterate_nesting_deeper_than_frame_stack);
    RUN_TEST(test_iterate_header_claims_no_values);
    RUN_TEST(test_iterate_survives_single_bit_corruption);
    RUN_TEST(test_iterate_truncated_body);
    return UNITY_END();
}
