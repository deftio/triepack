/**
 * @file bitstream_write.c
 * @brief Bit-level and byte-level write operations, buffer append, bulk copy.
 */

#include "bitstream_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ────────────────────────────────────────────────── */

static tp_result ensure_capacity(tp_bitstream_writer *w, uint64_t n_bits)
{
    size_t needed_bytes = (size_t)((w->pos + n_bits + 7) / 8);
    if (needed_bytes <= w->cap)
        return TP_OK;

    size_t new_cap;
    if (w->growth > 0) {
        new_cap = w->cap + w->growth;
    } else {
        new_cap = w->cap * 2;
    }
    while (new_cap < needed_bytes)
        new_cap = w->growth > 0 ? new_cap + w->growth : new_cap * 2;

    uint8_t *new_buf = realloc(w->buf, new_cap);
    if (!new_buf)
        return TP_ERR_ALLOC;

    memset(new_buf + w->cap, 0, new_cap - w->cap);
    w->buf = new_buf;
    w->cap = new_cap;
    return TP_OK;
}

static inline void write_bit_msb(uint8_t *buf, uint64_t bit_pos, uint8_t val)
{
    size_t byte_idx = (size_t)(bit_pos / 8);
    uint8_t bit_idx = (uint8_t)(7 - (bit_pos % 8));
    if (val)
        buf[byte_idx] = (uint8_t)(buf[byte_idx] | (1u << bit_idx));
    else
        buf[byte_idx] = (uint8_t)(buf[byte_idx] & ~(1u << bit_idx));
}

/* ── Bit-level write ─────────────────────────────────────────────────── */

tp_result tp_bs_write_bits(tp_bitstream_writer *w, uint64_t value, uint8_t n)
{
    if (!w || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;

    tp_result rc = ensure_capacity(w, n);
    if (rc != TP_OK)
        return rc;

    for (uint8_t i = 0; i < n; i++) {
        uint8_t bit = (uint8_t)((value >> (n - 1 - i)) & 1);
        write_bit_msb(w->buf, w->pos, bit);
        w->pos++;
    }
    return TP_OK;
}

tp_result tp_bs_write_bit(tp_bitstream_writer *w, uint8_t value)
{
    if (!w)
        return TP_ERR_INVALID_PARAM;

    tp_result rc = ensure_capacity(w, 1);
    if (rc != TP_OK)
        return rc;

    write_bit_msb(w->buf, w->pos, value & 1);
    w->pos++;
    return TP_OK;
}

tp_result tp_bs_writer_align_to_byte(tp_bitstream_writer *w)
{
    if (!w)
        return TP_ERR_INVALID_PARAM;
    uint64_t rem = w->pos % 8;
    if (rem != 0) {
        uint8_t pad = (uint8_t)(8 - rem);
        tp_result rc = ensure_capacity(w, pad);
        if (rc != TP_OK)
            return rc;
        /* Zero bits are already there from calloc/memset */
        w->pos += pad;
    }
    return TP_OK;
}

/* ── Byte-level write ────────────────────────────────────────────────── */

tp_result tp_bs_write_u8(tp_bitstream_writer *w, uint8_t val)
{
    return tp_bs_write_bits(w, val, 8);
}

tp_result tp_bs_write_u16(tp_bitstream_writer *w, uint16_t val)
{
    return tp_bs_write_bits(w, val, 16);
}

tp_result tp_bs_write_u32(tp_bitstream_writer *w, uint32_t val)
{
    return tp_bs_write_bits(w, val, 32);
}

tp_result tp_bs_write_u64(tp_bitstream_writer *w, uint64_t val)
{
    return tp_bs_write_bits(w, val, 64);
}

tp_result tp_bs_write_bytes(tp_bitstream_writer *w, const uint8_t *buf, size_t n)
{
    if (!w || (!buf && n > 0))
        return TP_ERR_INVALID_PARAM;

    for (size_t i = 0; i < n; i++) {
        tp_result rc = tp_bs_write_u8(w, buf[i]);
        if (rc != TP_OK)
            return rc;
    }
    return TP_OK;
}

/* ── Buffer append ───────────────────────────────────────────────────── */

tp_result tp_bs_writer_append_buffer(tp_bitstream_writer *w,
                                     const uint8_t *buf, uint64_t bit_len)
{
    if (!w || (!buf && bit_len > 0))
        return TP_ERR_INVALID_PARAM;

    tp_result rc = ensure_capacity(w, bit_len);
    if (rc != TP_OK)
        return rc;

    for (uint64_t i = 0; i < bit_len; i++) {
        size_t byte_idx  = (size_t)(i / 8);
        uint8_t bit_idx  = (uint8_t)(7 - (i % 8));
        uint8_t bit = (uint8_t)((buf[byte_idx] >> bit_idx) & 1);
        write_bit_msb(w->buf, w->pos, bit);
        w->pos++;
    }
    return TP_OK;
}

/* ── Bulk copy ───────────────────────────────────────────────────────── */

tp_result tp_bs_copy_bits(tp_bitstream_reader *r, tp_bitstream_writer *w,
                          uint64_t n_bits)
{
    if (!r || !w)
        return TP_ERR_INVALID_PARAM;
    if (r->pos + n_bits > r->bit_len)
        return TP_ERR_EOF;

    tp_result rc = ensure_capacity(w, n_bits);
    if (rc != TP_OK)
        return rc;

    for (uint64_t i = 0; i < n_bits; i++) {
        size_t byte_idx  = (size_t)(r->pos / 8);
        uint8_t bit_idx  = (uint8_t)(7 - (r->pos % 8));
        uint8_t bit = (uint8_t)((r->buf[byte_idx] >> bit_idx) & 1);
        write_bit_msb(w->buf, w->pos, bit);
        r->pos++;
        w->pos++;
    }
    return TP_OK;
}
