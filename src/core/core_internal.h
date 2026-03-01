/**
 * @file core_internal.h
 * @brief Internal types for the core trie codec.
 */

#ifndef CORE_INTERNAL_H
#define CORE_INTERNAL_H

#include "triepack/triepack.h"
#include "triepack/triepack_bitstream.h"

#include <stdlib.h>
#include <string.h>

/** Magic bytes: "TRP\0" */
#define TP_MAGIC_0 0x54
#define TP_MAGIC_1 0x52
#define TP_MAGIC_2 0x50
#define TP_MAGIC_3 0x00

#define TP_FORMAT_VERSION_MAJOR 1
#define TP_FORMAT_VERSION_MINOR 0

/* ── Control codes ──────────────────────────────────────────────────── */

#define TP_CTRL_END          0
#define TP_CTRL_END_VAL      1
#define TP_CTRL_SKIP         2
#define TP_CTRL_SUFFIX       3
#define TP_CTRL_ESCAPE       4
#define TP_CTRL_BRANCH       5
#define TP_NUM_CONTROL_CODES 6

/* ── Internal header (mirrors the 32-byte file header) ──────────────── */

typedef struct tp_header {
    uint8_t magic[4];
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t flags;
    uint32_t num_keys;
    uint32_t trie_data_offset;
    uint32_t value_store_offset;
    uint32_t suffix_table_offset;
    uint32_t total_data_bits;
    uint32_t reserved;
} tp_header;

/* Header flag bit positions */
#define TP_FLAG_HAS_VALUES       (1u << 0)
#define TP_FLAG_HAS_SUFFIX_TABLE (1u << 1)
#define TP_FLAG_HAS_NESTED_DICTS (1u << 2)
#define TP_FLAG_COMPACT_MODE     (1u << 3)

/* ── Key/value entry for encoder accumulation ───────────────────────── */

typedef struct tp_entry {
    char *key;
    size_t key_len;
    tp_value val;
} tp_entry;

/* ── Symbol analysis results ────────────────────────────────────────── */

typedef struct tp_symbol_info {
    uint8_t bits_per_symbol;
    uint16_t symbol_count;                     /* alphabet + control codes */
    uint16_t alphabet_size;                    /* unique byte values in keys */
    uint32_t ctrl_codes[TP_NUM_CONTROL_CODES]; /* symbol code for each ctrl */
    uint32_t symbol_map[256];                  /* byte value -> N-bit code */
    uint8_t reverse_map[256];                  /* N-bit code -> byte value (for decode) */
    bool code_is_ctrl[256];                    /* true if code is a control code */
} tp_symbol_info;

/* ── Encoder ────────────────────────────────────────────────────────── */

struct tp_encoder {
    tp_encoder_options opts;
    uint32_t count;
    tp_entry *entries;
    size_t entries_cap;
    bool sorted;
    tp_symbol_info sym;
};

/* ── Dictionary (decoder) ───────────────────────────────────────────── */

struct tp_dict {
    const uint8_t *buf;
    size_t len;
    tp_dict_info info;
    tp_header hdr;
    uint64_t trie_start;  /* absolute bit offset of prefix trie data */
    uint64_t value_start; /* absolute bit offset of value store */
    tp_symbol_info sym;
};

/* ── Iterator ───────────────────────────────────────────────────────── */

#define TP_ITER_MAX_DEPTH 256

typedef struct tp_iter_frame {
    uint64_t bit_pos;
    uint32_t remaining_children;
    size_t key_prefix_len;
} tp_iter_frame;

struct tp_iterator {
    const tp_dict *dict;
    uint64_t pos;
    uint8_t distance;
    bool done;
    char *key_buf;
    size_t key_buf_cap;
    size_t key_len;
    tp_iter_frame stack[TP_ITER_MAX_DEPTH];
    int stack_top;
    bool started;
};

/* ── Internal function declarations ─────────────────────────────────── */

/* header.c */
tp_result tp_header_write(tp_bitstream_writer *w, const tp_header *h);
tp_result tp_header_read(tp_bitstream_reader *r, tp_header *h);

/* integrity.c */
uint32_t tp_crc32(const uint8_t *data, size_t len);

/* value.c */
tp_result tp_value_encode(tp_bitstream_writer *w, const tp_value *val);
tp_result tp_value_decode(tp_bitstream_reader *r, tp_value *val, const uint8_t *base_buf);

#endif /* CORE_INTERNAL_H */
