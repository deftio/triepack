/*
 * basic_encode_decode.c
 *
 * Demonstrates encoding a few key-value pairs into a triepack blob
 * and decoding them back.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* Step 1 -- Create an encoder */
    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_create: %s\n", tp_result_str(rc));
        return 1;
    }

    /* Step 2 -- Insert key-value pairs */
    tp_value v;

    v = tp_value_int(42);
    tp_encoder_add(enc, "apple", &v);

    v = tp_value_int(17);
    tp_encoder_add(enc, "banana", &v);

    v = tp_value_int(8);
    tp_encoder_add(enc, "cherry", &v);

    v = tp_value_string("hello");
    tp_encoder_add(enc, "date", &v);

    /* Keys-only entries (NULL value = set membership) */
    tp_encoder_add(enc, "elderberry", NULL);
    tp_encoder_add(enc, "fig", NULL);

    printf("Entries added: %u\n", tp_encoder_count(enc));

    /* Step 3 -- Build the compressed blob */
    uint8_t *buf = NULL;
    size_t len = 0;
    rc = tp_encoder_build(enc, &buf, &len);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_build: %s\n", tp_result_str(rc));
        tp_encoder_destroy(&enc);
        return 1;
    }
    printf("Encoded blob: %zu bytes\n\n", len);

    /* Step 4 -- Open the blob for lookup */
    tp_dict *dict = NULL;
    rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "dict_open: %s\n", tp_result_str(rc));
        free(buf);
        tp_encoder_destroy(&enc);
        return 1;
    }

    printf("Dictionary contains %u keys\n\n", tp_dict_count(dict));

    /* Step 5 -- Look up values */
    tp_value val;

    rc = tp_dict_lookup(dict, "apple", &val);
    if (rc == TP_OK && val.type == TP_INT) {
        printf("apple  -> %lld\n", (long long)val.data.int_val);
    }

    rc = tp_dict_lookup(dict, "banana", &val);
    if (rc == TP_OK && val.type == TP_INT) {
        printf("banana -> %lld\n", (long long)val.data.int_val);
    }

    rc = tp_dict_lookup(dict, "cherry", &val);
    if (rc == TP_OK && val.type == TP_INT) {
        printf("cherry -> %lld\n", (long long)val.data.int_val);
    }

    rc = tp_dict_lookup(dict, "date", &val);
    if (rc == TP_OK && val.type == TP_STRING) {
        printf("date   -> \"%.*s\"\n", (int)val.data.string_val.str_len, val.data.string_val.str);
    }

    /* Existence check */
    bool found = false;
    tp_dict_contains(dict, "elderberry", &found);
    printf("elderberry exists: %s\n", found ? "yes" : "no");

    tp_dict_contains(dict, "missing", &found);
    printf("missing exists: %s\n", found ? "yes" : "no");

    /* Step 6 -- Clean up */
    tp_dict_close(&dict);
    tp_encoder_destroy(&enc);
    free(buf);

    printf("\nDone.\n");
    return 0;
}
