/**
 * @file generate_fixtures.c
 * @brief Generate .trp fixture files for cross-language interop testing.
 *
 * Writes known dictionaries to tests/fixtures/ so that both the C
 * library and language bindings (JavaScript, etc.) can verify byte-for-byte
 * binary compatibility.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open %s for writing\n", path);
        return 1;
    }
    if (fwrite(buf, 1, len, f) != len) {
        fprintf(stderr, "ERROR: short write to %s\n", path);
        fclose(f);
        return 1;
    }
    fclose(f);
    printf("  wrote %s (%zu bytes)\n", path, len);
    return 0;
}

static int build_and_write(tp_encoder *enc, const char *path)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    tp_result rc = tp_encoder_build(enc, &buf, &len);
    if (rc != TP_OK) {
        fprintf(stderr, "ERROR: tp_encoder_build failed: %s\n", tp_result_str(rc));
        return 1;
    }
    int ret = write_file(path, buf, len);
    free(buf);
    return ret;
}

/* ── Fixture generators ─────────────────────────────────────────────── */

static int gen_empty(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/empty.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    int ret = build_and_write(enc, path);
    tp_encoder_destroy(&enc);
    return ret;
}

static int gen_single_null(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/single_null.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    rc = tp_encoder_add(enc, "hello", NULL);
    if (rc != TP_OK) {
        tp_encoder_destroy(&enc);
        return 1;
    }

    int ret = build_and_write(enc, path);
    tp_encoder_destroy(&enc);
    return ret;
}

static int gen_single_int(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/single_int.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    tp_value val = tp_value_uint(42);
    rc = tp_encoder_add(enc, "key", &val);
    if (rc != TP_OK) {
        tp_encoder_destroy(&enc);
        return 1;
    }

    int ret = build_and_write(enc, path);
    tp_encoder_destroy(&enc);
    return ret;
}

static int gen_multi_mixed(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/multi_mixed.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    tp_value v;

    v = tp_value_bool(true);
    rc = tp_encoder_add(enc, "bool", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_float64(3.14159);
    rc = tp_encoder_add(enc, "f64", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_int(-100);
    rc = tp_encoder_add(enc, "int", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_string("hello");
    rc = tp_encoder_add(enc, "str", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_uint(200);
    rc = tp_encoder_add(enc, "uint", &v);
    if (rc != TP_OK)
        goto fail;

    {
        int ret = build_and_write(enc, path);
        tp_encoder_destroy(&enc);
        return ret;
    }

fail:
    tp_encoder_destroy(&enc);
    return 1;
}

static int gen_shared_prefix(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/shared_prefix.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    tp_value v;

    v = tp_value_uint(10);
    rc = tp_encoder_add(enc, "abc", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_uint(20);
    rc = tp_encoder_add(enc, "abd", &v);
    if (rc != TP_OK)
        goto fail;

    v = tp_value_uint(30);
    rc = tp_encoder_add(enc, "xyz", &v);
    if (rc != TP_OK)
        goto fail;

    {
        int ret = build_and_write(enc, path);
        tp_encoder_destroy(&enc);
        return ret;
    }

fail:
    tp_encoder_destroy(&enc);
    return 1;
}

static int gen_large(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/large.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%04d", i);
        tp_value v = tp_value_uint((uint64_t)i);
        rc = tp_encoder_add(enc, key, &v);
        if (rc != TP_OK) {
            tp_encoder_destroy(&enc);
            return 1;
        }
    }

    int ret = build_and_write(enc, path);
    tp_encoder_destroy(&enc);
    return ret;
}

static int gen_keys_only(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/keys_only.trp", dir);

    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return 1;

    rc = tp_encoder_add(enc, "apple", NULL);
    if (rc != TP_OK)
        goto fail;
    rc = tp_encoder_add(enc, "banana", NULL);
    if (rc != TP_OK)
        goto fail;
    rc = tp_encoder_add(enc, "cherry", NULL);
    if (rc != TP_OK)
        goto fail;

    {
        int ret = build_and_write(enc, path);
        tp_encoder_destroy(&enc);
        return ret;
    }

fail:
    tp_encoder_destroy(&enc);
    return 1;
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *dir = "tests/fixtures";
    if (argc > 1)
        dir = argv[1];

    printf("Generating fixtures in %s/\n", dir);

    int failed = 0;
    failed += gen_empty(dir);
    failed += gen_single_null(dir);
    failed += gen_single_int(dir);
    failed += gen_multi_mixed(dir);
    failed += gen_shared_prefix(dir);
    failed += gen_large(dir);
    failed += gen_keys_only(dir);

    if (failed > 0) {
        fprintf(stderr, "\nERROR: %d fixture(s) failed to generate\n", failed);
        return 1;
    }

    printf("\nAll %d fixtures generated successfully.\n", 7);
    return 0;
}
