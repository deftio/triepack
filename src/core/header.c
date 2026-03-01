/**
 * @file header.c
 * @brief Header parsing and writing for the .trp binary format.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

tp_result tp_header_write(tp_bitstream_writer *w, const tp_header *h)
{
    if (!w || !h)
        return TP_ERR_INVALID_PARAM;

    tp_result rc;
    /* Magic bytes (4) */
    rc = tp_bs_write_u8(w, h->magic[0]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_write_u8(w, h->magic[1]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_write_u8(w, h->magic[2]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_write_u8(w, h->magic[3]);
    if (rc != TP_OK)
        return rc;
    /* Version (2) */
    rc = tp_bs_write_u8(w, h->version_major);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_write_u8(w, h->version_minor);
    if (rc != TP_OK)
        return rc;
    /* Flags (2) */
    rc = tp_bs_write_u16(w, h->flags);
    if (rc != TP_OK)
        return rc;
    /* num_keys (4) */
    rc = tp_bs_write_u32(w, h->num_keys);
    if (rc != TP_OK)
        return rc;
    /* trie_data_offset (4) */
    rc = tp_bs_write_u32(w, h->trie_data_offset);
    if (rc != TP_OK)
        return rc;
    /* value_store_offset (4) */
    rc = tp_bs_write_u32(w, h->value_store_offset);
    if (rc != TP_OK)
        return rc;
    /* suffix_table_offset (4) */
    rc = tp_bs_write_u32(w, h->suffix_table_offset);
    if (rc != TP_OK)
        return rc;
    /* total_data_bits (4) */
    rc = tp_bs_write_u32(w, h->total_data_bits);
    if (rc != TP_OK)
        return rc;
    /* reserved (4) */
    rc = tp_bs_write_u32(w, h->reserved);
    return rc;
}

tp_result tp_header_read(tp_bitstream_reader *r, tp_header *h)
{
    if (!r || !h)
        return TP_ERR_INVALID_PARAM;

    tp_result rc;
    rc = tp_bs_read_u8(r, &h->magic[0]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u8(r, &h->magic[1]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u8(r, &h->magic[2]);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u8(r, &h->magic[3]);
    if (rc != TP_OK)
        return rc;

    /* Validate magic */
    if (h->magic[0] != TP_MAGIC_0 || h->magic[1] != TP_MAGIC_1 || h->magic[2] != TP_MAGIC_2 ||
        h->magic[3] != TP_MAGIC_3)
        return TP_ERR_BAD_MAGIC;

    rc = tp_bs_read_u8(r, &h->version_major);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u8(r, &h->version_minor);
    if (rc != TP_OK)
        return rc;

    /* Validate version */
    if (h->version_major != TP_FORMAT_VERSION_MAJOR)
        return TP_ERR_VERSION;

    rc = tp_bs_read_u16(r, &h->flags);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->num_keys);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->trie_data_offset);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->value_store_offset);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->suffix_table_offset);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->total_data_bits);
    if (rc != TP_OK)
        return rc;
    rc = tp_bs_read_u32(r, &h->reserved);
    return rc;
}
