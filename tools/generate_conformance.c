/**
 * @file generate_conformance.c
 * @brief Turn tests/conformance/cases.txt into .trp fixtures.
 *
 * The C library is the reference encoder: every binding checks that it
 * produces these exact bytes and reads them back to the same values.
 *
 * Usage: generate_conformance <cases.txt> <output-dir>
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/triepack.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE   8192
#define MAX_TOKEN  4096
#define MAX_BLOBS  4096

/* ── Token decoding ──────────────────────────────────────────────────── */

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

/**
 * Decode a corpus token ("~" for empty, %XX escapes) into raw bytes.
 * Returns the byte length, or -1 on a malformed token.
 */
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

/** Decode a run of hex digit pairs ("~" for empty) into raw bytes. */
static long hex_decode(const char *tok, uint8_t *out, size_t out_cap)
{
    if (strcmp(tok, "~") == 0)
        return 0;
    size_t len = strlen(tok);
    if (len % 2 != 0 || len / 2 > out_cap)
        return -1;
    for (size_t i = 0; i < len; i += 2) {
        int hi = hexval((unsigned char)tok[i]);
        int lo = hexval((unsigned char)tok[i + 1]);
        if (hi < 0 || lo < 0)
            return -1;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return (long)(len / 2);
}

static uint64_t hex_u64(const char *tok)
{
    uint64_t v = 0;
    for (const char *p = tok; *p; p++) {
        int h = hexval((unsigned char)*p);
        if (h < 0)
            return v;
        v = (v << 4) | (uint64_t)h;
    }
    return v;
}

/* ── Fixture writing ─────────────────────────────────────────────────── */

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open %s for writing\n", path);
        return 1;
    }
    int ok = fwrite(buf, 1, len, f) == len;
    fclose(f);
    if (!ok) {
        fprintf(stderr, "ERROR: short write to %s\n", path);
        return 1;
    }
    return 0;
}

/* Values hold pointers into caller-owned storage, so strings and blobs added
   to the encoder must stay alive until the build. Keep them in one arena. */
typedef struct {
    uint8_t *blocks[MAX_BLOBS];
    size_t count;
} arena;

static uint8_t *arena_take(arena *a, const uint8_t *src, size_t len)
{
    if (a->count >= MAX_BLOBS)
        return NULL;
    uint8_t *p = malloc(len ? len : 1);
    if (!p)
        return NULL;
    memcpy(p, src, len);
    a->blocks[a->count++] = p;
    return p;
}

static void arena_free(arena *a)
{
    for (size_t i = 0; i < a->count; i++)
        free(a->blocks[i]);
    a->count = 0;
}

static int flush_case(const char *name, const char *dir, tp_encoder *enc, arena *a)
{
    if (!name[0])
        return 0;

    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_encoder_build(enc, &buf, &len);
    if (rc != TP_OK) {
        fprintf(stderr, "ERROR: case '%s': build failed: %s\n", name, tp_result_str(rc));
        return 1;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.trp", dir, name);
    int ret = write_file(path, buf, len);
    if (ret == 0)
        printf("  %-32s %6zu bytes\n", name, len);
    free(buf);
    arena_free(a);
    return ret;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <cases.txt> <output-dir>\n", argv[0]);
        return 2;
    }
    const char *cases_path = argv[1];
    const char *out_dir = argv[2];

    FILE *f = fopen(cases_path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open %s\n", cases_path);
        return 1;
    }

    char line[MAX_LINE];
    char cur_name[256] = {0};
    tp_encoder *enc = NULL;
    arena a = {{0}, 0};
    int failures = 0;
    int cases = 0;
    unsigned long lineno = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        /* Strip the line ending. */
        size_t l = strlen(p);
        while (l > 0 && (p[l - 1] == '\n' || p[l - 1] == '\r'))
            p[--l] = '\0';
        if (l == 0 || *p == '#')
            continue;

        char *verb = strtok(p, " ");
        if (!verb)
            continue;

        if (strcmp(verb, "case") == 0) {
            if (flush_case(cur_name, out_dir, enc, &a) != 0)
                failures++;
            if (enc)
                tp_encoder_destroy(&enc);

            char *nm = strtok(NULL, " ");
            if (!nm) {
                fprintf(stderr, "ERROR: line %lu: case without a name\n", lineno);
                failures++;
                cur_name[0] = '\0';
                continue;
            }
            snprintf(cur_name, sizeof(cur_name), "%s", nm);
            cases++;
            /* Any trailing "encode=no" is for the binding harnesses; the
               fixture is generated either way. */
            if (tp_encoder_create(&enc) != TP_OK) {
                fprintf(stderr, "ERROR: encoder_create failed\n");
                fclose(f);
                return 1;
            }
            continue;
        }

        if (strcmp(verb, "key") != 0) {
            fprintf(stderr, "ERROR: line %lu: unknown directive '%s'\n", lineno, verb);
            failures++;
            continue;
        }
        if (!enc) {
            fprintf(stderr, "ERROR: line %lu: key outside a case\n", lineno);
            failures++;
            continue;
        }

        char *key_tok = strtok(NULL, " ");
        char *type_tok = strtok(NULL, " ");
        char *arg_tok = strtok(NULL, " ");
        if (!key_tok || !type_tok) {
            fprintf(stderr, "ERROR: line %lu: malformed key line\n", lineno);
            failures++;
            continue;
        }

        uint8_t key[MAX_TOKEN];
        long key_len = token_decode(key_tok, key, sizeof(key));
        if (key_len < 0) {
            fprintf(stderr, "ERROR: line %lu: bad key token\n", lineno);
            failures++;
            continue;
        }

        tp_value val;
        if (strcmp(type_tok, "null") == 0) {
            val = tp_value_null();
        } else if (strcmp(type_tok, "bool") == 0) {
            val = tp_value_bool(arg_tok && arg_tok[0] == '1');
        } else if (strcmp(type_tok, "int") == 0) {
            val = tp_value_int(strtoll(arg_tok ? arg_tok : "0", NULL, 10));
        } else if (strcmp(type_tok, "uint") == 0) {
            val = tp_value_uint(strtoull(arg_tok ? arg_tok : "0", NULL, 10));
        } else if (strcmp(type_tok, "f32") == 0) {
            uint32_t bits = (uint32_t)hex_u64(arg_tok ? arg_tok : "0");
            float fv;
            memcpy(&fv, &bits, sizeof(fv));
            val = tp_value_float32(fv);
        } else if (strcmp(type_tok, "f64") == 0) {
            uint64_t bits = hex_u64(arg_tok ? arg_tok : "0");
            double dv;
            memcpy(&dv, &bits, sizeof(dv));
            val = tp_value_float64(dv);
        } else if (strcmp(type_tok, "str") == 0) {
            uint8_t tmp[MAX_TOKEN];
            long n = token_decode(arg_tok ? arg_tok : "~", tmp, sizeof(tmp));
            if (n < 0) {
                fprintf(stderr, "ERROR: line %lu: bad str token\n", lineno);
                failures++;
                continue;
            }
            uint8_t *owned = arena_take(&a, tmp, (size_t)n);
            if (!owned) {
                fprintf(stderr, "ERROR: line %lu: out of arena space\n", lineno);
                failures++;
                continue;
            }
            val = tp_value_string_n((const char *)owned, (size_t)n);
        } else if (strcmp(type_tok, "blob") == 0) {
            uint8_t tmp[MAX_TOKEN];
            long n = hex_decode(arg_tok ? arg_tok : "~", tmp, sizeof(tmp));
            if (n < 0) {
                fprintf(stderr, "ERROR: line %lu: bad blob token\n", lineno);
                failures++;
                continue;
            }
            uint8_t *owned = arena_take(&a, tmp, (size_t)n);
            if (!owned) {
                fprintf(stderr, "ERROR: line %lu: out of arena space\n", lineno);
                failures++;
                continue;
            }
            val = tp_value_blob(owned, (size_t)n);
        } else {
            fprintf(stderr, "ERROR: line %lu: unknown type '%s'\n", lineno, type_tok);
            failures++;
            continue;
        }

        tp_result rc = tp_encoder_add_n(enc, (const char *)key, (size_t)key_len, &val);
        if (rc != TP_OK) {
            fprintf(stderr, "ERROR: line %lu: add failed: %s\n", lineno, tp_result_str(rc));
            failures++;
        }
    }

    if (flush_case(cur_name, out_dir, enc, &a) != 0)
        failures++;
    if (enc)
        tp_encoder_destroy(&enc);
    arena_free(&a);
    fclose(f);

    printf("%d cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
