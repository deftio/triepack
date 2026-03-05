/**
 * @file bitstream_varint.c
 * @brief VarInt (LEB128 / zigzag) read/write operations.
 */

#include "bitstream_internal.h"

/* ── VarInt read ─────────────────────────────────────────────────────── */

tp_result tp_bs_read_varint_u(tp_bitstream_reader *r, uint64_t *out)
{
    if (!r || !out)
        return TP_ERR_INVALID_PARAM;

    uint64_t val = 0;
    unsigned int shift = 0;

    for (int group = 0; group < TP_VARINT_MAX_GROUPS; group++) {
        if (r->pos + 8 > r->bit_len)
            return TP_ERR_EOF;

        /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
        uint64_t byte_val;
        tp_result rc = tp_bs_read_bits(r, 8, &byte_val);
        if (rc != TP_OK)
            return rc; /* LCOV_EXCL_LINE */

        val |= (byte_val & 0x7F) << shift;
        if ((byte_val & 0x80) == 0) {
            *out = val;
            return TP_OK;
        }
        shift += 7;
    }

    return TP_ERR_OVERFLOW;
}

tp_result tp_bs_read_varint_s(tp_bitstream_reader *r, int64_t *out)
{
    if (!r || !out)
        return TP_ERR_INVALID_PARAM;

    uint64_t raw;
    tp_result rc = tp_bs_read_varint_u(r, &raw);
    if (rc != TP_OK)
        return rc;

    /* Zigzag decode: if low bit set, negate; otherwise keep */
    if (raw & 1) {
        *out = -((int64_t)(raw >> 1)) - 1;
    } else {
        *out = (int64_t)(raw >> 1);
    }
    return TP_OK;
}

/* ── VarInt write ────────────────────────────────────────────────────── */

tp_result tp_bs_write_varint_u(tp_bitstream_writer *w, uint64_t value)
{
    if (!w)
        return TP_ERR_INVALID_PARAM;

    do {
        uint8_t byte_val = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte_val = (uint8_t)(byte_val | 0x80u);

        tp_result rc = tp_bs_write_bits(w, byte_val, 8);
        if (rc != TP_OK)
            return rc; /* LCOV_EXCL_LINE */
    } while (value != 0);

    return TP_OK;
}

tp_result tp_bs_write_varint_s(tp_bitstream_writer *w, int64_t value)
{
    if (!w)
        return TP_ERR_INVALID_PARAM;

    /* Zigzag encode: cast to unsigned before left shift to avoid UB */
    uint64_t raw = ((uint64_t)value << 1) ^ (uint64_t)(value >> 63);
    return tp_bs_write_varint_u(w, raw);
}
