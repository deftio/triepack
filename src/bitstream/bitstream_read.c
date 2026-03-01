/**
 * @file bitstream_read.c
 * @brief Bit-level and byte-level read operations.
 */

#include "bitstream_internal.h"

#include <string.h>

/* ── Internal helpers ────────────────────────────────────────────────── */

/**
 * Read a single bit from buf at the given absolute bit position (MSB-first).
 */
static inline uint8_t read_bit_msb(const uint8_t *buf, uint64_t bit_pos)
{
    size_t byte_idx = (size_t)(bit_pos / 8);
    uint8_t bit_idx = (uint8_t)(7 - (bit_pos % 8));
    return (uint8_t)((buf[byte_idx] >> bit_idx) & 1);
}

/* ── Stateless ROM functions ─────────────────────────────────────────── */

tp_result tp_bs_read_bits_at(const uint8_t *buf, uint64_t bit_pos, unsigned int n, uint64_t *out)
{
    if (!buf || !out || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;

    uint64_t val = 0;
    for (unsigned int i = 0; i < n; i++) {
        val = (val << 1) | read_bit_msb(buf, bit_pos + i);
    }
    *out = val;
    return TP_OK;
}

tp_result tp_bs_read_bits_signed_at(const uint8_t *buf, uint64_t bit_pos, unsigned int n,
                                    int64_t *out)
{
    if (!buf || !out || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;

    uint64_t raw;
    tp_result rc = tp_bs_read_bits_at(buf, bit_pos, n, &raw);
    if (rc != TP_OK)
        return rc;

    /* Sign-extend */
    if (n < 64 && (raw & ((uint64_t)1 << (n - 1)))) {
        raw |= ~(((uint64_t)1 << n) - 1);
    }
    *out = (int64_t)raw;
    return TP_OK;
}

tp_result tp_bs_read_varint_u_at(const uint8_t *buf, uint64_t bit_pos, uint64_t *out,
                                 unsigned int *bits_read)
{
    if (!buf || !out || !bits_read)
        return TP_ERR_INVALID_PARAM;

    uint64_t val = 0;
    unsigned int shift = 0;
    unsigned int total_bits = 0;

    for (int group = 0; group < TP_VARINT_MAX_GROUPS; group++) {
        uint64_t byte_val;
        tp_result rc = tp_bs_read_bits_at(buf, bit_pos + total_bits, 8, &byte_val);
        if (rc != TP_OK)
            return rc;
        total_bits += 8;

        val |= (byte_val & 0x7F) << shift;
        if ((byte_val & 0x80) == 0) {
            *out = val;
            *bits_read = total_bits;
            return TP_OK;
        }
        shift += 7;
    }

    return TP_ERR_OVERFLOW;
}

/* ── Bit-level read ──────────────────────────────────────────────────── */

tp_result tp_bs_read_bits(tp_bitstream_reader *r, unsigned int n, uint64_t *out)
{
    if (!r || !out || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;
    if (r->pos + n > r->bit_len)
        return TP_ERR_EOF;

    tp_result rc = tp_bs_read_bits_at(r->buf, r->pos, n, out);
    if (rc == TP_OK)
        r->pos += n;
    return rc;
}

tp_result tp_bs_read_bits_signed(tp_bitstream_reader *r, unsigned int n, int64_t *out)
{
    if (!r || !out || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;
    if (r->pos + n > r->bit_len)
        return TP_ERR_EOF;

    tp_result rc = tp_bs_read_bits_signed_at(r->buf, r->pos, n, out);
    if (rc == TP_OK)
        r->pos += n;
    return rc;
}

tp_result tp_bs_read_bit(tp_bitstream_reader *r, uint8_t *out)
{
    if (!r || !out)
        return TP_ERR_INVALID_PARAM;
    if (r->pos >= r->bit_len)
        return TP_ERR_EOF;

    *out = read_bit_msb(r->buf, r->pos);
    r->pos++;
    return TP_OK;
}

tp_result tp_bs_peek_bits(tp_bitstream_reader *r, unsigned int n, uint64_t *out)
{
    if (!r || !out || n == 0 || n > 64)
        return TP_ERR_INVALID_PARAM;
    if (r->pos + n > r->bit_len)
        return TP_ERR_EOF;

    return tp_bs_read_bits_at(r->buf, r->pos, n, out);
}

tp_result tp_bs_read_bits32(tp_bitstream_reader *r, unsigned int n, uint32_t *out)
{
    if (!r || !out || n == 0 || n > 32)
        return TP_ERR_INVALID_PARAM;

    uint64_t val;
    tp_result rc = tp_bs_read_bits(r, n, &val);
    if (rc == TP_OK)
        *out = (uint32_t)val;
    return rc;
}

/* ── Byte-level read ─────────────────────────────────────────────────── */

tp_result tp_bs_read_u8(tp_bitstream_reader *r, uint8_t *out)
{
    uint64_t val;
    tp_result rc = tp_bs_read_bits(r, 8, &val);
    if (rc == TP_OK)
        *out = (uint8_t)val;
    return rc;
}

tp_result tp_bs_read_u16(tp_bitstream_reader *r, uint16_t *out)
{
    uint64_t val;
    tp_result rc = tp_bs_read_bits(r, 16, &val);
    if (rc == TP_OK)
        *out = (uint16_t)val;
    return rc;
}

tp_result tp_bs_read_u32(tp_bitstream_reader *r, uint32_t *out)
{
    uint64_t val;
    tp_result rc = tp_bs_read_bits(r, 32, &val);
    if (rc == TP_OK)
        *out = (uint32_t)val;
    return rc;
}

tp_result tp_bs_read_u64(tp_bitstream_reader *r, uint64_t *out)
{
    return tp_bs_read_bits(r, 64, out);
}

tp_result tp_bs_read_bytes(tp_bitstream_reader *r, uint8_t *buf, size_t n)
{
    if (!r || !buf)
        return TP_ERR_INVALID_PARAM;

    for (size_t i = 0; i < n; i++) {
        tp_result rc = tp_bs_read_u8(r, &buf[i]);
        if (rc != TP_OK)
            return rc;
    }
    return TP_OK;
}

tp_result tp_bs_reader_direct_ptr(tp_bitstream_reader *r, const uint8_t **ptr, size_t n)
{
    if (!r || !ptr)
        return TP_ERR_INVALID_PARAM;
    if (!tp_bs_reader_is_byte_aligned(r))
        return TP_ERR_NOT_ALIGNED;
    if (r->pos + (uint64_t)n * 8 > r->bit_len)
        return TP_ERR_EOF;

    *ptr = r->buf + (r->pos / 8);
    r->pos += (uint64_t)n * 8;
    return TP_OK;
}
