/**
 * @file json_encode.c
 * @brief Encode a JSON string into a compressed .trp buffer.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "json_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── One-shot encode ─────────────────────────────────────────────────── */

tp_result tp_json_encode(const char *json_str, size_t json_len,
                         uint8_t **buf, size_t *buf_len)
{
    if (!json_str || !buf || !buf_len) return TP_ERR_INVALID_PARAM;
    (void)json_len;

    /* TODO: parse JSON, build key/value tries, emit .trp buffer */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}
