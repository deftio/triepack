/**
 * @file json_decode.c
 * @brief Decode a .trp buffer back to JSON text.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "json_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── One-shot decode ─────────────────────────────────────────────────── */

tp_result tp_json_decode(const uint8_t *buf, size_t buf_len,
                         char **json_str, size_t *json_len)
{
    if (!buf || !json_str || !json_len) return TP_ERR_INVALID_PARAM;
    (void)buf_len;

    /* TODO: walk trie, reconstruct JSON text */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Pretty-printed decode ───────────────────────────────────────────── */

tp_result tp_json_decode_pretty(const uint8_t *buf, size_t buf_len,
                                const char *indent,
                                char **json_str, size_t *json_len)
{
    if (!buf || !indent || !json_str || !json_len) return TP_ERR_INVALID_PARAM;
    (void)buf_len;

    /* TODO: walk trie, reconstruct pretty-printed JSON text */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}
