/**
 * @file triepack.h
 * @brief Core trie codec API for the triepack library.
 *
 * Encoder: batch-add key/value pairs, then build a compressed trie.
 * Dict: open a compiled .trp buffer for O(key-length) lookups.
 * Iterator: traverse search results without allocating result arrays.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_H
#define TRIEPACK_H

#include "triepack_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ────────────────────────────────────────────────────── */

typedef struct tp_encoder tp_encoder;
typedef struct tp_dict tp_dict;
typedef struct tp_iterator tp_iterator;

/* ── Encoder options ─────────────────────────────────────────────────── */

typedef struct tp_encoder_options {
    tp_addr_mode trie_mode;    /**< default: TP_ADDR_BIT */
    tp_addr_mode value_mode;   /**< default: TP_ADDR_BYTE */
    tp_checksum_type checksum; /**< default: TP_CHECKSUM_CRC32 */
    bool enable_suffix;        /**< default: true */
    bool compact_mode;         /**< default: false */
    uint8_t bits_per_symbol;   /**< 0=auto, 1-15=fixed */
} tp_encoder_options;

/** Return default encoder options. */
tp_encoder_options tp_encoder_defaults(void);

/* ── Encoder lifecycle ───────────────────────────────────────────────── */

/** Create an encoder with default options. */
tp_result tp_encoder_create(tp_encoder **out);

/** Create an encoder with custom options. */
tp_result tp_encoder_create_ex(tp_encoder **out, const tp_encoder_options *opts);

/** Add a NUL-terminated key with an associated value. */
tp_result tp_encoder_add(tp_encoder *enc, const char *key, const tp_value *val);

/** Add a key of explicit length with an associated value. */
tp_result tp_encoder_add_n(tp_encoder *enc, const char *key, size_t key_len, const tp_value *val);

/** Return the number of entries added so far. */
uint32_t tp_encoder_count(const tp_encoder *enc);

/**
 * @brief Build the compressed trie.
 *
 * On success, @p buf is allocated (caller must free()) and @p len is set.
 */
tp_result tp_encoder_build(tp_encoder *enc, uint8_t **buf, size_t *len);

/** Reset the encoder for reuse (clears all entries, keeps options). */
tp_result tp_encoder_reset(tp_encoder *enc);

/** Destroy the encoder and set the pointer to NULL. */
tp_result tp_encoder_destroy(tp_encoder **enc);

/* ── Dictionary (decoder) ────────────────────────────────────────────── */

/**
 * @brief Open a compiled .trp buffer for reading.
 *
 * Validates magic bytes, version, and checksum before returning.
 * The caller must keep @p buf alive for the lifetime of the dict.
 */
tp_result tp_dict_open(tp_dict **out, const uint8_t *buf, size_t len);

/**
 * @brief Open without integrity checks (faster, for trusted data).
 */
tp_result tp_dict_open_unchecked(tp_dict **out, const uint8_t *buf, size_t len);

/** Close a dictionary and set the pointer to NULL. */
tp_result tp_dict_close(tp_dict **dict);

/** Return the number of keys in the dictionary. */
uint32_t tp_dict_count(const tp_dict *dict);

/* ── Lookup ──────────────────────────────────────────────────────────── */

/** Look up a NUL-terminated key. */
tp_result tp_dict_lookup(const tp_dict *dict, const char *key, tp_value *val);

/** Look up a key of explicit length. */
tp_result tp_dict_lookup_n(const tp_dict *dict, const char *key, size_t key_len, tp_value *val);

/** Check whether a key exists. */
tp_result tp_dict_contains(const tp_dict *dict, const char *key, bool *out);

/* ── Dictionary info ─────────────────────────────────────────────────── */

typedef struct tp_dict_info {
    uint8_t format_version_major;
    uint8_t format_version_minor;
    uint32_t num_keys;
    tp_addr_mode trie_mode;
    tp_addr_mode value_mode;
    tp_checksum_type checksum_type;
    uint8_t bits_per_symbol;
    bool has_values;
    bool has_suffix_table;
    bool compact_mode;
    size_t total_bytes;
} tp_dict_info;

/** Retrieve metadata about the dictionary. */
tp_result tp_dict_get_info(const tp_dict *dict, tp_dict_info *info);

/* ── Iteration ───────────────────────────────────────────────────────── */

/** Create an iterator over all keys in the dictionary. */
tp_result tp_dict_iterate(const tp_dict *dict, tp_iterator **out);

/**
 * @brief Advance to the next key/value pair.
 *
 * Returns TP_ERR_EOF when there are no more entries.
 */
tp_result tp_iter_next(tp_iterator *it, const char **key, size_t *key_len, tp_value *val);

/** Reset an iterator to the beginning. */
tp_result tp_iter_reset(tp_iterator *it);

/** Destroy an iterator and set the pointer to NULL. */
tp_result tp_iter_destroy(tp_iterator **it);

/* ── Search ──────────────────────────────────────────────────────────── */

/** Find all keys with the given prefix; results via iterator. */
tp_result tp_dict_find_prefix(const tp_dict *dict, const char *prefix, tp_iterator **out);

/**
 * @brief Find keys within edit distance @p max_dist of @p query.
 *
 * @param dict      Dictionary to search.
 * @param query     Query string to match against.
 * @param max_dist  Maximum Levenshtein distance (0..2 recommended).
 * @param out       Receives an iterator over matching keys.
 */
tp_result tp_dict_find_fuzzy(const tp_dict *dict, const char *query, uint8_t max_dist,
                             tp_iterator **out);

/** Get the edit distance for the current iterator position (fuzzy only). */
tp_result tp_iter_get_distance(const tp_iterator *it, uint8_t *dist);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_H */
