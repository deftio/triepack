/*
 * prefix_search.c
 *
 * Demonstrates prefix search with an iterator.  Given a prefix
 * string the dictionary returns all matching keys in lexicographic
 * order.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include "triepack/triepack.h"

int main(void)
{
    /*
     * Assume we have an already-encoded blob (see basic_encode_decode
     * or rom_lookup for how to obtain one).
     *
     * tp_dict *dict = NULL;
     * tp_dict_open(&dict, buf, len);
     */

    /*
     * Step 1 -- Create a prefix iterator.
     *
     * tp_iterator *it = NULL;
     * tp_result rc = tp_dict_find_prefix(dict, "app", &it);
     */

    /*
     * Step 2 -- Iterate over all matching entries.
     *
     * const char *key;
     * size_t key_len;
     * tp_value val;
     *
     * while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
     *     printf("  %.*s -> %lld\n",
     *            (int)key_len, key,
     *            (long long)val.data.int_val);
     * }
     */

    /*
     * Step 3 -- Clean up.
     *
     * tp_iter_destroy(&it);
     * tp_dict_close(&dict);
     */

    printf("triepack prefix search example (stub -- core not yet implemented)\n");
    return 0;
}
