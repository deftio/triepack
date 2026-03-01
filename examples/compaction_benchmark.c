/*
 * compaction_benchmark.c
 *
 * Generates ~10,000 unique words programmatically and measures
 * TriePack compression ratio.  No external word-list file needed.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Base words ─────────────────────────────────────────────────────── */

static const char *const BASE_WORDS[] = {
    "abandon",  "abstract", "accept",  "account", "achieve", "action",   "address", "advance",
    "agree",    "allow",    "amount",  "answer",  "appear",  "apply",    "balance", "believe",
    "benefit",  "beyond",   "border",  "branch",  "break",   "build",    "capture", "center",
    "change",   "chapter",  "charge",  "circle",  "claim",   "clear",    "collect", "column",
    "combine",  "comment",  "commit",  "common",  "compare", "complete", "concern", "connect",
    "consider", "contain",  "control", "convert", "correct", "counter",  "cover",   "create",
    "current",  "custom",   "deliver", "demand",  "design",  "detail",   "develop", "direct",
    "display",  "double",   "dream",   "effect",
};
#define NUM_BASE_WORDS (sizeof(BASE_WORDS) / sizeof(BASE_WORDS[0]))

static const char *const PREFIXES[] = {
    "un", "re", "pre", "over", "mis", "out", "sub", "dis", "non", "co",
};
#define NUM_PREFIXES (sizeof(PREFIXES) / sizeof(PREFIXES[0]))

static const char *const SUFFIXES[] = {
    "ing", "tion", "ness", "able", "ment", "ful", "less", "ize", "ous", "ive",
    "ed",  "er",   "est",  "ly",   "al",   "ity", "ism",  "ist", "ure", "ance",
};
#define NUM_SUFFIXES (sizeof(SUFFIXES) / sizeof(SUFFIXES[0]))

/* Short stems for length variety (3-6 chars) */
static const char *const SHORT_STEMS[] = {
    "act", "add", "age", "aid", "aim", "air", "art", "ask", "bar", "bat", "bed", "big", "bit",
    "box", "bus", "buy", "cap", "car", "cat", "cup", "cut", "day", "dig", "dog", "dot", "dry",
    "ear", "eat", "end", "eye", "fan", "far", "fat", "few", "fit", "fix", "fly", "fun", "gap",
    "gas", "get", "god", "got", "gun", "gut", "hat", "hit", "hot", "ill", "jam",
};
#define NUM_SHORT_STEMS (sizeof(SHORT_STEMS) / sizeof(SHORT_STEMS[0]))

/* ── Unique word set using a simple hash table ─────────────────────── */

#define HASH_SIZE 32768

typedef struct word_node {
    char *word;
    struct word_node *next;
} word_node;

typedef struct word_set {
    word_node *buckets[HASH_SIZE];
    char **words;
    size_t count;
    size_t cap;
    size_t total_key_bytes;
} word_set;

static uint32_t djb2(const char *str)
{
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (uint32_t)(unsigned char)*str;
        str++;
    }
    return hash;
}

static bool word_set_contains(word_set *ws, const char *word)
{
    uint32_t idx = djb2(word) & (HASH_SIZE - 1);
    for (word_node *n = ws->buckets[idx]; n; n = n->next) {
        if (strcmp(n->word, word) == 0)
            return true;
    }
    return false;
}

static bool word_set_add(word_set *ws, const char *word)
{
    if (word_set_contains(ws, word))
        return false;

    /* Add to hash table */
    uint32_t idx = djb2(word) & (HASH_SIZE - 1);
    size_t wlen = strlen(word);
    char *copy = malloc(wlen + 1);
    if (!copy)
        return false;
    memcpy(copy, word, wlen + 1);

    word_node *n = malloc(sizeof(word_node));
    if (!n) {
        free(copy);
        return false;
    }
    n->word = copy;
    n->next = ws->buckets[idx];
    ws->buckets[idx] = n;

    /* Add to flat list */
    if (ws->count >= ws->cap) {
        size_t new_cap = ws->cap == 0 ? 256 : ws->cap * 2;
        char **new_words = realloc(ws->words, new_cap * sizeof(char *));
        if (!new_words)
            return false;
        ws->words = new_words;
        ws->cap = new_cap;
    }
    ws->words[ws->count] = copy;
    ws->count++;
    ws->total_key_bytes += wlen;
    return true;
}

static void word_set_free(word_set *ws)
{
    for (int i = 0; i < HASH_SIZE; i++) {
        word_node *n = ws->buckets[i];
        while (n) {
            word_node *next = n->next;
            free(n->word);
            free(n);
            n = next;
        }
    }
    free(ws->words);
}

/* ── Word generation ────────────────────────────────────────────────── */

static void generate_words(word_set *ws, size_t target)
{
    char buf[128];

    /* 1. Base words as-is */
    for (size_t i = 0; i < NUM_BASE_WORDS && ws->count < target; i++) {
        word_set_add(ws, BASE_WORDS[i]);
    }

    /* 2. Base words + suffixes */
    for (size_t i = 0; i < NUM_BASE_WORDS && ws->count < target; i++) {
        for (size_t j = 0; j < NUM_SUFFIXES && ws->count < target; j++) {
            snprintf(buf, sizeof(buf), "%s%s", BASE_WORDS[i], SUFFIXES[j]);
            word_set_add(ws, buf);
        }
    }

    /* 3. Prefixed base words */
    for (size_t i = 0; i < NUM_PREFIXES && ws->count < target; i++) {
        for (size_t j = 0; j < NUM_BASE_WORDS && ws->count < target; j++) {
            snprintf(buf, sizeof(buf), "%s%s", PREFIXES[i], BASE_WORDS[j]);
            word_set_add(ws, buf);
        }
    }

    /* 4. Prefixed base words + suffixes */
    for (size_t i = 0; i < NUM_PREFIXES && ws->count < target; i++) {
        for (size_t j = 0; j < NUM_BASE_WORDS && ws->count < target; j++) {
            for (size_t k = 0; k < NUM_SUFFIXES && ws->count < target; k++) {
                snprintf(buf, sizeof(buf), "%s%s%s", PREFIXES[i], BASE_WORDS[j], SUFFIXES[k]);
                word_set_add(ws, buf);
            }
        }
    }

    /* 5. Short stems + suffixes */
    for (size_t i = 0; i < NUM_SHORT_STEMS && ws->count < target; i++) {
        word_set_add(ws, SHORT_STEMS[i]);
        for (size_t j = 0; j < NUM_SUFFIXES && ws->count < target; j++) {
            snprintf(buf, sizeof(buf), "%s%s", SHORT_STEMS[i], SUFFIXES[j]);
            word_set_add(ws, buf);
        }
    }

    /* 6. Prefixed short stems */
    for (size_t i = 0; i < NUM_PREFIXES && ws->count < target; i++) {
        for (size_t j = 0; j < NUM_SHORT_STEMS && ws->count < target; j++) {
            snprintf(buf, sizeof(buf), "%s%s", PREFIXES[i], SHORT_STEMS[j]);
            word_set_add(ws, buf);
            for (size_t k = 0; k < NUM_SUFFIXES && ws->count < target; k++) {
                snprintf(buf, sizeof(buf), "%s%s%s", PREFIXES[i], SHORT_STEMS[j], SUFFIXES[k]);
                word_set_add(ws, buf);
            }
        }
    }
}

/* ── Main benchmark ─────────────────────────────────────────────────── */

int main(void)
{
    word_set ws;
    memset(&ws, 0, sizeof(ws));

    printf("TriePack Compaction Benchmark\n");
    printf("─────────────────────────────\n");

    /* Generate words */
    generate_words(&ws, 10000);

    printf("Keys:              %zu\n", ws.count);
    printf("Raw key bytes:     %zu\n", ws.total_key_bytes);
    printf("\n");

    /* --- Encode keys-only (NULL values) --- */
    {
        tp_encoder *enc = NULL;
        tp_result rc = tp_encoder_create(&enc);
        if (rc != TP_OK) {
            fprintf(stderr, "encoder_create failed: %s\n", tp_result_str(rc));
            word_set_free(&ws);
            return 1;
        }

        for (size_t i = 0; i < ws.count; i++) {
            rc = tp_encoder_add(enc, ws.words[i], NULL);
            if (rc != TP_OK) {
                fprintf(stderr, "encoder_add failed: %s\n", tp_result_str(rc));
                tp_encoder_destroy(&enc);
                word_set_free(&ws);
                return 1;
            }
        }

        uint8_t *buf = NULL;
        size_t len = 0;
        rc = tp_encoder_build(enc, &buf, &len);
        if (rc != TP_OK) {
            fprintf(stderr, "encoder_build failed: %s\n", tp_result_str(rc));
            tp_encoder_destroy(&enc);
            word_set_free(&ws);
            return 1;
        }

        /* Get info */
        tp_dict *dict = NULL;
        rc = tp_dict_open(&dict, buf, len);
        if (rc != TP_OK) {
            fprintf(stderr, "dict_open failed: %s\n", tp_result_str(rc));
            free(buf);
            tp_encoder_destroy(&enc);
            word_set_free(&ws);
            return 1;
        }

        tp_dict_info info;
        tp_dict_get_info(dict, &info);

        double ratio = ws.total_key_bytes > 0 ? (double)ws.total_key_bytes / (double)len : 0.0;
        double bits_per_key = ws.count > 0 ? (double)len * 8.0 / (double)ws.count : 0.0;

        printf("=== Keys Only (no values) ===\n");
        printf("Bits per symbol:   %u\n", (unsigned)info.bits_per_symbol);
        printf("Encoded blob:      %zu bytes\n", len);
        printf("Compression ratio: %.2fx\n", ratio);
        printf("Bits per key:      %.1f\n", bits_per_key);

        /* Verify all lookups */
        uint32_t verified = 0;
        for (size_t i = 0; i < ws.count; i++) {
            bool found = false;
            rc = tp_dict_contains(dict, ws.words[i], &found);
            if (rc == TP_OK && found)
                verified++;
        }
        printf("Lookup verified:   %u/%zu OK\n", verified, ws.count);
        printf("\n");

        tp_dict_close(&dict);
        free(buf);
        tp_encoder_destroy(&enc);
    }

    /* --- Encode keys with integer values --- */
    {
        tp_encoder *enc = NULL;
        tp_result rc = tp_encoder_create(&enc);
        if (rc != TP_OK) {
            fprintf(stderr, "encoder_create failed: %s\n", tp_result_str(rc));
            word_set_free(&ws);
            return 1;
        }

        for (size_t i = 0; i < ws.count; i++) {
            tp_value v = tp_value_uint((uint64_t)i);
            rc = tp_encoder_add(enc, ws.words[i], &v);
            if (rc != TP_OK) {
                fprintf(stderr, "encoder_add failed: %s\n", tp_result_str(rc));
                tp_encoder_destroy(&enc);
                word_set_free(&ws);
                return 1;
            }
        }

        uint8_t *buf = NULL;
        size_t len = 0;
        rc = tp_encoder_build(enc, &buf, &len);
        if (rc != TP_OK) {
            fprintf(stderr, "encoder_build failed: %s\n", tp_result_str(rc));
            tp_encoder_destroy(&enc);
            word_set_free(&ws);
            return 1;
        }

        tp_dict *dict = NULL;
        rc = tp_dict_open(&dict, buf, len);
        if (rc != TP_OK) {
            fprintf(stderr, "dict_open failed: %s\n", tp_result_str(rc));
            free(buf);
            tp_encoder_destroy(&enc);
            word_set_free(&ws);
            return 1;
        }

        double ratio = ws.total_key_bytes > 0 ? (double)ws.total_key_bytes / (double)len : 0.0;
        double bits_per_key = ws.count > 0 ? (double)len * 8.0 / (double)ws.count : 0.0;

        printf("=== Keys + uint values ===\n");
        printf("Encoded blob:      %zu bytes\n", len);
        printf("Compression ratio: %.2fx\n", ratio);
        printf("Bits per key:      %.1f\n", bits_per_key);

        /* Spot-check a few lookups */
        uint32_t verified = 0;
        for (size_t i = 0; i < ws.count; i++) {
            tp_value val;
            rc = tp_dict_lookup(dict, ws.words[i], &val);
            if (rc == TP_OK)
                verified++;
        }
        printf("Lookup verified:   %u/%zu OK\n", verified, ws.count);
        printf("\n");

        tp_dict_close(&dict);
        free(buf);
        tp_encoder_destroy(&enc);
    }

    printf("Done.\n");
    word_set_free(&ws);
    return 0;
}
