/**
 * @file json_internal.h
 * @brief Internal types for the JSON encode/decode/DOM layer.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include "triepack/triepack.h"
#include "triepack/triepack_json.h"

#include <stdlib.h>
#include <string.h>

/** Internal key prefix for metadata stored alongside user data. */
#define TP_JSON_META_ROOT "\x01root"

/** Root type values stored as uint in the metadata key. */
#define TP_JSON_ROOT_OBJECT 1
#define TP_JSON_ROOT_ARRAY  2

struct tp_json {
    tp_dict *dict;      /**< Opened dictionary handle (owned) */
    uint8_t *buf_owned; /**< Copy of buffer (owned, freed on close) */
    size_t buf_len;     /**< Length of owned buffer */
    tp_value_type root; /**< Cached root type (TP_DICT or TP_ARRAY) */
    uint32_t count;     /**< Cached top-level key/element count */
};

#endif /* JSON_INTERNAL_H */
