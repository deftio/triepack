/**
 * @file encoder.c
 * @brief Encoder lifecycle: accumulate key/value pairs, build compressed trie.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Defaults ────────────────────────────────────────────────────────── */

tp_encoder_options tp_encoder_defaults(void)
{
    tp_encoder_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.trie_mode = TP_ADDR_BIT;
    opts.value_mode = TP_ADDR_BYTE;
    opts.checksum = TP_CHECKSUM_CRC32;
    opts.enable_suffix = true;
    opts.compact_mode = false;
    opts.bits_per_symbol = 0; /* auto */
    return opts;
}

/* ── Create / Destroy ────────────────────────────────────────────────── */

tp_result tp_encoder_create(tp_encoder **out)
{
    if (!out)
        return TP_ERR_INVALID_PARAM;

    tp_encoder_options opts = tp_encoder_defaults();
    return tp_encoder_create_ex(out, &opts);
}

tp_result tp_encoder_create_ex(tp_encoder **out, const tp_encoder_options *opts)
{
    if (!out || !opts)
        return TP_ERR_INVALID_PARAM;

    tp_encoder *enc = calloc(1, sizeof(*enc));
    if (!enc)
        return TP_ERR_ALLOC;

    enc->opts = *opts;
    enc->count = 0;
    *out = enc;
    return TP_OK;
}

tp_result tp_encoder_destroy(tp_encoder **enc)
{
    if (!enc)
        return TP_ERR_INVALID_PARAM;
    free(*enc);
    *enc = NULL;
    return TP_OK;
}

/* ── Add entries ─────────────────────────────────────────────────────── */

tp_result tp_encoder_add(tp_encoder *enc, const char *key, const tp_value *val)
{
    if (!enc || !key)
        return TP_ERR_INVALID_PARAM;
    return tp_encoder_add_n(enc, key, strlen(key), val);
}

tp_result tp_encoder_add_n(tp_encoder *enc, const char *key, size_t key_len, const tp_value *val)
{
    if (!enc || !key)
        return TP_ERR_INVALID_PARAM;
    (void)key_len;
    (void)val;
    /* TODO: store key/value for later trie construction */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Query ───────────────────────────────────────────────────────────── */

uint32_t tp_encoder_count(const tp_encoder *enc)
{
    if (!enc)
        return 0;
    return enc->count;
}

/* ── Build ───────────────────────────────────────────────────────────── */

tp_result tp_encoder_build(tp_encoder *enc, uint8_t **buf, size_t *len)
{
    if (!enc || !buf || !len)
        return TP_ERR_INVALID_PARAM;
    /* TODO: build compressed trie from accumulated entries */
    return TP_ERR_INVALID_PARAM; /* stub — not yet implemented */
}

/* ── Reset ───────────────────────────────────────────────────────────── */

tp_result tp_encoder_reset(tp_encoder *enc)
{
    if (!enc)
        return TP_ERR_INVALID_PARAM;
    enc->count = 0;
    /* TODO: free accumulated key/value storage */
    return TP_OK;
}
