/**
 * @file decoder.c
 * @brief Dictionary lifecycle: open compiled .trp buffer, lookup keys.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"

/* ── Internal: parse trie config and symbol table ───────────────────── */

static tp_result parse_trie_config(tp_bitstream_reader *r, tp_dict *dict)
{
    uint64_t val;
    tp_result rc;

    /* bits_per_symbol (4 bits) */
    rc = tp_bs_read_bits(r, 4, &val);
    if (rc != TP_OK)
        return rc;
    dict->sym.bits_per_symbol = (uint8_t)val;
    dict->info.bits_per_symbol = (uint8_t)val;

    /* symbol_count (8 bits) */
    rc = tp_bs_read_bits(r, 8, &val);
    if (rc != TP_OK)
        return rc;
    dict->sym.symbol_count = (uint16_t)val;

    /* special_symbol_map: 6 control codes */
    memset(dict->sym.code_is_ctrl, 0, sizeof(dict->sym.code_is_ctrl));
    for (int c = 0; c < TP_NUM_CONTROL_CODES; c++) {
        rc = tp_bs_read_bits(r, dict->sym.bits_per_symbol, &val);
        if (rc != TP_OK)
            return rc;
        dict->sym.ctrl_codes[c] = (uint32_t)val;
        if (val < 256)
            dict->sym.code_is_ctrl[val] = true;
    }

    /* Read symbol table */
    memset(dict->sym.reverse_map, 0, sizeof(dict->sym.reverse_map));
    memset(dict->sym.symbol_map, 0, sizeof(dict->sym.symbol_map));
    uint32_t max_code = dict->sym.symbol_count;
    for (uint32_t code = TP_NUM_CONTROL_CODES; code < max_code; code++) {
        uint64_t cp;
        rc = tp_bs_read_varint_u(r, &cp);
        if (rc != TP_OK)
            return rc;
        if (code < 256 && cp < 256) {
            dict->sym.reverse_map[code] = (uint8_t)cp;
            dict->sym.symbol_map[cp] = code;
        }
    }

    return TP_OK;
}

/* ── Open / Close ────────────────────────────────────────────────────── */

static tp_result dict_open_impl(tp_dict **out, const uint8_t *buf, size_t len, bool check_crc)
{
    if (!out || !buf)
        return TP_ERR_INVALID_PARAM;
    if (len < TP_HEADER_SIZE)
        return TP_ERR_TRUNCATED;

    /* Parse header */
    /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
    tp_bitstream_reader *r = NULL;
    tp_result rc = tp_bs_reader_create(&r, buf, (uint64_t)len * 8);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    tp_header hdr;
    rc = tp_header_read(r, &hdr);
    if (rc != TP_OK) {
        tp_bs_reader_destroy(&r);
        return rc;
    }

    /* CRC verification */
    if (check_crc && len >= 4) {
        size_t crc_data_len = len - 4;
        uint32_t expected_crc =
            ((uint32_t)buf[crc_data_len] << 24) | ((uint32_t)buf[crc_data_len + 1] << 16) |
            ((uint32_t)buf[crc_data_len + 2] << 8) | ((uint32_t)buf[crc_data_len + 3]);
        uint32_t actual_crc = tp_crc32(buf, crc_data_len);
        if (actual_crc != expected_crc) {
            tp_bs_reader_destroy(&r);
            return TP_ERR_CORRUPT;
        }
    }

    /* Check for unsupported features */
    if (hdr.flags & TP_FLAG_HAS_SUFFIX_TABLE) {
        tp_bs_reader_destroy(&r);
        return TP_ERR_VERSION;
    }

    /* Allocate dict */
    tp_dict *dict = calloc(1, sizeof(*dict));
    if (!dict) {
        /* LCOV_EXCL_START */
        tp_bs_reader_destroy(&r);
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }

    dict->buf = buf;
    dict->len = len;
    dict->hdr = hdr;

    /* Populate info */
    dict->info.format_version_major = hdr.version_major;
    dict->info.format_version_minor = hdr.version_minor;
    dict->info.num_keys = hdr.num_keys;
    dict->info.has_values = (hdr.flags & TP_FLAG_HAS_VALUES) != 0;
    dict->info.has_suffix_table = false;
    dict->info.compact_mode = (hdr.flags & TP_FLAG_COMPACT_MODE) != 0;
    dict->info.total_bytes = len;
    dict->info.checksum_type = TP_CHECKSUM_CRC32;
    dict->info.trie_mode = TP_ADDR_BIT;
    dict->info.value_mode = TP_ADDR_BYTE;

    /* Parse trie config and symbol table */
    /* Reader is at bit 256 (byte 32, start of data stream) */
    rc = parse_trie_config(r, dict);
    if (rc != TP_OK) {
        tp_bs_reader_destroy(&r);
        free(dict);
        return rc;
    }

    uint64_t data_start = (uint64_t)TP_HEADER_SIZE * 8;
    dict->trie_start = data_start + hdr.trie_data_offset;
    dict->value_start = data_start + hdr.value_store_offset;

    tp_bs_reader_destroy(&r);
    *out = dict;
    return TP_OK;
}

tp_result tp_dict_open(tp_dict **out, const uint8_t *buf, size_t len)
{
    return dict_open_impl(out, buf, len, true);
}

tp_result tp_dict_open_unchecked(tp_dict **out, const uint8_t *buf, size_t len)
{
    return dict_open_impl(out, buf, len, false);
}

tp_result tp_dict_close(tp_dict **dict)
{
    if (!dict)
        return TP_ERR_INVALID_PARAM;
    free(*dict);
    *dict = NULL;
    return TP_OK;
}

/* ── Query ───────────────────────────────────────────────────────────── */

uint32_t tp_dict_count(const tp_dict *dict)
{
    if (!dict)
        return 0;
    return dict->info.num_keys;
}

/* ── Lookup ──────────────────────────────────────────────────────────── */

tp_result tp_dict_lookup(const tp_dict *dict, const char *key, tp_value *val)
{
    if (!dict || !key)
        return TP_ERR_INVALID_PARAM;
    return tp_dict_lookup_n(dict, key, strlen(key), val);
}

tp_result tp_dict_lookup_n(const tp_dict *dict, const char *key, size_t key_len, tp_value *val)
{
    if (!dict || !key)
        return TP_ERR_INVALID_PARAM;

    /* Empty dictionary: nothing to find */
    if (dict->info.num_keys == 0)
        return TP_ERR_NOT_FOUND;

    tp_bitstream_reader *r = NULL;
    tp_result rc = tp_bs_reader_create(&r, dict->buf, (uint64_t)dict->len * 8);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    rc = tp_bs_reader_seek(r, dict->trie_start);
    if (rc != TP_OK) {
        tp_bs_reader_destroy(&r);
        return rc; /* LCOV_EXCL_LINE */
    }

    uint8_t bps = dict->sym.bits_per_symbol;
    size_t key_idx = 0;
    uint32_t value_index = 0;
    bool found_value = false;
    bool expect_branch = false; /* set after END/END_VAL when key not consumed */

    while (true) {
        uint64_t sym_raw;
        rc = tp_bs_read_bits(r, bps, &sym_raw);
        if (rc != TP_OK) {
            tp_bs_reader_destroy(&r);
            /* If we were expecting a BRANCH after a terminal but hit
               EOF, the key doesn't exist. */
            if (expect_branch)
                return TP_ERR_NOT_FOUND;
            return rc;
        }
        uint32_t sym = (uint32_t)sym_raw;

        if (sym == dict->sym.ctrl_codes[TP_CTRL_END]) {
            if (key_idx == key_len) {
                tp_bs_reader_destroy(&r);
                if (val)
                    *val = tp_value_null();
                return TP_OK;
            }
            /* Key not fully consumed: a BRANCH may follow this terminal
               for children sharing this prefix. */
            expect_branch = true;
            continue;
        }

        if (sym == dict->sym.ctrl_codes[TP_CTRL_END_VAL]) {
            uint64_t vi;
            rc = tp_bs_read_varint_u(r, &vi);
            if (rc != TP_OK) {
                tp_bs_reader_destroy(&r);
                return rc;
            }
            if (key_idx == key_len) {
                value_index = (uint32_t)vi;
                found_value = true;
                break;
            }
            /* Key not fully consumed: a BRANCH may follow. */
            expect_branch = true;
            continue;
        }

        if (sym == dict->sym.ctrl_codes[TP_CTRL_BRANCH]) {
            expect_branch = false;
            uint64_t child_count;
            rc = tp_bs_read_varint_u(r, &child_count);
            if (rc != TP_OK) {
                tp_bs_reader_destroy(&r);
                return rc;
            }

            if (key_idx >= key_len) {
                tp_bs_reader_destroy(&r);
                return TP_ERR_NOT_FOUND;
            }

            uint8_t target = (uint8_t)key[key_idx];
            bool found_child = false;

            for (uint64_t ci = 0; ci < child_count; ci++) {
                bool has_skip = (ci < child_count - 1);
                uint64_t skip_dist = 0;

                if (has_skip) {
                    /* Read SKIP control code */
                    uint64_t skip_sym;
                    rc = tp_bs_read_bits(r, bps, &skip_sym);
                    if (rc != TP_OK) {
                        tp_bs_reader_destroy(&r);
                        return rc;
                    }
                    /* Read skip distance */
                    rc = tp_bs_read_varint_u(r, &skip_dist);
                    if (rc != TP_OK) {
                        tp_bs_reader_destroy(&r);
                        return rc;
                    }
                }

                /* Peek at first symbol of this child */
                uint64_t child_first_sym;
                rc = tp_bs_peek_bits(r, bps, &child_first_sym);
                if (rc != TP_OK) {
                    tp_bs_reader_destroy(&r);
                    return rc;
                }

                /* Determine if this is a regular symbol */
                uint32_t csym = (uint32_t)child_first_sym;
                bool is_ctrl = (csym < 256) && dict->sym.code_is_ctrl[csym];

                if (!is_ctrl) {
                    /* It's a regular symbol; check if it matches */
                    uint32_t expected_code = dict->sym.symbol_map[target];
                    if (csym == expected_code) {
                        /* Match! Consume this symbol and continue */
                        tp_bs_reader_advance(r, bps);
                        key_idx++;
                        found_child = true;
                        break;
                    }
                }

                /* Check if it's an END/END_VAL (terminal with no more key
                   chars). These can only match if target matches nothing,
                   so skip to next sibling. */

                if (has_skip) {
                    /* Skip to next sibling */
                    rc = tp_bs_reader_advance(r, skip_dist);
                    if (rc != TP_OK) {
                        tp_bs_reader_destroy(&r);
                        return rc;
                    }
                }
            }

            if (!found_child) {
                tp_bs_reader_destroy(&r);
                return TP_ERR_NOT_FOUND;
            }
            continue;
        }

        /* If we expected a BRANCH after a terminal but got something
           else, the key extends beyond a leaf node — not found. */
        if (expect_branch) {
            tp_bs_reader_destroy(&r);
            return TP_ERR_NOT_FOUND;
        }

        /* Regular symbol */
        if (!dict->sym.code_is_ctrl[sym < 256 ? sym : 0]) {
            if (key_idx >= key_len) {
                tp_bs_reader_destroy(&r);
                return TP_ERR_NOT_FOUND;
            }
            uint32_t expected_code = dict->sym.symbol_map[(uint8_t)key[key_idx]];
            if (sym != expected_code) {
                tp_bs_reader_destroy(&r);
                return TP_ERR_NOT_FOUND;
            }
            key_idx++;
            continue;
        }

        /* Unknown control code */
        tp_bs_reader_destroy(&r);
        return TP_ERR_INVALID_PARAM;
    }

    /* Decode value if found */
    if (found_value && val && dict->info.has_values) {
        /* Seek to value store and skip to the right value */
        rc = tp_bs_reader_seek(r, dict->value_start);
        if (rc != TP_OK) {
            tp_bs_reader_destroy(&r);
            return rc;
        }

        /* Skip value_index values */
        for (uint32_t i = 0; i < value_index; i++) {
            tp_value tmp;
            rc = tp_value_decode(r, &tmp, dict->buf);
            if (rc != TP_OK) {
                tp_bs_reader_destroy(&r);
                return rc;
            }
        }

        rc = tp_value_decode(r, val, dict->buf);
        tp_bs_reader_destroy(&r);
        return rc;
    }

    /* LCOV_EXCL_START — found_value is only set via CTRL_END_VAL which
       implies has_values, so the condition above always takes priority. */
    if (found_value && val) {
        *val = tp_value_null();
    }
    /* LCOV_EXCL_STOP */

    tp_bs_reader_destroy(&r);
    return TP_OK;
}

tp_result tp_dict_contains(const tp_dict *dict, const char *key, bool *out)
{
    if (!dict || !key || !out)
        return TP_ERR_INVALID_PARAM;

    tp_value val;
    tp_result rc = tp_dict_lookup(dict, key, &val);
    if (rc == TP_OK) {
        *out = true;
        return TP_OK;
    }
    if (rc == TP_ERR_NOT_FOUND) {
        *out = false;
        return TP_OK;
    }
    return rc;
}

/* ── Info ────────────────────────────────────────────────────────────── */

tp_result tp_dict_get_info(const tp_dict *dict, tp_dict_info *info)
{
    if (!dict || !info)
        return TP_ERR_INVALID_PARAM;
    *info = dict->info;
    return TP_OK;
}

/* ── Iteration ──────────────────────────────────────────────────────── */

tp_result tp_dict_iterate(const tp_dict *dict, tp_iterator **out)
{
    if (!dict || !out)
        return TP_ERR_INVALID_PARAM;

    tp_iterator *it = calloc(1, sizeof(*it));
    if (!it)
        return TP_ERR_ALLOC; /* LCOV_EXCL_LINE */

    it->dict = dict;
    it->pos = dict->trie_start;
    it->done = false;
    it->key_buf = malloc(256);
    if (!it->key_buf) {
        /* LCOV_EXCL_START */
        free(it);
        return TP_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
    }
    it->key_buf_cap = 256;
    it->key_len = 0;
    it->stack_top = -1;
    it->started = false;
    *out = it;
    return TP_OK;
}

tp_result tp_iter_next(tp_iterator *it, const char **key, size_t *key_len, tp_value *val)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;

    if (it->done)
        return TP_ERR_EOF;

    /* Simple iteration: scan through all keys using sequential lookup */
    /* For v0.1, we just do a linear scan of sorted entries from the encoder.
       A proper implementation would walk the trie directly. Since we don't
       store the sorted entry list in the dict, we return EOF for now. */
    it->done = true;
    (void)key;
    (void)key_len;
    (void)val;
    return TP_ERR_EOF;
}

tp_result tp_iter_reset(tp_iterator *it)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;
    it->pos = it->dict->trie_start;
    it->done = false;
    it->key_len = 0;
    it->stack_top = -1;
    it->started = false;
    return TP_OK;
}

tp_result tp_iter_destroy(tp_iterator **it)
{
    if (!it)
        return TP_ERR_INVALID_PARAM;
    if (*it) {
        free((*it)->key_buf);
        free(*it);
        *it = NULL;
    }
    return TP_OK;
}

/* ── Search ─────────────────────────────────────────────────────────── */

tp_result tp_dict_find_prefix(const tp_dict *dict, const char *prefix, tp_iterator **out)
{
    if (!dict || !prefix || !out)
        return TP_ERR_INVALID_PARAM;
    return tp_dict_iterate(dict, out);
}

tp_result tp_dict_find_fuzzy(const tp_dict *dict, const char *query, uint8_t max_dist,
                             tp_iterator **out)
{
    if (!dict || !query || !out)
        return TP_ERR_INVALID_PARAM;
    (void)max_dist;
    return tp_dict_iterate(dict, out);
}

tp_result tp_iter_get_distance(const tp_iterator *it, uint8_t *dist)
{
    if (!it || !dist)
        return TP_ERR_INVALID_PARAM;
    *dist = it->distance;
    return TP_OK;
}
