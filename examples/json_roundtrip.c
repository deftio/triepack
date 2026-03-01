/*
 * json_roundtrip.c
 *
 * Demonstrates encoding a JSON document into triepack format and
 * decoding it back. Shows both compact and pretty-printed output.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("=== JSON Round-Trip Example ===\n\n");

    /* Step 1 -- Provide a JSON string to encode. */
    const char *json_input =
        "{\"name\":\"triepack\",\"version\":1,\"features\":[\"fast\",\"compact\",\"typed\"],"
        "\"config\":{\"max_depth\":32,\"crc\":true}}";
    size_t json_input_len = strlen(json_input);

    printf("Input JSON (%zu bytes):\n%s\n\n", json_input_len, json_input);

    /* Step 2 -- Encode the JSON into a triepack blob. */
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(json_input, json_input_len, &buf, &buf_len);
    if (rc != TP_OK) {
        fprintf(stderr, "json_encode: %s\n", tp_result_str(rc));
        return 1;
    }
    printf("Encoded blob: %zu bytes (%.1f%% of input)\n\n", buf_len,
           100.0 * (double)buf_len / (double)json_input_len);

    /* Step 3 -- Decode the blob back to compact JSON. */
    char *json_compact = NULL;
    size_t compact_len = 0;
    rc = tp_json_decode(buf, buf_len, &json_compact, &compact_len);
    if (rc != TP_OK) {
        fprintf(stderr, "json_decode: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }
    printf("Decoded compact (%zu bytes):\n%s\n\n", compact_len, json_compact);

    /* Step 4 -- Decode with pretty printing. */
    char *json_pretty = NULL;
    size_t pretty_len = 0;
    rc = tp_json_decode_pretty(buf, buf_len, "  ", &json_pretty, &pretty_len);
    if (rc != TP_OK) {
        fprintf(stderr, "json_decode_pretty: %s\n", tp_result_str(rc));
        free(json_compact);
        free(buf);
        return 1;
    }
    printf("Decoded pretty:\n%s\n\n", json_pretty);

    /* Step 5 -- DOM-style path lookup. */
    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    if (rc == TP_OK) {
        tp_value val;

        rc = tp_json_lookup_path(j, "name", &val);
        if (rc == TP_OK && val.type == TP_STRING) {
            printf("Lookup 'name' -> \"%.*s\"\n", (int)val.data.string_val.str_len,
                   val.data.string_val.str);
        }

        rc = tp_json_lookup_path(j, "version", &val);
        if (rc == TP_OK && val.type == TP_INT) {
            printf("Lookup 'version' -> %lld\n", (long long)val.data.int_val);
        }

        rc = tp_json_lookup_path(j, "config.max_depth", &val);
        if (rc == TP_OK && val.type == TP_INT) {
            printf("Lookup 'config.max_depth' -> %lld\n", (long long)val.data.int_val);
        }

        rc = tp_json_lookup_path(j, "features[0]", &val);
        if (rc == TP_OK && val.type == TP_STRING) {
            printf("Lookup 'features[0]' -> \"%.*s\"\n", (int)val.data.string_val.str_len,
                   val.data.string_val.str);
        }

        tp_json_close(&j);
    }

    /* Step 6 -- Clean up. */
    free(json_pretty);
    free(json_compact);
    free(buf);

    printf("\nDone.\n");
    return 0;
}
