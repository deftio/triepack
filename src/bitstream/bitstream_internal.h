/**
 * @file bitstream_internal.h
 * @brief Internal types for the bitstream implementation.
 */

#ifndef BITSTREAM_INTERNAL_H
#define BITSTREAM_INTERNAL_H

#include "triepack/triepack_bitstream.h"

struct tp_bitstream_reader {
    const uint8_t *buf;
    uint64_t bit_len;
    uint64_t pos;
    tp_bit_order order;
    bool owns_buf;
};

struct tp_bitstream_writer {
    uint8_t *buf;
    size_t cap;
    size_t growth;
    uint64_t pos;
};

#endif /* BITSTREAM_INTERNAL_H */
