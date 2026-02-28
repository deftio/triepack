/*
 * rom_lookup.c
 *
 * Demonstrates opening a pre-compiled .trp blob (e.g. from a file
 * or embedded in ROM) and performing key lookups without any
 * encoding step.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    const char *path;

    (void)argc;
    (void)argv;

    path = (argc > 1) ? argv[1] : "data/sample.trp";

    /*
     * Step 1 -- Load the blob into memory (or map it read-only).
     *
     * FILE *fp = fopen(path, "rb");
     * fseek(fp, 0, SEEK_END);
     * long file_len = ftell(fp);
     * rewind(fp);
     * uint8_t *buf = malloc((size_t)file_len);
     * fread(buf, 1, (size_t)file_len, fp);
     * fclose(fp);
     */

    /*
     * Step 2 -- Open the blob for lookup.
     *
     * tp_dict *dict = NULL;
     * tp_result rc = tp_dict_open(&dict, buf, (size_t)file_len);
     * if (rc != TP_OK) {
     *     fprintf(stderr, "open failed: %s\n", tp_result_str(rc));
     *     free(buf);
     *     return 1;
     * }
     */

    /*
     * Step 3 -- Perform lookups.
     *
     * tp_value val;
     * rc = tp_dict_lookup(dict, "hello", &val);
     * if (rc == TP_OK) {
     *     printf("hello -> %lld\n", (long long)val.data.int_val);
     * } else {
     *     printf("hello: not found\n");
     * }
     */

    /*
     * Step 4 -- Clean up.
     *
     * tp_dict_close(&dict);
     * free(buf);
     */

    printf("triepack ROM lookup example (stub -- core not yet implemented)\n");
    printf("would load blob from: %s\n", path);
    return 0;
}
