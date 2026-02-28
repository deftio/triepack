/**
 * @file json_dom.c
 * @brief DOM-style access: open, close, path lookup, iteration.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "json_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Open / Close ────────────────────────────────────────────────────── */

tp_result tp_json_open(tp_json **out, const uint8_t *buf, size_t buf_len)
{
    if (!out || !buf)
        return TP_ERR_INVALID_PARAM;

    tp_json *j = calloc(1, sizeof(*j));
    if (!j)
        return TP_ERR_ALLOC;

    j->buf = buf;
    j->buf_len = buf_len;
    *out = j;
    return TP_OK;
}

tp_result tp_json_close(tp_json **json)
{
    if (!json)
        return TP_ERR_INVALID_PARAM;
    free(*json);
    *json = NULL;
    return TP_OK;
}

/* ── Path lookup ─────────────────────────────────────────────────────── */

tp_result tp_json_lookup_path(const tp_json *j, const char *path, tp_value *val)
{
    if (!j || !path || !val)
        return TP_ERR_INVALID_PARAM;

    /* TODO: parse dot/bracket path, walk trie to resolve value */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Root type ───────────────────────────────────────────────────────── */

tp_result tp_json_root_type(const tp_json *j, tp_value_type *type)
{
    if (!j || !type)
        return TP_ERR_INVALID_PARAM;

    /* TODO: read root node tag from buffer */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Iteration ───────────────────────────────────────────────────────── */

tp_result tp_json_iterate(const tp_json *j, tp_iterator **out)
{
    if (!j || !out)
        return TP_ERR_INVALID_PARAM;

    /* TODO: create iterator over top-level keys/elements */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Count ───────────────────────────────────────────────────────────── */

uint32_t tp_json_count(const tp_json *j)
{
    if (!j)
        return 0;

    /* TODO: return number of top-level keys or elements */
    return 0;
}
