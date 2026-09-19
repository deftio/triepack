/**
 * @file tsml_cli.c
 * @brief A terseml round-tripper and benchmark.
 *
 *   tsml roundtrip < in.tsml > out.tsml   decode, re-encode canonically
 *   tsml check     < in.tsml             exit non-zero if it does not parse
 *   tsml bench N   < in.tsml             time N decode+encode passes
 *
 * `roundtrip` is the differential-test harness: feed it bytes another
 * implementation produced and compare what comes back.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

/* fileno, fstat and clock_gettime are POSIX, not C99. glibc hides them under
   -std=c99 unless asked; Apple's libc exposes them anyway, which is why this
   built on macOS and not on Linux. terseml.c itself needs none of this. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "terseml.h"

static uint8_t *slurp(FILE *f, size_t *len)
{
    struct stat st;
    size_t cap = 1 << 16, n = 0;
    uint8_t *buf;
    /* A regular stdin can be sized exactly; doubling to reach a gigabyte
       costs a gigabyte of copying and, transiently, three of them resident. */
    if (fstat(fileno(f), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0)
        cap = (size_t)st.st_size + 1;
    buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        size_t got;
        if (n == cap) {
            uint8_t *bigger = realloc(buf, cap * 2);
            if (!bigger) { free(buf); return NULL; }
            buf = bigger;
            cap *= 2;
        }
        got = fread(buf + n, 1, cap - n, f);
        if (got == 0) break;
        n += got;
    }
    *len = n;
    return buf;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int fail(const char *what, tsml_result rc)
{
    fprintf(stderr, "%s: %s\n", what, tsml_strerror(rc));
    return 1;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "roundtrip";
    size_t len = 0;
    uint8_t *buf = slurp(stdin, &len);
    tsml_doc *doc = NULL;
    tsml_result rc;
    int status = 0;

    if (!buf) { fprintf(stderr, "out of memory reading input\n"); return 1; }

    if (strcmp(cmd, "bench") == 0) {
        long iters = argc > 2 ? strtol(argv[2], NULL, 10) : 10;
        double t0, dec_s = 0, enc_s = 0;
        long i;
        size_t out_len = 0;
        for (i = 0; i < iters; i++) {
            uint8_t *out = NULL;
            t0 = now();
            rc = tsml_decode_document(buf, len, &doc);
            dec_s += now() - t0;
            if (rc != TSML_OK) { status = fail("decode", rc); break; }
            /* Every top-level element, not just the first: a document of 69
               of them would otherwise report 69x the real throughput. */
            t0 = now();
            {
                size_t k, n = tsml_doc_count(doc);
                for (k = 0; k < n; k++) {
                    rc = tsml_encode(tsml_doc_node(doc, k), &out, &out_len);
                    if (rc != TSML_OK) break;
                    free(out);
                    out = NULL;
                }
            }
            enc_s += now() - t0;
            if (rc != TSML_OK) { status = fail("encode", rc); break; }
            free(out);
            tsml_doc_free(&doc);
        }
        if (!status) {
            double mb = (double)len / (1024.0 * 1024.0) * (double)iters;
            fprintf(stderr, "input %.2f MB x %ld\n", (double)len / (1024.0 * 1024.0), iters);
            fprintf(stderr, "decode %.3f s  %.1f MB/s\n", dec_s, mb / dec_s);
            fprintf(stderr, "encode %.3f s  %.1f MB/s\n", enc_s, mb / enc_s);
        }
        free(buf);
        return status;
    }

    rc = tsml_decode_document(buf, len, &doc);
    if (rc != TSML_OK) { free(buf); return fail("decode", rc); }

    if (strcmp(cmd, "check") != 0) {
        size_t i, n = tsml_doc_count(doc);
        for (i = 0; i < n; i++) {
            uint8_t *out = NULL;
            size_t out_len = 0;
            rc = tsml_encode(tsml_doc_node(doc, i), &out, &out_len);
            if (rc != TSML_OK) { status = fail("encode", rc); break; }
            fwrite(out, 1, out_len, stdout);
            free(out);
        }
    }

    tsml_doc_free(&doc);
    free(buf);
    return status;
}
