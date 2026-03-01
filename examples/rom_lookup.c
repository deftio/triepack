/*
 * rom_lookup.c
 *
 * Demonstrates ROM-style zero-allocation lookup. Encodes a small
 * dictionary in-code, then opens with tp_dict_open_unchecked() and
 * performs lookups directly on the const buffer.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("=== ROM Lookup Example ===\n\n");

    /* Step 1 -- Build a dictionary blob (in a real ROM scenario this
       would be a pre-compiled const array embedded in firmware). */
    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_create: %s\n", tp_result_str(rc));
        return 1;
    }

    tp_value v;

    v = tp_value_int(200);
    tp_encoder_add(enc, "HTTP_OK", &v);

    v = tp_value_int(404);
    tp_encoder_add(enc, "HTTP_NOT_FOUND", &v);

    v = tp_value_int(500);
    tp_encoder_add(enc, "HTTP_SERVER_ERROR", &v);

    v = tp_value_string("text/html");
    tp_encoder_add(enc, "CONTENT_TYPE", &v);

    uint8_t *buf = NULL;
    size_t len = 0;
    rc = tp_encoder_build(enc, &buf, &len);
    tp_encoder_destroy(&enc);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_build: %s\n", tp_result_str(rc));
        return 1;
    }
    printf("Compiled blob: %zu bytes\n\n", len);

    /* Step 2 -- Open without CRC check (ROM-style, trusted data). */
    tp_dict *dict = NULL;
    rc = tp_dict_open_unchecked(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "dict_open_unchecked: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }

    /* Step 3 -- Perform lookups. */
    const char *keys[] = {"HTTP_OK", "HTTP_NOT_FOUND", "HTTP_SERVER_ERROR", "CONTENT_TYPE",
                          "MISSING_KEY"};
    size_t num_keys = sizeof(keys) / sizeof(keys[0]);

    for (size_t i = 0; i < num_keys; i++) {
        tp_value val;
        rc = tp_dict_lookup(dict, keys[i], &val);
        if (rc == TP_OK) {
            if (val.type == TP_INT)
                printf("  %-20s -> %lld\n", keys[i], (long long)val.data.int_val);
            else if (val.type == TP_STRING)
                printf("  %-20s -> \"%.*s\"\n", keys[i], (int)val.data.string_val.str_len,
                       val.data.string_val.str);
            else
                printf("  %-20s -> (type %d)\n", keys[i], val.type);
        } else {
            printf("  %-20s -> NOT FOUND\n", keys[i]);
        }
    }

    /* Step 4 -- Clean up. */
    tp_dict_close(&dict);
    free(buf);

    printf("\nDone.\n");
    return 0;
}
