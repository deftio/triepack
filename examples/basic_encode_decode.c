/*
 * basic_encode_decode.c
 *
 * Demonstrates encoding a few key-value pairs into a triepack blob
 * and decoding them back.  This is a skeleton -- the core library is
 * not yet implemented so the calls are commented out, but the intended
 * API usage pattern is shown in full.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /*
     * Step 1 -- Create an encoder context.
     *
     * tp_encoder *enc = NULL;
     * tp_encoder_create(&enc);
     */

    /*
     * Step 2 -- Insert key-value pairs.
     *
     * tp_value v;
     * v = tp_value_int(42);
     * tp_encoder_add(enc, "apple",  &v);
     * v = tp_value_int(17);
     * tp_encoder_add(enc, "banana", &v);
     * v = tp_value_int(8);
     * tp_encoder_add(enc, "cherry", &v);
     * v = tp_value_string("hello");
     * tp_encoder_add(enc, "date",   &v);
     */

    /*
     * Step 3 -- Finalize and obtain the compressed blob.
     *
     * uint8_t *buf = NULL;
     * size_t len = 0;
     * tp_encoder_build(enc, &buf, &len);
     */

    /*
     * Step 4 -- Open the blob for lookup.
     *
     * tp_dict *dict = NULL;
     * tp_dict_open(&dict, buf, len);
     */

    /*
     * Step 5 -- Look up a key.
     *
     * tp_value val;
     * tp_result rc = tp_dict_lookup(dict, "banana", &val);
     * if (rc == TP_OK) {
     *     printf("banana -> %lld\n", (long long)val.data.int_val);
     * }
     */

    /*
     * Step 6 -- Clean up.
     *
     * tp_dict_close(&dict);
     * tp_encoder_destroy(&enc);
     * free(buf);
     */

    printf("triepack basic example (stub -- core not yet implemented)\n");
    return 0;
}
