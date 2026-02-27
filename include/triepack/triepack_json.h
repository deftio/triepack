/**
 * @file triepack_json.h
 * @brief JSON encode/decode and DOM-style access for triepack.
 *
 * Encodes JSON documents into the .trp binary format and decodes them
 * back. Provides a DOM-style API for path-based lookups without
 * decoding the entire document.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_JSON_H
#define TRIEPACK_JSON_H

#include "triepack_common.h"
#include "triepack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque type ─────────────────────────────────────────────────────── */

typedef struct tp_json tp_json;

/* ── One-shot encode/decode ──────────────────────────────────────────── */

/**
 * @brief Encode a JSON string into a .trp buffer.
 *
 * On success, @p buf is allocated (caller must free()) and @p buf_len is set.
 */
tp_result tp_json_encode(const char *json_str, size_t json_len,
                         uint8_t **buf, size_t *buf_len);

/**
 * @brief Decode a .trp buffer back to a JSON string.
 *
 * On success, @p json_str is allocated (caller must free()).
 */
tp_result tp_json_decode(const uint8_t *buf, size_t buf_len,
                         char **json_str, size_t *json_len);

/**
 * @brief Decode a .trp buffer to a pretty-printed JSON string.
 *
 * @param indent  Indentation string per level (e.g. "  " or "\t").
 */
tp_result tp_json_decode_pretty(const uint8_t *buf, size_t buf_len,
                                const char *indent,
                                char **json_str, size_t *json_len);

/* ── DOM-style access ────────────────────────────────────────────────── */

/** Open a .trp buffer for DOM-style JSON access. */
tp_result tp_json_open(tp_json **out, const uint8_t *buf, size_t buf_len);

/** Close a JSON DOM handle and set the pointer to NULL. */
tp_result tp_json_close(tp_json **json);

/**
 * @brief Look up a value by dot/bracket path.
 *
 * Examples: "name", "address.city", "items[0].price"
 */
tp_result tp_json_lookup_path(const tp_json *j, const char *path, tp_value *val);

/** Get the type of the root JSON value. */
tp_result tp_json_root_type(const tp_json *j, tp_value_type *type);

/** Iterate over the top-level keys (object) or elements (array). */
tp_result tp_json_iterate(const tp_json *j, tp_iterator **out);

/** Return the number of top-level keys or elements. */
uint32_t  tp_json_count(const tp_json *j);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_JSON_H */
