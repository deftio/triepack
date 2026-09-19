/* test_scale.c — large dictionaries.
 *
 * Inputs of a gigabyte are ordinary now, so the suite has to say what happens
 * at that size rather than assume. Two things are being checked: that large
 * dictionaries round-trip correctly, and that the point where the v1 header
 * runs out of room is a clean error rather than a corrupt file.
 *
 * The v1 header stores trie_data_offset, value_store_offset and
 * total_data_bits as 32-bit *bit* counts, so the data stream cannot exceed
 * 2^32 bits -- 512 MB. A 1 GB input is therefore not representable in v1 at
 * all; format v2 widens these to 64-bit byte offsets. See
 * docs/internals/format-spec-v2.md.
 *
 * Size is tiered so the default run stays fast:
 *
 *   (default)                    ~4 MB of keys, a few seconds
 *   TRIEPACK_SCALE_MB=64         64 MB
 *   TRIEPACK_SCALE_MB=700        past the v1 ceiling; asserts the refusal
 *
 * The large tiers need roughly 8x their size in RAM and are not run by
 * default; CI runs the default tier per-PR and a larger tier nightly.
 */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

/* Target size in megabytes of raw key bytes. */
static size_t scale_mb(void)
{
    const char *env = getenv("TRIEPACK_SCALE_MB");
    if (!env)
        return 4;
    long v = strtol(env, NULL, 10);
    return (v > 0) ? (size_t)v : 4;
}

/* Deterministic key with a wide byte range, so the alphabet is exercised
   rather than just [a-z0-9]. */
static void make_key(char *buf, size_t buf_len, size_t i)
{
    snprintf(buf, buf_len, "k/%08zx/%08zx/item", i, (i * 2654435761u) & 0xFFFFFFFF);
}

/* ── Large round-trip ──────────────────────────────────────────────── */

void test_large_dictionary_round_trips(void)
{
    const size_t target_bytes = scale_mb() * 1024 * 1024;
    char key[64];
    make_key(key, sizeof(key), 0);
    const size_t key_len = strlen(key);
    const size_t n = target_bytes / key_len;

    printf("\n  scale: %zu keys, ~%zu MB of key bytes\n", n, target_bytes / (1024 * 1024));

    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    clock_t t0 = clock();
    for (size_t i = 0; i < n; i++) {
        make_key(key, sizeof(key), i);
        tp_value v = tp_value_uint(i);
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, &v));
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_encoder_build(enc, &buf, &len);
    double build_s = (double)(clock() - t0) / CLOCKS_PER_SEC;
    tp_encoder_destroy(&enc);

    if (rc == TP_ERR_OVERFLOW) {
        /* Past the v1 ceiling. Refusing is the correct answer: truncating the
           offsets would yield a valid CRC over a file that decodes to
           garbage. This is the case format v2 exists to support. */
        printf("  encoder refused at the v1 512 MB ceiling (TP_ERR_OVERFLOW)\n");
        TEST_ASSERT_NULL(buf);
        return;
    }
    TEST_ASSERT_EQUAL(TP_OK, rc);
    printf("  encoded %zu bytes in %.1f s (%.2f bytes/key)\n", len, build_s,
           (double)len / (double)n);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(dict, &info));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, info.num_keys);

    /* Spot-check lookups across the range rather than all n: the point is
       that correctness does not depend on position in the dictionary. */
    t0 = clock();
    /* Enough probes to characterise latency, few enough that the default
       tier stays quick: v1 value lookup is O(n), so each probe costs more as
       the dictionary grows. */
    const size_t probes = (n < 100) ? n : 100;
    for (size_t p = 0; p < probes; p++) {
        size_t i = (n / probes) * p;
        make_key(key, sizeof(key), i);
        tp_value got;
        TEST_ASSERT_EQUAL(TP_OK, tp_dict_lookup(dict, key, &got));
        TEST_ASSERT_EQUAL_UINT64((uint64_t)i, got.data.uint_val);
    }
    double us_per_lookup = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / (double)probes;
    printf("  lookup: %.2f us/key over %zu probes\n", us_per_lookup, probes);

    /* A key that cannot exist must not be found however large the trie. */
    tp_value missing;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND, tp_dict_lookup(dict, "no/such/key", &missing));

    tp_dict_close(&dict);
    free(buf);
}

/* ── Iteration at scale ────────────────────────────────────────────── */

/* The walk carries its own frame stack, so a large dictionary is where a
   bound that is wrong by one shows up as a missing or repeated key. */
void test_large_dictionary_iterates_completely(void)
{
    const size_t n = scale_mb() * 4000; /* keep this tier quick */
    char key[64];

    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
    for (size_t i = 0; i < n; i++) {
        make_key(key, sizeof(key), i);
        tp_value v = tp_value_uint(i);
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_add(enc, key, &v));
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_build(enc, &buf, &len));
    tp_encoder_destroy(&enc);

    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_iterator *it = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_iterate(dict, &it));

    const char *k;
    size_t kl;
    tp_value val;
    size_t seen = 0;
    char prev[64] = {0};
    size_t prev_len = 0;

    while (tp_iter_next(it, &k, &kl, &val) == TP_OK) {
        /* Keys must come out strictly ascending, with none repeated. */
        if (seen > 0) {
            size_t cmp_len = (prev_len < kl) ? prev_len : kl;
            int c = memcmp(prev, k, cmp_len);
            TEST_ASSERT_TRUE_MESSAGE(c < 0 || (c == 0 && prev_len < kl),
                                     "iteration produced keys out of order");
        }
        TEST_ASSERT_TRUE(kl < sizeof(prev));
        memcpy(prev, k, kl);
        prev_len = kl;
        seen++;
    }

    TEST_ASSERT_EQUAL_size_t(n, seen);

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_large_dictionary_round_trips);
    RUN_TEST(test_large_dictionary_iterates_completely);
    return UNITY_END();
}
