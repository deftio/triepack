/*
 * run_benchmarks.c
 *
 * Measures TriePack compression and lookup speed for the project's
 * static test data files.  Outputs a Markdown-friendly table.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"
#include "triepack/triepack_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────────────────── */

static uint8_t *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    *out_len = fread(buf, 1, (size_t)sz, f);
    buf[*out_len] = '\0';
    fclose(f);
    return buf;
}

static double now_ms(void)
{
    return (double)clock() / ((double)CLOCKS_PER_SEC / 1000.0);
}

/* ── Word list benchmark ─────────────────────────────────────────────── */

static void bench_wordlist(const char *path)
{
    size_t file_len = 0;
    uint8_t *data = read_file(path, &file_len);
    if (!data) {
        fprintf(stderr, "Cannot read %s\n", path);
        return;
    }

    /* Parse lines into words */
    char **words = NULL;
    size_t word_count = 0, word_cap = 0;
    size_t total_key_bytes = 0;
    char *line = strtok((char *)data, "\n\r");
    while (line) {
        size_t wlen = strlen(line);
        if (wlen > 0) {
            if (word_count >= word_cap) {
                word_cap = word_cap == 0 ? 1024 : word_cap * 2;
                words = realloc(words, word_cap * sizeof(char *));
            }
            words[word_count++] = line;
            total_key_bytes += wlen;
        }
        line = strtok(NULL, "\n\r");
    }

    printf("### %s\n\n", path);
    printf("- Words: %zu\n", word_count);
    printf("- Raw key bytes: %zu\n", total_key_bytes);
    printf("- File size: %zu bytes\n\n", file_len);

    /* === Keys only === */
    {
        tp_encoder *enc = NULL;
        tp_encoder_create(&enc);
        double t0 = now_ms();
        for (size_t i = 0; i < word_count; i++)
            tp_encoder_add(enc, words[i], NULL);
        uint8_t *buf = NULL;
        size_t len = 0;
        tp_encoder_build(enc, &buf, &len);
        double t_encode = now_ms() - t0;

        tp_dict *dict = NULL;
        tp_dict_open(&dict, buf, len);
        tp_dict_info info;
        tp_dict_get_info(dict, &info);

        t0 = now_ms();
        uint32_t found = 0;
        for (size_t i = 0; i < word_count; i++) {
            bool exists = false;
            if (tp_dict_contains(dict, words[i], &exists) == TP_OK && exists)
                found++;
        }
        double t_lookup = now_ms() - t0;
        double us_per_lookup = (t_lookup * 1000.0) / (double)word_count;

        printf("**Keys only (no values)**\n\n");
        printf("| Metric | Value |\n");
        printf("|--------|-------|\n");
        printf("| Bits per symbol | %u |\n", (unsigned)info.bits_per_symbol);
        printf("| Encoded size | %zu bytes |\n", len);
        printf("| Compression ratio | %.2fx |\n", (double)total_key_bytes / (double)len);
        printf("| Bits per key | %.1f |\n", (double)len * 8.0 / (double)word_count);
        printf("| Encode time | %.1f ms |\n", t_encode);
        printf("| Lookup time (%zu keys) | %.1f ms (%.2f us/key) |\n", word_count, t_lookup,
               us_per_lookup);
        printf("| Lookups verified | %u/%zu |\n\n", found, word_count);

        tp_dict_close(&dict);
        free(buf);
        tp_encoder_destroy(&enc);
    }

    /* === Keys + uint values === */
    {
        tp_encoder *enc = NULL;
        tp_encoder_create(&enc);
        double t0 = now_ms();
        for (size_t i = 0; i < word_count; i++) {
            tp_value v = tp_value_uint((uint64_t)i);
            tp_encoder_add(enc, words[i], &v);
        }
        uint8_t *buf = NULL;
        size_t len = 0;
        tp_encoder_build(enc, &buf, &len);
        double t_encode = now_ms() - t0;

        tp_dict *dict = NULL;
        tp_dict_open(&dict, buf, len);

        t0 = now_ms();
        uint32_t found = 0;
        for (size_t i = 0; i < word_count; i++) {
            tp_value val;
            if (tp_dict_lookup(dict, words[i], &val) == TP_OK)
                found++;
        }
        double t_lookup = now_ms() - t0;
        double us_per_lookup = (t_lookup * 1000.0) / (double)word_count;

        printf("**Keys + integer values**\n\n");
        printf("| Metric | Value |\n");
        printf("|--------|-------|\n");
        printf("| Encoded size | %zu bytes |\n", len);
        printf("| Compression ratio | %.2fx |\n", (double)total_key_bytes / (double)len);
        printf("| Bits per key | %.1f |\n", (double)len * 8.0 / (double)word_count);
        printf("| Encode time | %.1f ms |\n", t_encode);
        printf("| Lookup time (%zu keys) | %.1f ms (%.2f us/key) |\n", word_count, t_lookup,
               us_per_lookup);
        printf("| Lookups verified | %u/%zu |\n\n", found, word_count);

        tp_dict_close(&dict);
        free(buf);
        tp_encoder_destroy(&enc);
    }

    free(words);
    free(data);
}

/* ── JSON benchmark ──────────────────────────────────────────────────── */

static void bench_json(const char *path)
{
    size_t file_len = 0;
    uint8_t *data = read_file(path, &file_len);
    if (!data) {
        fprintf(stderr, "Cannot read %s\n", path);
        return;
    }

    printf("### %s\n\n", path);
    printf("- JSON size: %zu bytes\n\n", file_len);

    /* Encode */
    double t0 = now_ms();
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_json_encode((const char *)data, file_len, &buf, &len);
    double t_encode = now_ms() - t0;

    if (rc != TP_OK) {
        fprintf(stderr, "JSON encode failed: %s\n", tp_result_str(rc));
        free(data);
        return;
    }

    /* Decode back to JSON */
    char *json_out = NULL;
    size_t json_len = 0;
    t0 = now_ms();
    rc = tp_json_decode(buf, len, &json_out, &json_len);
    double t_decode = now_ms() - t0;

    /* Count keys */
    tp_dict *dict = NULL;
    tp_dict_open(&dict, buf, len);
    uint32_t num_keys = tp_dict_count(dict);
    tp_dict_info info;
    tp_dict_get_info(dict, &info);

    /* Lookup benchmark */
    t0 = now_ms();
    size_t lookup_count = 100;
    for (size_t rep = 0; rep < lookup_count; rep++) {
        tp_value val;
        tp_dict_lookup(dict, "catalog_version", &val);
    }
    double t_lookup = now_ms() - t0;

    printf("| Metric | Value |\n");
    printf("|--------|-------|\n");
    printf("| JSON input size | %zu bytes |\n", file_len);
    printf("| Encoded .trp size | %zu bytes |\n", len);
    printf("| Compression | %.1f%% of JSON |\n", (double)len * 100.0 / (double)file_len);
    printf("| Flattened keys | %u |\n", num_keys);
    printf("| Bits per symbol | %u |\n", (unsigned)info.bits_per_symbol);
    printf("| Encode time | %.1f ms |\n", t_encode);
    printf("| Decode time | %.1f ms |\n", t_decode);
    printf("| Single lookup (avg of %zu) | %.2f us |\n\n", lookup_count,
           t_lookup * 1000.0 / (double)lookup_count);

    tp_dict_close(&dict);
    free(json_out);
    free(buf);
    free(data);
}

/* ── Small JSON examples ─────────────────────────────────────────────── */

static void bench_small_json(void)
{
    printf("### Small JSON Documents\n\n");
    printf("| Document | JSON bytes | .trp bytes | Ratio | Keys |\n");
    printf("|----------|-----------|-----------|-------|------|\n");

    struct {
        const char *name;
        const char *json;
    } docs[] = {
        {"Empty object", "{}"},
        {"Simple config", "{\"name\":\"triepack\",\"version\":1,\"debug\":false}"},
        {"Nested object",
         "{\"db\":{\"host\":\"localhost\",\"port\":5432},\"cache\":{\"ttl\":300}}"},
        {"Small array", "{\"items\":[\"alpha\",\"beta\",\"gamma\",\"delta\"]}"},
        {"Mixed types",
         "{\"str\":\"hello\",\"int\":42,\"float\":3.14,\"bool\":true,\"null\":null}"},
        {"App config",
         "{\"app\":{\"name\":\"TriePack Demo\",\"version\":\"2.1.0\",\"debug\":false},"
         "\"database\":{\"host\":\"db.example.com\",\"port\":5432,"
         "\"credentials\":{\"username\":\"admin\",\"password\":null},"
         "\"pool\":{\"max_connections\":20,\"min_connections\":2,\"timeout_ms\":5000}},"
         "\"features\":[\"auth\",\"logging\",\"metrics\",\"caching\"],"
         "\"limits\":{\"requests_per_second\":10000,"
         "\"rate_limit\":{\"max_requests\":1000,\"window_seconds\":60}}}"},
    };
    size_t ndocs = sizeof(docs) / sizeof(docs[0]);

    for (size_t i = 0; i < ndocs; i++) {
        size_t json_len = strlen(docs[i].json);
        uint8_t *buf = NULL;
        size_t len = 0;
        tp_result rc = tp_json_encode(docs[i].json, json_len, &buf, &len);
        if (rc != TP_OK)
            continue;

        tp_dict *dict = NULL;
        tp_dict_open(&dict, buf, len);
        uint32_t num_keys = tp_dict_count(dict);

        printf("| %s | %zu | %zu | %.0f%% | %u |\n", docs[i].name, json_len, len,
               (double)len * 100.0 / (double)json_len, num_keys);

        tp_dict_close(&dict);
        free(buf);
    }
    printf("\n");
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    const char *data_dir = "tests/data";
    if (argc > 1)
        data_dir = argv[1];

    char path[4096];

    printf("# TriePack Benchmark Results\n\n");

    /* Word list */
    snprintf(path, sizeof(path), "%s/common_words_10k.txt", data_dir);
    bench_wordlist(path);

    printf("---\n\n");

    /* JSON */
    snprintf(path, sizeof(path), "%s/benchmark_100k.json", data_dir);
    bench_json(path);

    printf("---\n\n");

    /* Small JSON examples */
    bench_small_json();

    return 0;
}
