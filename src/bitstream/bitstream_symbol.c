/**
 * @file bitstream_symbol.c
 * @brief Fixed-width symbol and UTF-8 codepoint read/write.
 */

#include "bitstream_internal.h"

/* ── Symbol ──────────────────────────────────────────────────────────── */

tp_result tp_bs_read_symbol(tp_bitstream_reader *r, uint8_t bps, uint32_t *out)
{
    if (!r || !out || bps == 0 || bps > 32)
        return TP_ERR_INVALID_PARAM;

    uint64_t val;
    tp_result rc = tp_bs_read_bits(r, bps, &val);
    if (rc == TP_OK)
        *out = (uint32_t)val;
    return rc;
}

tp_result tp_bs_write_symbol(tp_bitstream_writer *w, uint32_t val, uint8_t bps)
{
    if (!w || bps == 0 || bps > 32)
        return TP_ERR_INVALID_PARAM;
    return tp_bs_write_bits(w, val, bps);
}

/* ── UTF-8 ───────────────────────────────────────────────────────────── */

tp_result tp_bs_read_utf8(tp_bitstream_reader *r, uint32_t *out)
{
    if (!r || !out)
        return TP_ERR_INVALID_PARAM;

    uint8_t first;
    tp_result rc = tp_bs_read_u8(r, &first);
    if (rc != TP_OK)
        return rc;

    uint32_t cp;
    uint8_t cont;

    if ((first & 0x80) == 0) {
        *out = first;
        return TP_OK;
    } else if ((first & 0xE0) == 0xC0) {
        cp = first & 0x1F;
        cont = 1;
    } else if ((first & 0xF0) == 0xE0) {
        cp = first & 0x0F;
        cont = 2;
    } else if ((first & 0xF8) == 0xF0) {
        cp = first & 0x07;
        cont = 3;
    } else {
        return TP_ERR_INVALID_UTF8;
    }

    for (uint8_t i = 0; i < cont; i++) {
        uint8_t b;
        rc = tp_bs_read_u8(r, &b);
        if (rc != TP_OK)
            return rc;
        if ((b & 0xC0) != 0x80)
            return TP_ERR_INVALID_UTF8;
        cp = (cp << 6) | (b & 0x3F);
    }

    *out = cp;
    return TP_OK;
}

tp_result tp_bs_write_utf8(tp_bitstream_writer *w, uint32_t cp)
{
    if (!w)
        return TP_ERR_INVALID_PARAM;

    if (cp <= 0x7F) {
        return tp_bs_write_u8(w, (uint8_t)cp);
    } else if (cp <= 0x7FF) {
        tp_result rc = tp_bs_write_u8(w, (uint8_t)(0xC0 | (cp >> 6)));
        if (rc != TP_OK) return rc;
        return tp_bs_write_u8(w, (uint8_t)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        tp_result rc = tp_bs_write_u8(w, (uint8_t)(0xE0 | (cp >> 12)));
        if (rc != TP_OK) return rc;
        rc = tp_bs_write_u8(w, (uint8_t)(0x80 | ((cp >> 6) & 0x3F)));
        if (rc != TP_OK) return rc;
        return tp_bs_write_u8(w, (uint8_t)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        tp_result rc = tp_bs_write_u8(w, (uint8_t)(0xF0 | (cp >> 18)));
        if (rc != TP_OK) return rc;
        rc = tp_bs_write_u8(w, (uint8_t)(0x80 | ((cp >> 12) & 0x3F)));
        if (rc != TP_OK) return rc;
        rc = tp_bs_write_u8(w, (uint8_t)(0x80 | ((cp >> 6) & 0x3F)));
        if (rc != TP_OK) return rc;
        return tp_bs_write_u8(w, (uint8_t)(0x80 | (cp & 0x3F)));
    }

    return TP_ERR_INVALID_UTF8;
}
