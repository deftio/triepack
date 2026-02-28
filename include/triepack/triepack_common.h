/**
 * @file triepack_common.h
 * @brief Shared types and error codes for the triepack library.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_COMMON_H
#define TRIEPACK_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error codes ─────────────────────────────────────────────────────── */

typedef enum {
    TP_OK = 0,

    /* Bitstream errors */
    TP_ERR_EOF = -1,              /**< Read past end of stream */
    TP_ERR_ALLOC = -2,            /**< Memory allocation failed */
    TP_ERR_INVALID_PARAM = -3,    /**< NULL pointer or invalid argument */
    TP_ERR_INVALID_POSITION = -4, /**< Seek beyond bounds */
    TP_ERR_NOT_ALIGNED = -5,      /**< Byte op on non-aligned cursor */
    TP_ERR_OVERFLOW = -6,         /**< VarInt exceeds max groups */
    TP_ERR_INVALID_UTF8 = -7,     /**< Malformed UTF-8 */

    /* Dictionary errors */
    TP_ERR_BAD_MAGIC = -10, /**< Not a .trp file */
    TP_ERR_VERSION = -11,   /**< Unsupported format version */
    TP_ERR_CORRUPT = -12,   /**< Integrity check failed */
    TP_ERR_NOT_FOUND = -13, /**< Key not in dictionary */
    TP_ERR_TRUNCATED = -14, /**< Data shorter than header claims */

    /* JSON errors */
    TP_ERR_JSON_SYNTAX = -20, /**< Malformed JSON */
    TP_ERR_JSON_DEPTH = -21,  /**< Nesting exceeds limit */
    TP_ERR_JSON_TYPE = -22    /**< Type mismatch on path lookup */
} tp_result;

/**
 * @brief Return a human-readable string for the given result code.
 */
const char *tp_result_str(tp_result result);

/* ── Value types ─────────────────────────────────────────────────────── */

/** Value type tags (match binary format 4-bit tags). */
typedef enum {
    TP_NULL = 0,
    TP_BOOL,
    TP_INT,
    TP_UINT,
    TP_FLOAT32,
    TP_FLOAT64,
    TP_STRING,
    TP_BLOB,
    TP_ARRAY,
    TP_DICT
} tp_value_type;

/** Tagged union for values stored in a triepack dictionary. */
typedef struct tp_value {
    tp_value_type type;
    union {
        bool bool_val;
        int64_t int_val;
        uint64_t uint_val;
        float float32_val;
        double float64_val;
        struct {
            const char *str;
            size_t str_len;
        } string_val;
        struct {
            const uint8_t *data;
            size_t len;
        } blob_val;
        struct {
            const struct tp_value *elements;
            size_t count;
        } array_val;
        struct {
            const uint8_t *dict_buf;
            size_t dict_len;
        } dict_val;
    } data;
} tp_value;

/* ── Addressing modes ────────────────────────────────────────────────── */

typedef enum {
    TP_ADDR_BIT = 0,
    TP_ADDR_BYTE,
    TP_ADDR_SYMBOL_FIXED,
    TP_ADDR_SYMBOL_UTF8
} tp_addr_mode;

/* ── Checksum types ──────────────────────────────────────────────────── */

typedef enum { TP_CHECKSUM_CRC32 = 0, TP_CHECKSUM_SHA256, TP_CHECKSUM_XXHASH64 } tp_checksum_type;

/* ── Format constants ────────────────────────────────────────────────── */

#define TP_HEADER_SIZE       32 /**< Fixed header size in bytes */
#define TP_MAX_NESTING_DEPTH 32 /**< Maximum nesting depth for JSON */
#define TP_VARINT_MAX_GROUPS 10 /**< Maximum VarInt continuation groups */

/* ── Value construction helpers ───────────────────────────────────────── */

tp_value tp_value_null(void);
tp_value tp_value_bool(bool val);
tp_value tp_value_int(int64_t val);
tp_value tp_value_uint(uint64_t val);
tp_value tp_value_float32(float val);
tp_value tp_value_float64(double val);
tp_value tp_value_string(const char *str);
tp_value tp_value_string_n(const char *str, size_t len);
tp_value tp_value_blob(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_COMMON_H */
