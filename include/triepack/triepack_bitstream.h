/**
 * @file triepack_bitstream.h
 * @brief Bitstream reader/writer API for the triepack library.
 *
 * Provides arbitrary-width bit field I/O, VarInt encoding, and symbol-level
 * read/write. The reader is ROM-safe (operates on const buffers with no
 * allocation after create). Stateless _at() functions need no object at all.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_BITSTREAM_H
#define TRIEPACK_BITSTREAM_H

#include "triepack_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ────────────────────────────────────────────────────── */

typedef struct tp_bitstream_reader tp_bitstream_reader;
typedef struct tp_bitstream_writer tp_bitstream_writer;

typedef enum {
    TP_BIT_ORDER_MSB_FIRST = 0, /**< Default, matches triepack format */
    TP_BIT_ORDER_LSB_FIRST = 1
} tp_bit_order;

/* ── Reader construction ─────────────────────────────────────────────── */

/**
 * @brief Create a reader over an existing buffer (zero-copy, ROM-safe).
 *
 * The caller must keep @p buf alive for the lifetime of the reader.
 * @p bit_len is the number of valid bits in @p buf.
 */
tp_result tp_bs_reader_create(tp_bitstream_reader **out, const uint8_t *buf, uint64_t bit_len);

/**
 * @brief Create a reader that copies the buffer (safe if original is freed).
 */
tp_result tp_bs_reader_create_copy(tp_bitstream_reader **out, const uint8_t *buf, uint64_t bit_len);

/**
 * @brief Destroy a reader and set the pointer to NULL.
 */
tp_result tp_bs_reader_destroy(tp_bitstream_reader **reader);

/**
 * @brief Set the bit ordering for subsequent reads.
 */
tp_result tp_bs_reader_set_bit_order(tp_bitstream_reader *r, tp_bit_order order);

/* ── Writer construction ─────────────────────────────────────────────── */

/**
 * @brief Create a writer with the given initial capacity and growth increment.
 *
 * @param out          Receives the new writer handle.
 * @param initial_cap  Initial buffer size in bytes (0 = use default).
 * @param growth       Growth increment in bytes (0 = double on realloc).
 */
tp_result tp_bs_writer_create(tp_bitstream_writer **out, size_t initial_cap, size_t growth);

/**
 * @brief Destroy a writer and set the pointer to NULL.
 */
tp_result tp_bs_writer_destroy(tp_bitstream_writer **writer);

/* ── Reader cursor ───────────────────────────────────────────────────── */

/** Current read position in bits. */
uint64_t tp_bs_reader_position(const tp_bitstream_reader *r);

/** Number of bits remaining from cursor to end. */
uint64_t tp_bs_reader_remaining(const tp_bitstream_reader *r);

/** Total length of the stream in bits. */
uint64_t tp_bs_reader_length(const tp_bitstream_reader *r);

/** Seek to an absolute bit position. */
tp_result tp_bs_reader_seek(tp_bitstream_reader *r, uint64_t bit_pos);

/** Advance the cursor by @p n bits. */
tp_result tp_bs_reader_advance(tp_bitstream_reader *r, uint64_t n);

/** Advance to the next byte boundary (no-op if already aligned). */
tp_result tp_bs_reader_align_to_byte(tp_bitstream_reader *r);

/** Check whether the cursor is on a byte boundary. */
bool tp_bs_reader_is_byte_aligned(const tp_bitstream_reader *r);

/* ── Bit-level read ──────────────────────────────────────────────────── */

/** Read @p n bits (1..64) as an unsigned value. */
tp_result tp_bs_read_bits(tp_bitstream_reader *r, unsigned int n, uint64_t *out);

/** Read @p n bits (1..64) as a sign-extended value. */
tp_result tp_bs_read_bits_signed(tp_bitstream_reader *r, unsigned int n, int64_t *out);

/** Read a single bit. */
tp_result tp_bs_read_bit(tp_bitstream_reader *r, uint8_t *out);

/** Peek @p n bits without advancing the cursor. */
tp_result tp_bs_peek_bits(tp_bitstream_reader *r, unsigned int n, uint64_t *out);

/** Read @p n bits (1..32) as a 32-bit unsigned value. */
tp_result tp_bs_read_bits32(tp_bitstream_reader *r, unsigned int n, uint32_t *out);

/* ── Bit-level write ─────────────────────────────────────────────────── */

/** Write the low @p n bits (1..64) of @p value. */
tp_result tp_bs_write_bits(tp_bitstream_writer *w, uint64_t value, unsigned int n);

/**
 * @brief Write the low @p n bits (1..64) of a signed value.
 *
 * The value is truncated to @p n bits.  On read-back with
 * tp_bs_read_bits_signed(), the MSB is sign-extended to recover the
 * original signed value.  The valid range for a @p n-bit signed field
 * is -(2^(n-1)) to 2^(n-1)-1.
 *
 * Example: writing -3 as a 5-bit signed field stores 11101 (binary).
 * Reading 5 signed bits back yields -3.
 */
tp_result tp_bs_write_bits_signed(tp_bitstream_writer *w, int64_t value, unsigned int n);

/** Write a single bit. */
tp_result tp_bs_write_bit(tp_bitstream_writer *w, uint8_t value);

/** Pad with zero bits to the next byte boundary. */
tp_result tp_bs_writer_align_to_byte(tp_bitstream_writer *w);

/** Current write position in bits. */
uint64_t tp_bs_writer_position(const tp_bitstream_writer *w);

/* ── Byte-level read ─────────────────────────────────────────────────── */

/** Read an 8-bit unsigned integer (1 byte). */
tp_result tp_bs_read_u8(tp_bitstream_reader *r, uint8_t *out);

/** Read a 16-bit unsigned integer (2 bytes, big-endian). */
tp_result tp_bs_read_u16(tp_bitstream_reader *r, uint16_t *out);

/** Read a 32-bit unsigned integer (4 bytes, big-endian). */
tp_result tp_bs_read_u32(tp_bitstream_reader *r, uint32_t *out);

/** Read a 64-bit unsigned integer (8 bytes, big-endian). */
tp_result tp_bs_read_u64(tp_bitstream_reader *r, uint64_t *out);

/** Read @p n raw bytes into @p buf. */
tp_result tp_bs_read_bytes(tp_bitstream_reader *r, uint8_t *buf, size_t n);

/**
 * @brief Get a direct pointer into the underlying buffer.
 *
 * Returns TP_ERR_NOT_ALIGNED if cursor is not byte-aligned.
 * The pointer is valid as long as the reader and its backing buffer exist.
 */
tp_result tp_bs_reader_direct_ptr(tp_bitstream_reader *r, const uint8_t **ptr, size_t n);

/* ── Byte-level write ────────────────────────────────────────────────── */

/** Write an 8-bit unsigned integer (1 byte). */
tp_result tp_bs_write_u8(tp_bitstream_writer *w, uint8_t val);

/** Write a 16-bit unsigned integer (2 bytes, big-endian). */
tp_result tp_bs_write_u16(tp_bitstream_writer *w, uint16_t val);

/** Write a 32-bit unsigned integer (4 bytes, big-endian). */
tp_result tp_bs_write_u32(tp_bitstream_writer *w, uint32_t val);

/** Write a 64-bit unsigned integer (8 bytes, big-endian). */
tp_result tp_bs_write_u64(tp_bitstream_writer *w, uint64_t val);

/** Write @p n raw bytes from @p buf. */
tp_result tp_bs_write_bytes(tp_bitstream_writer *w, const uint8_t *buf, size_t n);

/* ── VarInt ──────────────────────────────────────────────────────────── */

/** Read a variable-length unsigned integer (LEB128-style). */
tp_result tp_bs_read_varint_u(tp_bitstream_reader *r, uint64_t *out);

/** Read a variable-length signed integer (zigzag + LEB128). */
tp_result tp_bs_read_varint_s(tp_bitstream_reader *r, int64_t *out);

/** Write a variable-length unsigned integer. */
tp_result tp_bs_write_varint_u(tp_bitstream_writer *w, uint64_t value);

/** Write a variable-length signed integer (zigzag-encoded). */
tp_result tp_bs_write_varint_s(tp_bitstream_writer *w, int64_t value);

/* ── Symbol ──────────────────────────────────────────────────────────── */

/** Read a fixed-width symbol of @p bps bits. */
tp_result tp_bs_read_symbol(tp_bitstream_reader *r, unsigned int bps, uint32_t *out);

/** Write a fixed-width symbol of @p bps bits. */
tp_result tp_bs_write_symbol(tp_bitstream_writer *w, uint32_t val, unsigned int bps);

/** Read a UTF-8 encoded codepoint. */
tp_result tp_bs_read_utf8(tp_bitstream_reader *r, uint32_t *out);

/** Write a UTF-8 encoded codepoint. */
tp_result tp_bs_write_utf8(tp_bitstream_writer *w, uint32_t cp);

/* ── Stateless ROM functions ─────────────────────────────────────────── */

/**
 * @brief Read @p n bits at a given bit offset from a raw buffer.
 *
 * No object, no cursor, no allocation. Safe for ROM.
 */
tp_result tp_bs_read_bits_at(const uint8_t *buf, uint64_t bit_pos, unsigned int n, uint64_t *out);

/** Read @p n bits at a given bit offset as a sign-extended value. */
tp_result tp_bs_read_bits_signed_at(const uint8_t *buf, uint64_t bit_pos, unsigned int n,
                                    int64_t *out);

/**
 * @brief Read a VarInt at a given bit offset.
 *
 * @param buf        Source buffer.
 * @param bit_pos    Bit offset into @p buf.
 * @param out        Receives the decoded unsigned value.
 * @param bits_read  On success, set to the number of bits consumed.
 */
tp_result tp_bs_read_varint_u_at(const uint8_t *buf, uint64_t bit_pos, uint64_t *out,
                                 unsigned int *bits_read);

/* ── Buffer access ───────────────────────────────────────────────────── */

/**
 * @brief Get a read-only view of the writer's buffer.
 */
tp_result tp_bs_writer_get_buffer(const tp_bitstream_writer *w, const uint8_t **buf,
                                  uint64_t *bit_len);

/**
 * @brief Detach the buffer from the writer (caller takes ownership).
 *
 * After this call the writer is empty (position = 0).
 */
tp_result tp_bs_writer_detach_buffer(tp_bitstream_writer *w, uint8_t **buf, size_t *byte_len,
                                     uint64_t *bit_len);

/**
 * @brief Append raw bits from an external buffer to the writer.
 */
tp_result tp_bs_writer_append_buffer(tp_bitstream_writer *w, const uint8_t *buf, uint64_t bit_len);

/**
 * @brief Create a reader from the writer's current contents.
 *
 * The reader references the writer's buffer (zero-copy). The writer must
 * outlive the reader.
 */
tp_result tp_bs_writer_to_reader(tp_bitstream_writer *w, tp_bitstream_reader **reader);

/**
 * @brief Get a read-only view of the reader's underlying buffer.
 */
tp_result tp_bs_reader_get_buffer(const tp_bitstream_reader *r, const uint8_t **buf,
                                  uint64_t *bit_len);

/* ── Bulk ────────────────────────────────────────────────────────────── */

/**
 * @brief Copy @p n_bits from reader to writer.
 */
tp_result tp_bs_copy_bits(tp_bitstream_reader *r, tp_bitstream_writer *w, uint64_t n_bits);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_BITSTREAM_H */
