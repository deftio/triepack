/**
 * @file bitstream.c
 * @brief Reader/writer construction, destruction, and common result helpers.
 */

#include "bitstream_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Result string ───────────────────────────────────────────────────── */

const char *tp_result_str(tp_result result)
{
    switch (result) {
    case TP_OK:
        return "OK";
    case TP_ERR_EOF:
        return "read past end of stream";
    case TP_ERR_ALLOC:
        return "memory allocation failed";
    case TP_ERR_INVALID_PARAM:
        return "invalid parameter";
    case TP_ERR_INVALID_POSITION:
        return "seek beyond bounds";
    case TP_ERR_NOT_ALIGNED:
        return "not byte-aligned";
    case TP_ERR_OVERFLOW:
        return "varint overflow";
    case TP_ERR_INVALID_UTF8:
        return "invalid UTF-8";
    case TP_ERR_BAD_MAGIC:
        return "bad magic bytes";
    case TP_ERR_VERSION:
        return "unsupported version";
    case TP_ERR_CORRUPT:
        return "data corrupt";
    case TP_ERR_NOT_FOUND:
        return "key not found";
    case TP_ERR_TRUNCATED:
        return "data truncated";
    case TP_ERR_JSON_SYNTAX:
        return "JSON syntax error";
    case TP_ERR_JSON_DEPTH:
        return "JSON nesting too deep";
    case TP_ERR_JSON_TYPE:
        return "JSON type mismatch";
    default:
        return "unknown error";
    }
}

/* ── Value helpers ───────────────────────────────────────────────────── */

tp_value tp_value_null(void)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_NULL;
    return v;
}

tp_value tp_value_bool(bool val)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_BOOL;
    v.data.bool_val = val;
    return v;
}

tp_value tp_value_int(int64_t val)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_INT;
    v.data.int_val = val;
    return v;
}

tp_value tp_value_uint(uint64_t val)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_UINT;
    v.data.uint_val = val;
    return v;
}

tp_value tp_value_float32(float val)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_FLOAT32;
    v.data.float32_val = val;
    return v;
}

tp_value tp_value_float64(double val)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_FLOAT64;
    v.data.float64_val = val;
    return v;
}

tp_value tp_value_string(const char *str)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_STRING;
    v.data.string_val.str = str;
    v.data.string_val.str_len = str ? strlen(str) : 0;
    return v;
}

tp_value tp_value_string_n(const char *str, size_t len)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_STRING;
    v.data.string_val.str = str;
    v.data.string_val.str_len = len;
    return v;
}

tp_value tp_value_blob(const uint8_t *data, size_t len)
{
    tp_value v;
    memset(&v, 0, sizeof(v));
    v.type = TP_BLOB;
    v.data.blob_val.data = data;
    v.data.blob_val.len = len;
    return v;
}

/* ── Reader lifecycle ────────────────────────────────────────────────── */

static const size_t DEFAULT_WRITER_CAP = 256;

tp_result tp_bs_reader_create(tp_bitstream_reader **out, const uint8_t *buf, uint64_t bit_len)
{
    if (!out)
        return TP_ERR_INVALID_PARAM;

    /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
    tp_bitstream_reader *r = calloc(1, sizeof(*r));
    if (!r)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    r->buf = buf;
    r->bit_len = bit_len;
    r->pos = 0;
    r->order = TP_BIT_ORDER_MSB_FIRST;
    r->owns_buf = false;
    *out = r;
    return TP_OK;
}

tp_result tp_bs_reader_create_copy(tp_bitstream_reader **out, const uint8_t *buf, uint64_t bit_len)
{
    if (!out)
        return TP_ERR_INVALID_PARAM;

    size_t byte_len = (size_t)((bit_len + 7) / 8);
    uint8_t *copy = malloc(byte_len);
    if (!copy && byte_len > 0)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    if (buf && byte_len > 0)
        memcpy(copy, buf, byte_len);

    tp_bitstream_reader *r = calloc(1, sizeof(*r));
    if (!r) {
        /* LCOV_EXCL_START */
        free(copy);
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }

    r->buf = copy;
    r->bit_len = bit_len;
    r->pos = 0;
    r->order = TP_BIT_ORDER_MSB_FIRST;
    r->owns_buf = true;
    *out = r;
    return TP_OK;
}

tp_result tp_bs_reader_destroy(tp_bitstream_reader **reader)
{
    if (!reader)
        return TP_ERR_INVALID_PARAM;
    if (*reader) {
        if ((*reader)->owns_buf)
            free((void *)(*reader)->buf);
        free(*reader);
        *reader = NULL;
    }
    return TP_OK;
}

tp_result tp_bs_reader_set_bit_order(tp_bitstream_reader *r, tp_bit_order order)
{
    if (!r)
        return TP_ERR_INVALID_PARAM;
    r->order = order;
    return TP_OK;
}

/* ── Writer lifecycle ────────────────────────────────────────────────── */

tp_result tp_bs_writer_create(tp_bitstream_writer **out, size_t initial_cap, size_t growth)
{
    if (!out)
        return TP_ERR_INVALID_PARAM;

    tp_bitstream_writer *w = calloc(1, sizeof(*w));
    if (!w)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    w->cap = initial_cap > 0 ? initial_cap : DEFAULT_WRITER_CAP;
    w->growth = growth;
    w->pos = 0;
    w->buf = calloc(1, w->cap);
    if (!w->buf) {
        /* LCOV_EXCL_START */
        free(w);
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }

    *out = w;
    return TP_OK;
}

tp_result tp_bs_writer_destroy(tp_bitstream_writer **writer)
{
    if (!writer)
        return TP_ERR_INVALID_PARAM;
    if (*writer) {
        free((*writer)->buf);
        free(*writer);
        *writer = NULL;
    }
    return TP_OK;
}

/* ── Reader cursor ───────────────────────────────────────────────────── */

uint64_t tp_bs_reader_position(const tp_bitstream_reader *r)
{
    return r ? r->pos : 0;
}

uint64_t tp_bs_reader_remaining(const tp_bitstream_reader *r)
{
    if (!r || r->pos >= r->bit_len)
        return 0;
    return r->bit_len - r->pos;
}

uint64_t tp_bs_reader_length(const tp_bitstream_reader *r)
{
    return r ? r->bit_len : 0;
}

tp_result tp_bs_reader_seek(tp_bitstream_reader *r, uint64_t bit_pos)
{
    if (!r)
        return TP_ERR_INVALID_PARAM;
    if (bit_pos > r->bit_len)
        return TP_ERR_INVALID_POSITION;
    r->pos = bit_pos;
    return TP_OK;
}

tp_result tp_bs_reader_advance(tp_bitstream_reader *r, uint64_t n)
{
    if (!r)
        return TP_ERR_INVALID_PARAM;
    if (r->pos + n > r->bit_len)
        return TP_ERR_EOF;
    r->pos += n;
    return TP_OK;
}

tp_result tp_bs_reader_align_to_byte(tp_bitstream_reader *r)
{
    if (!r)
        return TP_ERR_INVALID_PARAM;
    uint64_t rem = r->pos % 8;
    if (rem != 0) {
        uint64_t skip = 8 - rem;
        if (r->pos + skip > r->bit_len)
            return TP_ERR_EOF;
        r->pos += skip;
    }
    return TP_OK;
}

bool tp_bs_reader_is_byte_aligned(const tp_bitstream_reader *r)
{
    return r ? (r->pos % 8 == 0) : false;
}

/* ── Writer position ─────────────────────────────────────────────────── */

uint64_t tp_bs_writer_position(const tp_bitstream_writer *w)
{
    return w ? w->pos : 0;
}

/* ── Buffer access ───────────────────────────────────────────────────── */

tp_result tp_bs_writer_get_buffer(const tp_bitstream_writer *w, const uint8_t **buf,
                                  uint64_t *bit_len)
{
    if (!w || !buf || !bit_len)
        return TP_ERR_INVALID_PARAM;
    *buf = w->buf;
    *bit_len = w->pos;
    return TP_OK;
}

tp_result tp_bs_writer_detach_buffer(tp_bitstream_writer *w, uint8_t **buf, size_t *byte_len,
                                     uint64_t *bit_len)
{
    if (!w || !buf || !byte_len || !bit_len)
        return TP_ERR_INVALID_PARAM;
    *buf = w->buf;
    *byte_len = (size_t)((w->pos + 7) / 8);
    *bit_len = w->pos;

    /* Reset writer to empty state */
    w->buf = calloc(1, w->cap);
    if (!w->buf) {
        /* LCOV_EXCL_START */
        w->cap = 0;
        w->pos = 0;
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }
    w->pos = 0;
    return TP_OK;
}

tp_result tp_bs_reader_get_buffer(const tp_bitstream_reader *r, const uint8_t **buf,
                                  uint64_t *bit_len)
{
    if (!r || !buf || !bit_len)
        return TP_ERR_INVALID_PARAM;
    *buf = r->buf;
    *bit_len = r->bit_len;
    return TP_OK;
}

tp_result tp_bs_writer_to_reader(tp_bitstream_writer *w, tp_bitstream_reader **reader)
{
    if (!w || !reader)
        return TP_ERR_INVALID_PARAM;
    return tp_bs_reader_create(reader, w->buf, w->pos);
}
