/**
 * @file core_internal.h
 * @brief Internal types for the core trie codec.
 */

#ifndef CORE_INTERNAL_H
#define CORE_INTERNAL_H

#include "triepack/triepack.h"
#include "triepack/triepack_bitstream.h"

/** Magic bytes: "TRP\0" */
#define TP_MAGIC_0 0x54
#define TP_MAGIC_1 0x52
#define TP_MAGIC_2 0x50
#define TP_MAGIC_3 0x00

#define TP_FORMAT_VERSION_MAJOR 1
#define TP_FORMAT_VERSION_MINOR 0

struct tp_encoder {
    tp_encoder_options opts;
    uint32_t           count;
    /* TODO: key/value storage during accumulation phase */
};

struct tp_dict {
    const uint8_t *buf;
    size_t         len;
    tp_dict_info   info;
    /* TODO: parsed header, trie root offset, etc. */
};

struct tp_iterator {
    const tp_dict *dict;
    uint64_t       pos;
    uint8_t        distance; /* for fuzzy search results */
    bool           done;
    /* TODO: traversal state */
};

#endif /* CORE_INTERNAL_H */
