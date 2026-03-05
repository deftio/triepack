/**
 * @file encoder.c
 * @brief Encoder lifecycle: accumulate key/value pairs, build compressed trie.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

/* ── Value deep copy / free helpers ───────────────────────────────────── */

/**
 * Deep-copy heap-referencing data (string/blob) inside a tp_value so
 * the encoder owns it.  Must be paired with value_free_copy().
 */
static tp_result value_deep_copy(tp_value *v)
{
    if (v->type == TP_STRING && v->data.string_val.str) {
        size_t len = v->data.string_val.str_len;
        char *copy = malloc(len + 1);
        if (!copy)
            return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */
        memcpy(copy, v->data.string_val.str, len);
        copy[len] = '\0';
        v->data.string_val.str = copy;
    } else if (v->type == TP_BLOB && v->data.blob_val.data && v->data.blob_val.len > 0) {
        size_t len = v->data.blob_val.len;
        uint8_t *copy = malloc(len);
        if (!copy)
            return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */
        memcpy(copy, v->data.blob_val.data, len);
        v->data.blob_val.data = copy;
    }
    return TP_OK;
}

/** Free deep-copied data inside a tp_value. */
static void value_free_copy(tp_value *v)
{
    if (v->type == TP_STRING && v->data.string_val.str) {
        free((void *)v->data.string_val.str);
        v->data.string_val.str = NULL;
    } else if (v->type == TP_BLOB && v->data.blob_val.data) {
        free((void *)v->data.blob_val.data);
        v->data.blob_val.data = NULL;
    }
}

/* ── Defaults ────────────────────────────────────────────────────────── */

tp_encoder_options tp_encoder_defaults(void)
{
    tp_encoder_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.trie_mode = TP_ADDR_BIT;
    opts.value_mode = TP_ADDR_BYTE;
    opts.checksum = TP_CHECKSUM_CRC32;
    opts.enable_suffix = false; /* not implemented yet */
    opts.compact_mode = false;
    opts.bits_per_symbol = 0; /* auto */
    return opts;
}

/* ── Create / Destroy ────────────────────────────────────────────────── */

tp_result tp_encoder_create(tp_encoder **out)
{
    if (!out)
        return TP_ERR_INVALID_PARAM;

    tp_encoder_options opts = tp_encoder_defaults();
    return tp_encoder_create_ex(out, &opts);
}

tp_result tp_encoder_create_ex(tp_encoder **out, const tp_encoder_options *opts)
{
    if (!out || !opts)
        return TP_ERR_INVALID_PARAM;

    /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
    tp_encoder *enc = calloc(1, sizeof(*enc));
    if (!enc)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    enc->opts = *opts;
    enc->count = 0;
    enc->entries = NULL;
    enc->entries_cap = 0;
    enc->sorted = false;
    *out = enc;
    return TP_OK;
}

tp_result tp_encoder_destroy(tp_encoder **enc)
{
    if (!enc)
        return TP_ERR_INVALID_PARAM;
    if (*enc) {
        for (uint32_t i = 0; i < (*enc)->count; i++) {
            value_free_copy(&(*enc)->entries[i].val);
            free((*enc)->entries[i].key);
        }
        free((*enc)->entries);
        free(*enc);
        *enc = NULL;
    }
    return TP_OK;
}

/* ── Add entries ─────────────────────────────────────────────────────── */

tp_result tp_encoder_add(tp_encoder *enc, const char *key, const tp_value *val)
{
    if (!enc || !key)
        return TP_ERR_INVALID_PARAM;
    return tp_encoder_add_n(enc, key, strlen(key), val);
}

tp_result tp_encoder_add_n(tp_encoder *enc, const char *key, size_t key_len, const tp_value *val)
{
    if (!enc || !key)
        return TP_ERR_INVALID_PARAM;

    /* Grow entries array if needed */
    if (enc->count >= enc->entries_cap) {
        size_t new_cap = enc->entries_cap == 0 ? 16 : enc->entries_cap * 2;
        tp_entry *new_entries = realloc(enc->entries, new_cap * sizeof(tp_entry));
        if (!new_entries)
            return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */
        enc->entries = new_entries;
        enc->entries_cap = new_cap;
    }

    /* Copy key */
    char *key_copy = malloc(key_len + 1);
    if (!key_copy)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */
    memcpy(key_copy, key, key_len);
    key_copy[key_len] = '\0';

    tp_entry *e = &enc->entries[enc->count];
    e->key = key_copy;
    e->key_len = key_len;
    if (val)
        e->val = *val;
    else
        e->val = tp_value_null();

    /* Deep-copy string/blob data so the encoder owns it */
    tp_result vrc = value_deep_copy(&e->val);
    if (vrc != TP_OK) {
        /* LCOV_EXCL_START */
        free(key_copy);
        return vrc;
        /* LCOV_EXCL_STOP */
    }

    enc->count++;
    enc->sorted = false;
    return TP_OK;
}

/* ── Query ───────────────────────────────────────────────────────────── */

uint32_t tp_encoder_count(const tp_encoder *enc)
{
    if (!enc)
        return 0;
    return enc->count;
}

/* ── Sort entries ────────────────────────────────────────────────────── */

static int entry_cmp(const void *a, const void *b)
{
    const tp_entry *ea = (const tp_entry *)a;
    const tp_entry *eb = (const tp_entry *)b;
    size_t min_len = ea->key_len < eb->key_len ? ea->key_len : eb->key_len;
    int cmp = memcmp(ea->key, eb->key, min_len);
    if (cmp != 0)
        return cmp;
    if (ea->key_len < eb->key_len)
        return -1;
    if (ea->key_len > eb->key_len)
        return 1;
    return 0;
}

static void sort_entries(tp_encoder *enc)
{
    if (enc->count > 1)
        qsort(enc->entries, enc->count, sizeof(tp_entry), entry_cmp);
    enc->sorted = true;
}

/* Remove duplicate keys (keep last added) */
static void dedup_entries(tp_encoder *enc)
{
    if (enc->count <= 1)
        return;
    uint32_t write = 0;
    for (uint32_t i = 0; i < enc->count; i++) {
        if (i + 1 < enc->count && enc->entries[i].key_len == enc->entries[i + 1].key_len &&
            memcmp(enc->entries[i].key, enc->entries[i + 1].key, enc->entries[i].key_len) == 0) {
            value_free_copy(&enc->entries[i].val);
            free(enc->entries[i].key);
            continue;
        }
        if (write != i)
            enc->entries[write] = enc->entries[i];
        write++;
    }
    enc->count = write;
}

/* ── Symbol analysis ─────────────────────────────────────────────────── */

static void analyze_symbols(tp_encoder *enc)
{
    tp_symbol_info *sym = &enc->sym;
    memset(sym, 0, sizeof(*sym));

    bool used[256] = {false};

    /* Scan all keys to find used byte values */
    for (uint32_t i = 0; i < enc->count; i++) {
        for (size_t j = 0; j < enc->entries[i].key_len; j++) {
            used[(uint8_t)enc->entries[i].key[j]] = true;
        }
    }

    /* Count alphabet size */
    sym->alphabet_size = 0;
    for (int i = 0; i < 256; i++) {
        if (used[i])
            sym->alphabet_size++;
    }

    /* Determine bits_per_symbol */
    uint16_t total_symbols = sym->alphabet_size + TP_NUM_CONTROL_CODES;
    uint8_t bps = enc->opts.bits_per_symbol;
    if (bps == 0) {
        /* Auto: find minimum bits to cover all symbols */
        bps = 1;
        while (((uint16_t)1 << bps) < total_symbols)
            bps++;
    }
    sym->bits_per_symbol = bps;
    sym->symbol_count = total_symbols;

    /* Assign control codes to the first slots */
    for (int c = 0; c < TP_NUM_CONTROL_CODES; c++) {
        sym->ctrl_codes[c] = (uint32_t)c;
        sym->code_is_ctrl[c] = true;
    }

    /* Assign alphabet symbols starting after control codes */
    uint32_t code = TP_NUM_CONTROL_CODES;
    for (int i = 0; i < 256; i++) {
        if (used[i]) {
            sym->symbol_map[i] = code;
            if (code < 256)
                sym->reverse_map[code] = (uint8_t)i;
            code++;
        }
    }
}

/* ── Trie encoding ───────────────────────────────────────────────────── */

/* Helper: compute VarInt encoded size in bits */
static uint64_t varint_bits(uint64_t val)
{
    uint64_t bits = 0;
    do {
        bits += 8;
        val >>= 7;
    } while (val > 0);
    return bits;
}

/**
 * Compute the number of bits needed to encode the trie subtree for
 * entries[start..end) given that common_prefix characters have already been
 * consumed. This is the "dry run" pass used to compute SKIP distances.
 * value_idx is tracked so VarInt sizes for value indices are exact.
 */
static uint64_t trie_subtree_size(const tp_encoder *enc, uint32_t start, uint32_t end,
                                  size_t prefix_len, bool has_values, uint32_t *value_idx);

/**
 * Write the trie subtree for entries[start..end).
 */
static tp_result trie_write(const tp_encoder *enc, tp_bitstream_writer *w, uint32_t start,
                            uint32_t end, size_t prefix_len, bool has_values, uint32_t *value_idx);

static uint64_t trie_subtree_size(const tp_encoder *enc, uint32_t start, uint32_t end,
                                  size_t prefix_len, bool has_values, uint32_t *value_idx)
{
    uint8_t bps = enc->sym.bits_per_symbol;
    uint64_t bits = 0;

    /* Find common prefix beyond prefix_len */
    size_t common = prefix_len;
    while (true) {
        bool all_have = true;
        uint8_t ch = 0;
        for (uint32_t i = start; i < end; i++) {
            if (enc->entries[i].key_len <= common) {
                all_have = false;
                break;
            }
            if (i == start)
                ch = (uint8_t)enc->entries[i].key[common];
            else if ((uint8_t)enc->entries[i].key[common] != ch) {
                all_have = false;
                break;
            }
        }
        if (!all_have)
            break;
        common++;
    }

    /* Emit symbols for the common prefix part */
    bits += (uint64_t)(common - prefix_len) * bps;

    /* Check if any entry terminates exactly at 'common' */
    bool has_terminal = false;
    uint32_t terminal_idx = start;
    if (enc->entries[start].key_len == common) {
        has_terminal = true;
        terminal_idx = start;
    }

    /* Count children (entries whose keys continue past 'common') */
    uint32_t child_count = 0;
    uint32_t prev_start = has_terminal ? start + 1 : start;
    {
        uint32_t cs = prev_start;
        while (cs < end) {
            child_count++;
            uint8_t ch = (uint8_t)enc->entries[cs].key[common];
            uint32_t ce = cs + 1;
            while (ce < end && (uint8_t)enc->entries[ce].key[common] == ch)
                ce++;
            cs = ce;
        }
    }

    if (has_terminal && child_count == 0) {
        if (has_values && enc->entries[terminal_idx].val.type != TP_NULL) {
            bits += bps; /* END_VAL */
            bits += varint_bits(*value_idx);
        } else {
            bits += bps; /* END */
        }
        (*value_idx)++;
    } else if (has_terminal && child_count > 0) {
        if (has_values && enc->entries[terminal_idx].val.type != TP_NULL) {
            bits += bps; /* END_VAL */
            bits += varint_bits(*value_idx);
        } else {
            bits += bps; /* END */
        }
        (*value_idx)++;
        bits += bps; /* BRANCH */
        bits += varint_bits(child_count);
        uint32_t cs = prev_start;
        uint32_t child_i = 0;
        while (cs < end) {
            uint32_t ce = cs + 1;
            while (ce < end &&
                   (uint8_t)enc->entries[ce].key[common] == (uint8_t)enc->entries[cs].key[common])
                ce++;

            if (child_i < child_count - 1) {
                /* Need to compute child size first to know skip distance */
                uint32_t saved_vi = *value_idx;
                uint64_t child_sz = trie_subtree_size(enc, cs, ce, common, has_values, value_idx);
                bits += bps; /* SKIP */
                bits += varint_bits(child_sz);
                bits += child_sz;
                (void)saved_vi;
            } else {
                bits += trie_subtree_size(enc, cs, ce, common, has_values, value_idx);
            }
            child_i++;
            cs = ce;
        }
    } else {         /* !has_terminal, child_count > 1 */
        bits += bps; /* BRANCH */
        bits += varint_bits(child_count);
        uint32_t cs = prev_start;
        uint32_t child_i = 0;
        while (cs < end) {
            uint32_t ce = cs + 1;
            while (ce < end &&
                   (uint8_t)enc->entries[ce].key[common] == (uint8_t)enc->entries[cs].key[common])
                ce++;

            if (child_i < child_count - 1) {
                uint32_t saved_vi = *value_idx;
                uint64_t child_sz = trie_subtree_size(enc, cs, ce, common, has_values, value_idx);
                bits += bps; /* SKIP */
                bits += varint_bits(child_sz);
                bits += child_sz;
                (void)saved_vi;
            } else {
                bits += trie_subtree_size(enc, cs, ce, common, has_values, value_idx);
            }
            child_i++;
            cs = ce;
        }
    }

    return bits;
}

static tp_result trie_write(const tp_encoder *enc, tp_bitstream_writer *w, uint32_t start,
                            uint32_t end, size_t prefix_len, bool has_values, uint32_t *value_idx)
{
    uint8_t bps = enc->sym.bits_per_symbol;
    tp_result rc;

    /* Find common prefix beyond prefix_len */
    size_t common = prefix_len;
    while (true) {
        bool all_have = true;
        uint8_t ch = 0;
        for (uint32_t i = start; i < end; i++) {
            if (enc->entries[i].key_len <= common) {
                all_have = false;
                break;
            }
            if (i == start)
                ch = (uint8_t)enc->entries[i].key[common];
            else if ((uint8_t)enc->entries[i].key[common] != ch) {
                all_have = false;
                break;
            }
        }
        if (!all_have)
            break;
        common++;
    }

    /* Write common prefix symbols */
    for (size_t i = prefix_len; i < common; i++) {
        uint8_t ch = (uint8_t)enc->entries[start].key[i];
        uint32_t code = enc->sym.symbol_map[ch];
        rc = tp_bs_write_bits(w, code, bps);
        if (rc != TP_OK)
            return rc; /* LCOV_EXCL_LINE */
    }

    /* Check if any entry terminates exactly at 'common' */
    bool has_terminal = false;
    uint32_t terminal_entry = start;
    if (enc->entries[start].key_len == common) {
        has_terminal = true;
        terminal_entry = start;
    }

    /* Count and locate children */
    uint32_t child_count = 0;
    uint32_t children_start = has_terminal ? start + 1 : start;
    {
        uint32_t cs = children_start;
        while (cs < end) {
            child_count++;
            uint8_t ch = (uint8_t)enc->entries[cs].key[common];
            uint32_t ce = cs + 1;
            while (ce < end && (uint8_t)enc->entries[ce].key[common] == ch)
                ce++;
            cs = ce;
        }
    }

    /* Write terminal if present */
    if (has_terminal) {
        if (has_values && enc->entries[terminal_entry].val.type != TP_NULL) {
            rc = tp_bs_write_bits(w, enc->sym.ctrl_codes[TP_CTRL_END_VAL], bps);
            if (rc != TP_OK)
                return rc; /* LCOV_EXCL_LINE */
            rc = tp_bs_write_varint_u(w, *value_idx);
            if (rc != TP_OK)
                return rc; /* LCOV_EXCL_LINE */
        } else {
            rc = tp_bs_write_bits(w, enc->sym.ctrl_codes[TP_CTRL_END], bps);
            if (rc != TP_OK)
                return rc; /* LCOV_EXCL_LINE */
        }
        (*value_idx)++;
    }

    if (child_count == 0) {
        return TP_OK;
    }

    /* Branch */
    rc = tp_bs_write_bits(w, enc->sym.ctrl_codes[TP_CTRL_BRANCH], bps);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */
    rc = tp_bs_write_varint_u(w, child_count);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    /* For each child: optionally write SKIP, then recurse.
       Pass 'common' (not common+1) so each child writes its own
       distinguishing character as the first symbol — the decoder
       peeks at this symbol to decide which branch to follow. */
    uint32_t cs = children_start;
    uint32_t child_i = 0;
    while (cs < end) {
        uint32_t ce = cs + 1;
        while (ce < end &&
               (uint8_t)enc->entries[ce].key[common] == (uint8_t)enc->entries[cs].key[common])
            ce++;

        if (child_i < child_count - 1) {
            uint32_t vi_copy = *value_idx;
            uint64_t child_sz = trie_subtree_size(enc, cs, ce, common, has_values, &vi_copy);
            rc = tp_bs_write_bits(w, enc->sym.ctrl_codes[TP_CTRL_SKIP], bps);
            if (rc != TP_OK)
                return rc; /* LCOV_EXCL_LINE */
            rc = tp_bs_write_varint_u(w, child_sz);
            if (rc != TP_OK)
                return rc; /* LCOV_EXCL_LINE */
        }

        rc = trie_write(enc, w, cs, ce, common, has_values, value_idx);
        if (rc != TP_OK)
            return rc; /* LCOV_EXCL_LINE */

        child_i++;
        cs = ce;
    }

    return TP_OK;
}

/* ── Build ───────────────────────────────────────────────────────────── */

tp_result tp_encoder_build(tp_encoder *enc, uint8_t **buf, size_t *len)
{
    if (!enc || !buf || !len)
        return TP_ERR_INVALID_PARAM;

    /* Sort and deduplicate */
    sort_entries(enc);
    dedup_entries(enc);

    /* Determine if any entries have non-null values */
    bool has_values = false;
    for (uint32_t i = 0; i < enc->count; i++) {
        if (enc->entries[i].val.type != TP_NULL) {
            has_values = true;
            break;
        }
    }

    /* Analyze symbols and build maps */
    analyze_symbols(enc);

    /* Create writer */
    tp_bitstream_writer *w = NULL;
    tp_result rc = tp_bs_writer_create(&w, 256, 0);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    /* Write placeholder header (32 bytes) */
    tp_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic[0] = TP_MAGIC_0;
    hdr.magic[1] = TP_MAGIC_1;
    hdr.magic[2] = TP_MAGIC_2;
    hdr.magic[3] = TP_MAGIC_3;
    hdr.version_major = TP_FORMAT_VERSION_MAJOR;
    hdr.version_minor = TP_FORMAT_VERSION_MINOR;
    hdr.num_keys = enc->count;
    if (has_values)
        hdr.flags |= TP_FLAG_HAS_VALUES;

    rc = tp_header_write(w, &hdr);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_bs_writer_destroy(&w);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    /* Data stream starts at bit 256 (byte 32) */
    uint64_t data_start = tp_bs_writer_position(w);

    /* Write trie config: bits_per_symbol (4 bits) + symbol_count (8 bits) */
    rc = tp_bs_write_bits(w, enc->sym.bits_per_symbol, 4);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_bs_writer_destroy(&w);
        return rc;
        /* LCOV_EXCL_STOP */
    }
    rc = tp_bs_write_bits(w, enc->sym.symbol_count, 8);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_bs_writer_destroy(&w);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    /* Write special symbol map (6 * bps bits) */
    for (int c = 0; c < TP_NUM_CONTROL_CODES; c++) {
        rc = tp_bs_write_bits(w, enc->sym.ctrl_codes[c], enc->sym.bits_per_symbol);
        if (rc != TP_OK) {
            /* LCOV_EXCL_START */
            tp_bs_writer_destroy(&w);
            return rc;
            /* LCOV_EXCL_STOP */
        }
    }

    /* Write symbol table (VarInt codepoints for non-control symbols) */
    for (int i = 0; i < 256; i++) {
        /* Find byte values that are in the alphabet, in code order */
    }
    /* Actually: write in code order, skipping control codes */
    {
        uint32_t max_code = enc->sym.symbol_count;
        for (uint32_t code = TP_NUM_CONTROL_CODES; code < max_code; code++) {
            uint8_t byte_val = 0;
            if (code < 256)
                byte_val = enc->sym.reverse_map[code];
            rc = tp_bs_write_varint_u(w, byte_val);
            if (rc != TP_OK) {
                /* LCOV_EXCL_START */
                tp_bs_writer_destroy(&w);
                return rc;
                /* LCOV_EXCL_STOP */
            }
        }
    }

    /* Record trie data offset (relative to data stream start) */
    uint64_t trie_data_offset = tp_bs_writer_position(w) - data_start;

    /* Write prefix trie */
    uint32_t value_idx = 0;
    if (enc->count > 0) {
        rc = trie_write(enc, w, 0, enc->count, 0, has_values, &value_idx);
        if (rc != TP_OK) {
            /* LCOV_EXCL_START */
            tp_bs_writer_destroy(&w);
            return rc;
            /* LCOV_EXCL_STOP */
        }
    }

    /* Record value store offset */
    uint64_t value_store_offset = tp_bs_writer_position(w) - data_start;

    /* Write value store */
    if (has_values) {
        for (uint32_t i = 0; i < enc->count; i++) {
            rc = tp_value_encode(w, &enc->entries[i].val);
            if (rc != TP_OK) {
                /* LCOV_EXCL_START */
                tp_bs_writer_destroy(&w);
                return rc;
                /* LCOV_EXCL_STOP */
            }
        }
    }

    /* Total data bits */
    uint64_t total_data_bits = tp_bs_writer_position(w) - data_start;

    /* Align to byte boundary before CRC */
    rc = tp_bs_writer_align_to_byte(w);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_bs_writer_destroy(&w);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    /* Now patch the header with final offsets */
    /* We need to overwrite bytes 12-27 with the correct offsets */
    const uint8_t *wbuf;
    uint64_t wbit_len;
    tp_bs_writer_get_buffer(w, &wbuf, &wbit_len);

    /* Build the final buffer: first get current data */
    size_t pre_crc_bytes = (size_t)(tp_bs_writer_position(w) / 8);

    /* Compute CRC-32 over everything so far */
    uint32_t crc = tp_crc32(wbuf, pre_crc_bytes);

    /* Write CRC-32 as 4 bytes at the end */
    rc = tp_bs_write_u32(w, crc);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_bs_writer_destroy(&w);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    /* Detach buffer */
    uint8_t *out_buf;
    size_t out_byte_len;
    uint64_t out_bit_len;
    rc = tp_bs_writer_detach_buffer(w, &out_buf, &out_byte_len, &out_bit_len);
    tp_bs_writer_destroy(&w);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    /* Patch header fields in the output buffer */
    /* trie_data_offset at byte 12 (big-endian) */
    out_buf[12] = (uint8_t)(trie_data_offset >> 24);
    out_buf[13] = (uint8_t)(trie_data_offset >> 16);
    out_buf[14] = (uint8_t)(trie_data_offset >> 8);
    out_buf[15] = (uint8_t)(trie_data_offset);
    /* value_store_offset at byte 16 */
    out_buf[16] = (uint8_t)(value_store_offset >> 24);
    out_buf[17] = (uint8_t)(value_store_offset >> 16);
    out_buf[18] = (uint8_t)(value_store_offset >> 8);
    out_buf[19] = (uint8_t)(value_store_offset);
    /* suffix_table_offset at byte 20 = 0 (no suffix table) */
    out_buf[20] = 0;
    out_buf[21] = 0;
    out_buf[22] = 0;
    out_buf[23] = 0;
    /* total_data_bits at byte 24 */
    out_buf[24] = (uint8_t)(total_data_bits >> 24);
    out_buf[25] = (uint8_t)(total_data_bits >> 16);
    out_buf[26] = (uint8_t)(total_data_bits >> 8);
    out_buf[27] = (uint8_t)(total_data_bits);

    /* Recompute CRC over the patched data (everything except last 4 bytes) */
    size_t crc_data_len = out_byte_len - 4;
    crc = tp_crc32(out_buf, crc_data_len);
    out_buf[crc_data_len] = (uint8_t)(crc >> 24);
    out_buf[crc_data_len + 1] = (uint8_t)(crc >> 16);
    out_buf[crc_data_len + 2] = (uint8_t)(crc >> 8);
    out_buf[crc_data_len + 3] = (uint8_t)(crc);

    *buf = out_buf;
    *len = out_byte_len;
    return TP_OK;
}

/* ── Reset ───────────────────────────────────────────────────────────── */

tp_result tp_encoder_reset(tp_encoder *enc)
{
    if (!enc)
        return TP_ERR_INVALID_PARAM;
    for (uint32_t i = 0; i < enc->count; i++) {
        value_free_copy(&enc->entries[i].val);
        free(enc->entries[i].key);
    }
    free(enc->entries);
    enc->entries = NULL;
    enc->entries_cap = 0;
    enc->count = 0;
    enc->sorted = false;
    return TP_OK;
}
