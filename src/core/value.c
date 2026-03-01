/**
 * @file value.c
 * @brief Value encoding and decoding helpers for typed trie terminals.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

tp_result tp_value_encode(tp_bitstream_writer *w, const tp_value *val)
{
    if (!w || !val)
        return TP_ERR_INVALID_PARAM;

    /* Write 4-bit type tag */
    tp_result rc = tp_bs_write_bits(w, (uint64_t)val->type, 4);
    if (rc != TP_OK)
        return rc;

    switch (val->type) {
    case TP_NULL:
        break;
    case TP_BOOL:
        rc = tp_bs_write_bit(w, val->data.bool_val ? 1 : 0);
        break;
    case TP_INT:
        rc = tp_bs_write_varint_s(w, val->data.int_val);
        break;
    case TP_UINT:
        rc = tp_bs_write_varint_u(w, val->data.uint_val);
        break;
    case TP_FLOAT32: {
        uint32_t bits;
        memcpy(&bits, &val->data.float32_val, sizeof(bits));
        rc = tp_bs_write_u32(w, bits);
        break;
    }
    case TP_FLOAT64: {
        uint64_t bits;
        memcpy(&bits, &val->data.float64_val, sizeof(bits));
        rc = tp_bs_write_u64(w, bits);
        break;
    }
    case TP_STRING: {
        rc = tp_bs_write_varint_u(w, val->data.string_val.str_len);
        if (rc != TP_OK)
            return rc;
        /* Align to byte boundary before writing raw string data */
        rc = tp_bs_writer_align_to_byte(w);
        if (rc != TP_OK)
            return rc;
        rc = tp_bs_write_bytes(w, (const uint8_t *)val->data.string_val.str,
                               val->data.string_val.str_len);
        break;
    }
    case TP_BLOB: {
        rc = tp_bs_write_varint_u(w, val->data.blob_val.len);
        if (rc != TP_OK)
            return rc;
        /* Align to byte boundary before writing raw blob data */
        rc = tp_bs_writer_align_to_byte(w);
        if (rc != TP_OK)
            return rc;
        rc = tp_bs_write_bytes(w, val->data.blob_val.data, val->data.blob_val.len);
        break;
    }
    case TP_ARRAY:
    case TP_DICT:
        return TP_ERR_INVALID_PARAM; /* deferred */
    }
    return rc;
}

tp_result tp_value_decode(tp_bitstream_reader *r, tp_value *val, const uint8_t *base_buf)
{
    if (!r || !val)
        return TP_ERR_INVALID_PARAM;

    memset(val, 0, sizeof(*val));

    uint64_t tag;
    tp_result rc = tp_bs_read_bits(r, 4, &tag);
    if (rc != TP_OK)
        return rc;
    val->type = (tp_value_type)tag;

    switch (val->type) {
    case TP_NULL:
        break;
    case TP_BOOL: {
        uint8_t bit;
        rc = tp_bs_read_bit(r, &bit);
        if (rc == TP_OK)
            val->data.bool_val = (bit != 0);
        break;
    }
    case TP_INT:
        rc = tp_bs_read_varint_s(r, &val->data.int_val);
        break;
    case TP_UINT:
        rc = tp_bs_read_varint_u(r, &val->data.uint_val);
        break;
    case TP_FLOAT32: {
        uint32_t bits;
        rc = tp_bs_read_u32(r, &bits);
        if (rc == TP_OK)
            memcpy(&val->data.float32_val, &bits, sizeof(float));
        break;
    }
    case TP_FLOAT64: {
        uint64_t bits;
        rc = tp_bs_read_u64(r, &bits);
        if (rc == TP_OK)
            memcpy(&val->data.float64_val, &bits, sizeof(double));
        break;
    }
    case TP_STRING: {
        uint64_t slen;
        rc = tp_bs_read_varint_u(r, &slen);
        if (rc != TP_OK)
            return rc;
        /* Align to byte boundary before reading raw string data */
        rc = tp_bs_reader_align_to_byte(r);
        if (rc != TP_OK)
            return rc;
        if (base_buf) {
            const uint8_t *ptr;
            rc = tp_bs_reader_direct_ptr(r, &ptr, (size_t)slen);
            if (rc == TP_OK) {
                val->data.string_val.str = (const char *)ptr;
                val->data.string_val.str_len = (size_t)slen;
            }
        } else {
            rc = tp_bs_reader_advance(r, slen * 8);
            val->data.string_val.str = NULL;
            val->data.string_val.str_len = (size_t)slen;
        }
        break;
    }
    case TP_BLOB: {
        uint64_t blen;
        rc = tp_bs_read_varint_u(r, &blen);
        if (rc != TP_OK)
            return rc;
        /* Align to byte boundary before reading raw blob data */
        rc = tp_bs_reader_align_to_byte(r);
        if (rc != TP_OK)
            return rc;
        if (base_buf) {
            const uint8_t *ptr;
            rc = tp_bs_reader_direct_ptr(r, &ptr, (size_t)blen);
            if (rc == TP_OK) {
                val->data.blob_val.data = ptr;
                val->data.blob_val.len = (size_t)blen;
            }
        } else {
            rc = tp_bs_reader_advance(r, blen * 8);
            val->data.blob_val.data = NULL;
            val->data.blob_val.len = (size_t)blen;
        }
        break;
    }
    case TP_ARRAY:
    case TP_DICT:
        return TP_ERR_INVALID_PARAM; /* deferred */
    }
    return rc;
}
