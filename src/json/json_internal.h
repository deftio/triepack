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

struct tp_json {
    const uint8_t *buf;
    size_t         buf_len;
    /* TODO: parsed header, root node offset, cached counts, etc. */
};

#endif /* JSON_INTERNAL_H */
