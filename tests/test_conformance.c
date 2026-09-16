/**
 * @file test_conformance.c
 * @brief Run the C library against the cross-language conformance corpus.
 *
 * Reads tests/conformance/cases.txt — the corpus every implementation shares —
 * and checks the C library against the fixtures generated from it: every key
 * must look up to the corpus value, and re-encoding the corpus must reproduce
 * the fixture byte for byte.
 *
 * See tests/conformance/README.md.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/triepack.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define MAX_LINE  8192
#define MAX_TOKEN 4096
#define MAX_KEYS  2048
#define MAX_BLOBS 4096

/* ── Token decoding (mirrors tools/generate_conformance.c) ───────────── */

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static long token_decode(const char *tok, uint8_t *out, size_t out_cap)
{
    if (strcmp(tok, "~") == 0)
        return 0;
    size_t n = 0;
    for (const char *p = tok; *p;) {
        if (n >= out_cap)
            return -1;
        if (*p == '%') {
            int hi = hexval((unsigned char)p[1]);
            int lo = hi < 0 ? -1 : hexval((unsigned char)p[2]);
            if (lo < 0)
                return -1;
            out[n++] = (uint8_t)((hi << 4) | lo);
            p += 3;
        } else {
            out[n++] = (uint8_t)*p++;
        }
    }
    return (long)n;
}

static long hex_decode(const char *tok, uint8_t *out, size_t out_cap)
{
    if (strcmp(tok, "~") == 0)
        return 0;
    size_t len = strlen(tok);
    if (len % 2 != 0 || len / 2 > out_cap)
        return -1;
    for (size_t i = 0; i < len; i += 2)
        out[i / 2] =
            (uint8_t)((hexval((unsigned char)tok[i]) << 4) | hexval((unsigned char)tok[i + 1]));
    return (long)(len / 2);
}

static uint64_t hex_u64(const char *tok)
{
    uint64_t v = 0;
    for (const char *p = tok; *p; p++)
        v = (v << 4) | (uint64_t)hexval((unsigned char)*p);
    return v;
}

/* ── Corpus model ────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *key;
    size_t key_len;
    tp_value val;
} conf_key;

typedef struct {
    char name[256];
    bool encode_expected;
    conf_key keys[MAX_KEYS];
    size_t key_count;
    uint8_t *owned[MAX_BLOBS];
    size_t owned_count;
} conf_case;

/* Sized so every path below provably fits: g_root + a 256-byte case
   name + the longest literal, which gcc checks with
   -Wformat-truncation. */
#define CONF_ROOT_MAX 512
#define CONF_PATH_MAX (CONF_ROOT_MAX + 256 + 128)

static char g_root[CONF_ROOT_MAX];

static void find_root(void)
{
    /* FIXTURE_DIR is the source tree; fall back to the working directory. */
    snprintf(g_root, sizeof(g_root), "%s", FIXTURE_DIR);
    char probe[CONF_PATH_MAX];
    snprintf(probe, sizeof(probe), "%s/tests/conformance/cases.txt", g_root);
    FILE *f = fopen(probe, "rb");
    if (f) {
        fclose(f);
        return;
    }
    snprintf(g_root, sizeof(g_root), ".");
}

static void case_free(conf_case *c)
{
    for (size_t i = 0; i < c->key_count; i++)
        free(c->keys[i].key);
    for (size_t i = 0; i < c->owned_count; i++)
        free(c->owned[i]);
    c->key_count = 0;
    c->owned_count = 0;
}

static uint8_t *case_own(conf_case *c, const uint8_t *src, size_t len)
{
    uint8_t *p = malloc(len ? len : 1);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, src, len);
    TEST_ASSERT_TRUE(c->owned_count < MAX_BLOBS);
    c->owned[c->owned_count++] = p;
    return p;
}

static uint8_t *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(sz > 0 ? (size_t)sz : 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = sz > 0 ? fread(buf, 1, (size_t)sz, f) : 0;
    fclose(f);
    *out_len = got;
    return buf;
}

/* ── Value comparison ────────────────────────────────────────────────── */

static bool values_match(const tp_value *got, const tp_value *want)
{
    /* float32 corpus entries are widened to double by the generator, so the
       fixture carries the float32 tag while the corpus value is a double. */
    if (want->type == TP_FLOAT64 && got->type == TP_FLOAT32) {
        double g = (double)got->data.float32_val;
        double w = want->data.float64_val;
        return (isnan(g) && isnan(w)) || g == w;
    }
    if (got->type != want->type)
        return false;

    switch (got->type) {
    case TP_NULL:
        return true;
    case TP_BOOL:
        return got->data.bool_val == want->data.bool_val;
    case TP_INT:
        return got->data.int_val == want->data.int_val;
    case TP_UINT:
        return got->data.uint_val == want->data.uint_val;
    case TP_FLOAT32: {
        float g = got->data.float32_val, w = want->data.float32_val;
        return (isnan(g) && isnan(w)) || memcmp(&g, &w, sizeof(g)) == 0;
    }
    case TP_FLOAT64: {
        double g = got->data.float64_val, w = want->data.float64_val;
        /* Compare by bit pattern so -0.0 is distinguished; NaNs all match. */
        return (isnan(g) && isnan(w)) || memcmp(&g, &w, sizeof(g)) == 0;
    }
    case TP_STRING:
        return got->data.string_val.str_len == want->data.string_val.str_len &&
               memcmp(got->data.string_val.str, want->data.string_val.str,
                      got->data.string_val.str_len) == 0;
    case TP_BLOB:
        return got->data.blob_val.len == want->data.blob_val.len &&
               (got->data.blob_val.len == 0 ||
                memcmp(got->data.blob_val.data, want->data.blob_val.data, got->data.blob_val.len) ==
                    0);
    default:
        return false;
    }
}

/* ── Per-case checks ─────────────────────────────────────────────────── */

static void check_case(conf_case *c)
{
    char path[CONF_PATH_MAX];
    snprintf(path, sizeof(path), "%s/tests/conformance/fixtures/%s.trp", g_root, c->name);

    size_t len = 0;
    uint8_t *fixture = read_file(path, &len);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, c->name);

    /* 1. Every corpus key looks up to its corpus value. */
    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, fixture, len);
    TEST_ASSERT_EQUAL_MESSAGE(TP_OK, rc, c->name);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)c->key_count, tp_dict_count(dict), c->name);

    for (size_t i = 0; i < c->key_count; i++) {
        tp_value got;
        rc = tp_dict_lookup_n(dict, (const char *)c->keys[i].key, c->keys[i].key_len, &got);
        TEST_ASSERT_EQUAL_MESSAGE(TP_OK, rc, c->name);
        TEST_ASSERT_TRUE_MESSAGE(values_match(&got, &c->keys[i].val), c->name);
    }
    tp_dict_close(&dict);

    /* 2. Re-encoding the corpus reproduces the fixture byte for byte. */
    if (c->encode_expected) {
        tp_encoder *enc = NULL;
        TEST_ASSERT_EQUAL(TP_OK, tp_encoder_create(&enc));
        for (size_t i = 0; i < c->key_count; i++) {
            rc = tp_encoder_add_n(enc, (const char *)c->keys[i].key, c->keys[i].key_len,
                                  &c->keys[i].val);
            TEST_ASSERT_EQUAL_MESSAGE(TP_OK, rc, c->name);
        }
        uint8_t *built = NULL;
        size_t built_len = 0;
        rc = tp_encoder_build(enc, &built, &built_len);
        TEST_ASSERT_EQUAL_MESSAGE(TP_OK, rc, c->name);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(len, built_len, c->name);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(fixture, built, len, c->name);
        free(built);
        tp_encoder_destroy(&enc);
    }

    free(fixture);
}

/* ── Corpus walk ─────────────────────────────────────────────────────── */

static int g_cases_seen = 0;

static void run_corpus(void)
{
    char path[CONF_PATH_MAX];
    snprintf(path, sizeof(path), "%s/tests/conformance/cases.txt", g_root);
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open the conformance corpus");

    conf_case cur;
    memset(&cur, 0, sizeof(cur));
    bool have_case = false;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        size_t l = strlen(p);
        while (l > 0 && (p[l - 1] == '\n' || p[l - 1] == '\r'))
            p[--l] = '\0';
        if (l == 0 || *p == '#')
            continue;

        char *verb = strtok(p, " ");
        if (!verb)
            continue;

        if (strcmp(verb, "case") == 0) {
            if (have_case) {
                check_case(&cur);
                g_cases_seen++;
                case_free(&cur);
            }
            memset(&cur, 0, sizeof(cur));
            have_case = true;
            char *nm = strtok(NULL, " ");
            TEST_ASSERT_NOT_NULL(nm);
            snprintf(cur.name, sizeof(cur.name), "%s", nm);
            cur.encode_expected = true;
            for (char *flag = strtok(NULL, " "); flag; flag = strtok(NULL, " ")) {
                if (strcmp(flag, "encode=no") == 0)
                    cur.encode_expected = false;
            }
            continue;
        }

        TEST_ASSERT_EQUAL_STRING("key", verb);
        char *key_tok = strtok(NULL, " ");
        char *type_tok = strtok(NULL, " ");
        char *arg_tok = strtok(NULL, " ");
        TEST_ASSERT_NOT_NULL(key_tok);
        TEST_ASSERT_NOT_NULL(type_tok);

        uint8_t key[MAX_TOKEN];
        long key_len = token_decode(key_tok, key, sizeof(key));
        TEST_ASSERT_GREATER_OR_EQUAL(0, key_len);

        tp_value val;
        if (strcmp(type_tok, "null") == 0) {
            val = tp_value_null();
        } else if (strcmp(type_tok, "bool") == 0) {
            val = tp_value_bool(arg_tok && arg_tok[0] == '1');
        } else if (strcmp(type_tok, "int") == 0) {
            val = tp_value_int(strtoll(arg_tok, NULL, 10));
        } else if (strcmp(type_tok, "uint") == 0) {
            val = tp_value_uint(strtoull(arg_tok, NULL, 10));
        } else if (strcmp(type_tok, "f32") == 0) {
            /* The corpus records float32 bits; compare against the widened
               double the reader hands back. */
            uint32_t bits = (uint32_t)hex_u64(arg_tok);
            float fv;
            memcpy(&fv, &bits, sizeof(fv));
            val = tp_value_float64((double)fv);
        } else if (strcmp(type_tok, "f64") == 0) {
            uint64_t bits = hex_u64(arg_tok);
            double dv;
            memcpy(&dv, &bits, sizeof(dv));
            val = tp_value_float64(dv);
        } else if (strcmp(type_tok, "str") == 0) {
            uint8_t tmp[MAX_TOKEN];
            long n = token_decode(arg_tok, tmp, sizeof(tmp));
            TEST_ASSERT_GREATER_OR_EQUAL(0, n);
            val = tp_value_string_n((const char *)case_own(&cur, tmp, (size_t)n), (size_t)n);
        } else if (strcmp(type_tok, "blob") == 0) {
            uint8_t tmp[MAX_TOKEN];
            long n = hex_decode(arg_tok, tmp, sizeof(tmp));
            TEST_ASSERT_GREATER_OR_EQUAL(0, n);
            val = tp_value_blob(case_own(&cur, tmp, (size_t)n), (size_t)n);
        } else {
            TEST_FAIL_MESSAGE(type_tok);
            continue;
        }

        TEST_ASSERT_TRUE(cur.key_count < MAX_KEYS);
        conf_key *k = &cur.keys[cur.key_count++];
        k->key = malloc((size_t)key_len ? (size_t)key_len : 1);
        TEST_ASSERT_NOT_NULL(k->key);
        memcpy(k->key, key, (size_t)key_len);
        k->key_len = (size_t)key_len;
        k->val = val;
    }

    if (have_case) {
        check_case(&cur);
        g_cases_seen++;
        case_free(&cur);
    }
    fclose(f);
}

/* ── Malformed inputs ────────────────────────────────────────────────── */

/* Each of these is a valid fixture with one field damaged. Damage inside the
   data is re-sealed with a correct CRC, so the reader has to catch it rather
   than being handed a checksum failure. Kept in step with
   tools/gen_malformed_fixtures.py. */
static const char *const MALFORMED[] = {
    "bad_crc.trp",
    "bad_magic.trp",
    "bad_version.trp",
    "bps_max.trp",
    "bps_too_wide.trp",
    "bps_zero.trp",
    "symbol_count_below_control.trp",
    "symbol_count_exceeds_bps.trp",
    "symbol_count_zero.trp",
    "truncated_body.trp",
    "truncated_header.trp",
};

/* ── Tests ───────────────────────────────────────────────────────────── */

static void test_conformance_corpus(void)
{
    find_root();
    run_corpus();
    /* Guard against silently running an empty corpus. */
    TEST_ASSERT_GREATER_THAN_INT(20, g_cases_seen);
}

static void test_conformance_rejects_malformed(void)
{
    find_root();
    for (size_t i = 0; i < sizeof(MALFORMED) / sizeof(MALFORMED[0]); i++) {
        char path[CONF_PATH_MAX];
        snprintf(path, sizeof(path), "%s/tests/conformance/malformed/%s", g_root, MALFORMED[i]);

        size_t len = 0;
        uint8_t *buf = read_file(path, &len);
        TEST_ASSERT_NOT_NULL_MESSAGE(buf, MALFORMED[i]);

        tp_dict *dict = NULL;
        tp_result rc = tp_dict_open(&dict, buf, len);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(TP_OK, rc, MALFORMED[i]);
        if (rc == TP_OK)
            tp_dict_close(&dict);
        free(buf);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_conformance_corpus);
    RUN_TEST(test_conformance_rejects_malformed);
    return UNITY_END();
}
