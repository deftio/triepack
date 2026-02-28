/*
 * json_roundtrip.c
 *
 * Demonstrates encoding a JSON document into triepack format and
 * decoding it back.  This is a skeleton -- the JSON library is not
 * yet implemented so the calls are commented out.
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
    /*
     * Step 1 -- Provide a JSON string to encode.
     *
     * const char *json_input =
     *     "{ \"name\": \"triepack\", \"version\": 1 }";
     * size_t json_input_len = strlen(json_input);
     */

    /*
     * Step 2 -- Encode the JSON into a triepack blob.
     *
     * uint8_t *buf = NULL;
     * size_t buf_len = 0;
     * tp_result rc = tp_json_encode(json_input, json_input_len,
     *                               &buf, &buf_len);
     */

    /*
     * Step 3 -- Decode the blob back to JSON text.
     *
     * char *json_output = NULL;
     * size_t json_output_len = 0;
     * rc = tp_json_decode(buf, buf_len, &json_output, &json_output_len);
     */

    /*
     * Step 4 -- Use the output.
     *
     * printf("round-tripped JSON (%zu bytes):\n%s\n",
     *        json_output_len, json_output);
     */

    /*
     * Step 5 -- Clean up.
     *
     * free(json_output);
     * free(buf);
     */

    printf("triepack JSON round-trip example (stub -- json lib not yet implemented)\n");
    return 0;
}
