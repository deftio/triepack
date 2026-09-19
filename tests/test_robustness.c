/* test_robustness.c — real-world input that was never meant for us.
 *
 * The corpora in tests/data/ are ours: we chose them, and they are shaped the
 * way a trie likes. This file runs the library over whatever is sitting in
 * data/, which is gitignored and therefore whatever the developer happened to
 * drop there — currently enwik9, a gigabyte of Wikipedia XML from the Large
 * Text Compression Benchmark.
 *
 * The assertion is deliberately weak and deliberately important: **it must
 * not die.** Every outcome is acceptable except a crash, a hang, or a
 * silently wrong answer. Refusing the input is a fine answer; the alphabet
 * and size ceilings exist precisely so that inputs past them are refused
 * rather than silently corrupted.
 *
 * data/ is gitignored, so CI will not have it. A missing directory is a pass,
 * not a failure — this tests what is there when it is there.
 *
 * Size is tiered so the default stays quick:
 *
 *   (default)                     16 MB of the file, ~20 s
 *   TRIEPACK_ROBUSTNESS_MB=1024   the whole gigabyte
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */
#include "triepack/triepack.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DATA_DIR
#define DATA_DIR "data"
#endif

void setUp(void) {}
void tearDown(void) {}

static size_t budget_bytes(void)
{
    const char *env = getenv("TRIEPACK_ROBUSTNESS_MB");
    long mb = env ? strtol(env, NULL, 10) : 16;
    if (mb <= 0)
        mb = 16;
    return (size_t)mb * 1024 * 1024;
}

/* Any file in data/ will do; enwik9 is what is there today. */
static FILE *open_corpus(const char **name_out)
{
    static const char *candidates[] = {DATA_DIR "/enwik9", DATA_DIR "/enwik8",
                                       DATA_DIR "/corpus.txt"};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) {
            *name_out = candidates[i];
            return f;
        }
    }
    return NULL;
}

/* Is this one of the outcomes the format documents? Anything else means the
   library invented a failure mode. */
static int is_documented_result(tp_result rc)
{
    switch (rc) {
    case TP_OK:
    case TP_ERR_ALPHABET: /* more than 249 distinct byte values */
    case TP_ERR_OVERFLOW: /* data stream past the 512 MB ceiling */
    case TP_ERR_ALLOC:
        return 1;
    default:
        return 0;
    }
}

/* ── The corpus, whatever it is ─────────────────────────────────────── */

void test_real_world_corpus_does_not_die(void)
{
    const char *name = NULL;
    FILE *f = open_corpus(&name);
    if (!f) {
        TEST_PASS_MESSAGE("no corpus in " DATA_DIR "/ — skipping (it is gitignored)");
        return;
    }

    const size_t budget = budget_bytes();
    printf("\n  corpus: %s, reading up to %zu MB\n", name, budget / (1024 * 1024));

    tp_encoder *enc = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));

    const size_t LINE_MAX_BYTES = (size_t)1 << 20; /* enwik9 has very long lines */
    char *line = malloc(LINE_MAX_BYTES);
    TEST_ASSERT_NOT_NULL(line);

    size_t consumed = 0, nlines = 0;
    while (consumed < budget && fgets(line, (int)LINE_MAX_BYTES, f)) {
        size_t n = strlen(line);
        consumed += n;
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (!n)
            continue;
        tp_value v = tp_value_uint(nlines);
        tp_result rc = tp_encoder_add(enc, line, &v);
        /* add may legitimately fail on memory; anything else is a bug. */
        TEST_ASSERT_TRUE_MESSAGE(rc == TP_OK || rc == TP_ERR_ALLOC,
                                 "encoder_add invented a failure");
        if (rc != TP_OK)
            break;
        nlines++;
    }
    fclose(f);
    free(line);
    printf("  %zu lines added\n", nlines);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_encoder_build(enc, &buf, &len);
    tp_encoder_destroy(&enc);

    TEST_ASSERT_TRUE_MESSAGE(is_documented_result(rc), "build returned an undocumented result");

    if (rc != TP_OK) {
        /* A refusal must leave nothing behind and say why. */
        printf("  refused: %s (a documented limit, not a crash)\n", tp_result_str(rc));
        TEST_ASSERT_NULL(buf);
        return;
    }

    printf("  encoded %zu bytes from %zu MB of input\n", len, consumed / (1024 * 1024));

    /* If it built, it must be readable and internally consistent. */
    tp_dict *dict = NULL;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_open(&dict, buf, len));

    tp_dict_info info;
    TEST_ASSERT_EQUAL(TP_OK, tp_dict_get_info(dict, &info));
    TEST_ASSERT_TRUE(info.num_keys > 0);
    TEST_ASSERT_TRUE_MESSAGE((size_t)info.num_keys <= nlines,
                             "more keys than lines: duplicates were not collapsed");

    /* A key that cannot be there must not be found, whatever the corpus. */
    tp_value ignored;
    TEST_ASSERT_EQUAL(TP_ERR_NOT_FOUND,
                      tp_dict_lookup(dict, "\x01no-such-key-in-any-corpus\x01", &ignored));

    tp_dict_close(&dict);
    free(buf);
}

/* ── The same bytes, damaged ────────────────────────────────────────── */

/* Real files arrive truncated and corrupted. Opening such a thing must fail
   or succeed, never crash — this is the path a hostile file takes. */
void test_real_world_corpus_survives_damage(void)
{
    const char *name = NULL;
    FILE *f = open_corpus(&name);
    if (!f) {
        TEST_PASS_MESSAGE("no corpus in " DATA_DIR "/ — skipping");
        return;
    }

    /* A small slice is enough: this is about the decoder, not scale. */
    tp_encoder *enc = NULL;
    tp_encoder_create(&enc);
    char line[8192];
    size_t consumed = 0, nlines = 0;
    /* Small on purpose: this is about the decoder coping with damage, not
       about scale, and each damaged copy is walked in full. */
    const size_t SLICE_BYTES = (size_t)256 << 10;
    while (consumed < SLICE_BYTES && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        consumed += n;
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (!n)
            continue;
        tp_value v = tp_value_uint(nlines++);
        if (tp_encoder_add(enc, line, &v) != TP_OK)
            break;
    }
    fclose(f);

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_encoder_build(enc, &buf, &len);
    tp_encoder_destroy(&enc);
    if (rc != TP_OK) {
        TEST_PASS_MESSAGE("slice was refused; nothing to damage");
        return;
    }

    uint8_t *scratch = malloc(len);
    TEST_ASSERT_NOT_NULL(scratch);

    /* Truncation at many lengths. */
    for (size_t cut = 32; cut < len; cut += (len / 60) + 1) {
        memcpy(scratch, buf, cut);
        tp_dict *d = NULL;
        if (tp_dict_open(&d, scratch, cut) == TP_OK) {
            tp_value v;
            (void)tp_dict_lookup(d, "anything", &v);
            tp_dict_close(&d);
        }
    }

    /* Single-bit damage, spread across the file. */
    for (size_t byte = 0; byte < len; byte += (len / 120) + 1) {
        for (int bit = 0; bit < 8; bit += 3) {
            memcpy(scratch, buf, len);
            scratch[byte] ^= (uint8_t)(1u << bit);
            tp_dict *d = NULL;
            /* CRC will reject most of these; the unchecked path is where the
               walk itself has to cope. */
            if (tp_dict_open_unchecked(&d, scratch, len) == TP_OK) {
                tp_value v;
                (void)tp_dict_lookup(d, "anything", &v);
                tp_iterator *it = NULL;
                if (tp_dict_iterate(d, &it) == TP_OK) {
                    const char *k;
                    size_t kl;
                    unsigned steps = 0;
                    while (tp_iter_next(it, &k, &kl, &v) == TP_OK)
                        TEST_ASSERT_TRUE_MESSAGE(++steps < 200000, "iteration did not terminate");
                    tp_iter_destroy(&it);
                }
                tp_dict_close(&d);
            }
        }
    }

    free(scratch);
    free(buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_world_corpus_does_not_die);
    RUN_TEST(test_real_world_corpus_survives_damage);
    return UNITY_END();
}
