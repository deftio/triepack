/*
 * prefix_search.c
 *
 * Demonstrates building a word list and checking key existence
 * with tp_dict_contains(). Shows prefix-style membership checking
 * for related keys sharing common prefixes.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("=== Prefix Search Example ===\n\n");

    /* Step 1 -- Build a word list with shared prefixes. */
    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_create: %s\n", tp_result_str(rc));
        return 1;
    }

    /* Words with "app" prefix */
    tp_encoder_add(enc, "app", NULL);
    tp_encoder_add(enc, "apple", NULL);
    tp_encoder_add(enc, "application", NULL);
    tp_encoder_add(enc, "apply", NULL);
    tp_encoder_add(enc, "approve", NULL);

    /* Words with "ban" prefix */
    tp_encoder_add(enc, "ban", NULL);
    tp_encoder_add(enc, "banana", NULL);
    tp_encoder_add(enc, "band", NULL);
    tp_encoder_add(enc, "bank", NULL);

    /* Other words */
    tp_encoder_add(enc, "cherry", NULL);
    tp_encoder_add(enc, "date", NULL);

    printf("Added %u words\n", tp_encoder_count(enc));

    uint8_t *buf = NULL;
    size_t len = 0;
    rc = tp_encoder_build(enc, &buf, &len);
    tp_encoder_destroy(&enc);
    if (rc != TP_OK) {
        fprintf(stderr, "encoder_build: %s\n", tp_result_str(rc));
        return 1;
    }
    printf("Compiled blob: %zu bytes\n\n", len);

    /* Step 2 -- Open and check membership for various keys. */
    tp_dict *dict = NULL;
    rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "dict_open: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }

    /* Check app-related words */
    const char *queries[] = {"app",    "apple",  "application", "apply",  "approve",
                             "append", "banana", "bank",        "cherry", "grape"};
    size_t num_queries = sizeof(queries) / sizeof(queries[0]);

    printf("Membership checks:\n");
    for (size_t i = 0; i < num_queries; i++) {
        bool found = false;
        tp_dict_contains(dict, queries[i], &found);
        printf("  %-15s %s\n", queries[i], found ? "YES" : "no");
    }

    printf("\nDictionary info:\n");
    tp_dict_info info;
    rc = tp_dict_get_info(dict, &info);
    if (rc == TP_OK) {
        printf("  Keys:       %u\n", info.num_keys);
        printf("  Total size: %zu bytes\n", info.total_bytes);
        printf("  BPS:        %u bits\n", info.bits_per_symbol);
    }

    /* Step 3 -- Clean up. */
    tp_dict_close(&dict);
    free(buf);

    printf("\nDone.\n");
    return 0;
}
