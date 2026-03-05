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

    /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
    tp_json *j = calloc(1, sizeof(*j));
    if (!j)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    /* Make our own copy of the buffer so the caller doesn't need to keep it alive */
    j->buf_owned = malloc(buf_len);
    if (!j->buf_owned) {
        /* LCOV_EXCL_START */
        free(j);
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }
    memcpy(j->buf_owned, buf, buf_len);
    j->buf_len = buf_len;

    /* Open as dict */
    tp_result rc = tp_dict_open(&j->dict, j->buf_owned, buf_len);
    if (rc != TP_OK) {
        free(j->buf_owned);
        free(j);
        return rc;
    }

    /* Determine root type */
    tp_value root_val;
    if (tp_dict_lookup(j->dict, TP_JSON_META_ROOT, &root_val) == TP_OK) {
        uint32_t rt = 0;
        if (root_val.type == TP_UINT)
            rt = (uint32_t)root_val.data.uint_val;
        else if (root_val.type == TP_INT)
            rt = (uint32_t)root_val.data.int_val;
        j->root = (rt == TP_JSON_ROOT_ARRAY) ? TP_ARRAY : TP_DICT;
    } else {
        j->root = TP_DICT; /* default to object */
    }

    /* Count top-level keys (keys without dots, excluding metadata and array indices) */
    /* For objects: keys without '.' separator
       For arrays: count [0], [1], ... indices */
    /* Since we can't iterate yet, we use dict count minus metadata */
    j->count = tp_dict_count(j->dict);
    if (j->count > 0)
        j->count--; /* subtract the metadata key */

    *out = j;
    return TP_OK;
}

tp_result tp_json_close(tp_json **json)
{
    if (!json)
        return TP_ERR_INVALID_PARAM;
    if (*json) {
        if ((*json)->dict)
            tp_dict_close(&(*json)->dict);
        free((*json)->buf_owned);
        free(*json);
        *json = NULL;
    }
    return TP_OK;
}

/* ── Path lookup ─────────────────────────────────────────────────────── */

tp_result tp_json_lookup_path(const tp_json *j, const char *path, tp_value *val)
{
    if (!j || !path || !val)
        return TP_ERR_INVALID_PARAM;

    /* The path is already in dot/bracket notation, which matches our
       flattened key format. Just look it up directly. */
    return tp_dict_lookup(j->dict, path, val);
}

/* ── Root type ───────────────────────────────────────────────────────── */

tp_result tp_json_root_type(const tp_json *j, tp_value_type *type)
{
    if (!j || !type)
        return TP_ERR_INVALID_PARAM;

    *type = j->root;
    return TP_OK;
}

/* ── Iteration ───────────────────────────────────────────────────────── */

tp_result tp_json_iterate(const tp_json *j, tp_iterator **out)
{
    if (!j || !out)
        return TP_ERR_INVALID_PARAM;

    return tp_dict_iterate(j->dict, out);
}

/* ── Count ───────────────────────────────────────────────────────────── */

uint32_t tp_json_count(const tp_json *j)
{
    if (!j)
        return 0;

    return j->count;
}
