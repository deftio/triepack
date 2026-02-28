/**
 * @file decoder.c
 * @brief Dictionary lifecycle: open compiled .trp buffer, lookup keys.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Open / Close ────────────────────────────────────────────────────── */

tp_result tp_dict_open(tp_dict **out, const uint8_t *buf, size_t len)
{
    if (!out || !buf)
        return TP_ERR_INVALID_PARAM;
    if (len < TP_HEADER_SIZE)
        return TP_ERR_TRUNCATED;

    /* TODO: validate magic, version, checksum */

    tp_dict *dict = calloc(1, sizeof(*dict));
    if (!dict)
        return TP_ERR_ALLOC;

    dict->buf = buf;
    dict->len = len;
    *out = dict;

    /* stub — header parsing not yet implemented */
    return TP_ERR_INVALID_PARAM;
}

tp_result tp_dict_open_unchecked(tp_dict **out, const uint8_t *buf, size_t len)
{
    if (!out || !buf)
        return TP_ERR_INVALID_PARAM;
    if (len < TP_HEADER_SIZE)
        return TP_ERR_TRUNCATED;

    /* TODO: parse header without integrity checks */

    tp_dict *dict = calloc(1, sizeof(*dict));
    if (!dict)
        return TP_ERR_ALLOC;

    dict->buf = buf;
    dict->len = len;
    *out = dict;

    /* stub — header parsing not yet implemented */
    return TP_ERR_INVALID_PARAM;
}

tp_result tp_dict_close(tp_dict **dict)
{
    if (!dict)
        return TP_ERR_INVALID_PARAM;
    free(*dict);
    *dict = NULL;
    return TP_OK;
}

/* ── Query ───────────────────────────────────────────────────────────── */

uint32_t tp_dict_count(const tp_dict *dict)
{
    if (!dict)
        return 0;
    return dict->info.num_keys;
}

/* ── Lookup ──────────────────────────────────────────────────────────── */

tp_result tp_dict_lookup(const tp_dict *dict, const char *key, tp_value *val)
{
    if (!dict || !key)
        return TP_ERR_INVALID_PARAM;
    return tp_dict_lookup_n(dict, key, strlen(key), val);
}

tp_result tp_dict_lookup_n(const tp_dict *dict, const char *key, size_t key_len, tp_value *val)
{
    if (!dict || !key)
        return TP_ERR_INVALID_PARAM;
    (void)key_len;
    (void)val;
    /* TODO: trie traversal for key lookup */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

tp_result tp_dict_contains(const tp_dict *dict, const char *key, bool *out)
{
    if (!dict || !key || !out)
        return TP_ERR_INVALID_PARAM;
    /* TODO: trie traversal, set *out = true/false */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Info ────────────────────────────────────────────────────────────── */

tp_result tp_dict_get_info(const tp_dict *dict, tp_dict_info *info)
{
    if (!dict || !info)
        return TP_ERR_INVALID_PARAM;
    *info = dict->info;
    return TP_ERR_INVALID_PARAM; /* stub — info not populated yet */
}

/* ── Iteration ──────────────────────────────────────────────────────── */

tp_result tp_dict_iterate(const tp_dict *dict, tp_iterator **out)
{
    if (!dict || !out)
        return TP_ERR_INVALID_PARAM;

    tp_iterator *it = calloc(1, sizeof(*it));
    if (!it)
        return TP_ERR_ALLOC;

    it->dict = dict;
    it->pos = 0;
    it->done = false;
    *out = it;
    return TP_OK;
}

tp_result tp_iter_next(tp_iterator *it, const char **key, size_t *key_len, tp_value *val)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;
    (void)key;
    (void)key_len;
    (void)val;
    /* TODO: trie traversal to next entry */
    return TP_ERR_EOF; /* stub — no entries to iterate */
}

tp_result tp_iter_reset(tp_iterator *it)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;
    it->pos = 0;
    it->done = false;
    return TP_OK;
}

tp_result tp_iter_destroy(tp_iterator **it)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;
    free(*it);
    *it = NULL;
    return TP_OK;
}

/* ── Search ─────────────────────────────────────────────────────────── */

tp_result tp_dict_find_prefix(const tp_dict *dict, const char *prefix, tp_iterator **out)
{
    if (!dict || !prefix || !out)
        return TP_ERR_INVALID_PARAM;
    /* TODO: position iterator at prefix node */
    return tp_dict_iterate(dict, out);
}

tp_result tp_dict_find_fuzzy(const tp_dict *dict, const char *query, uint8_t max_dist,
                             tp_iterator **out)
{
    if (!dict || !query || !out)
        return TP_ERR_INVALID_PARAM;
    (void)max_dist;
    /* TODO: fuzzy search */
    return tp_dict_iterate(dict, out);
}

tp_result tp_iter_get_distance(const tp_iterator *it, uint8_t *dist)
{
    if (!it || !dist)
        return TP_ERR_INVALID_PARAM;
    *dist = it->distance;
    return TP_OK;
}
