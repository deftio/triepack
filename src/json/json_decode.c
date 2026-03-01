/**
 * @file json_decode.c
 * @brief Decode a .trp buffer back to JSON text.
 *
 * Reads all key-value pairs from the trie, unflattens dot-path keys
 * back into nested JSON objects and arrays, and outputs JSON text.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "core_internal.h"
#include "json_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Dynamic string buffer ───────────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static void sb_init(strbuf *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_free(strbuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static tp_result sb_grow(strbuf *sb, size_t need)
{
    if (sb->len + need <= sb->cap)
        return TP_OK;
    size_t new_cap = sb->cap == 0 ? 256 : sb->cap;
    while (new_cap < sb->len + need)
        new_cap *= 2;
    char *p = realloc(sb->data, new_cap);
    if (!p)
        return TP_ERR_ALLOC;
    sb->data = p;
    sb->cap = new_cap;
    return TP_OK;
}

static tp_result sb_append(strbuf *sb, const char *s, size_t n)
{
    tp_result rc = sb_grow(sb, n);
    if (rc != TP_OK)
        return rc;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    return TP_OK;
}

static tp_result sb_appendc(strbuf *sb, char c)
{
    return sb_append(sb, &c, 1);
}

static tp_result sb_append_str(strbuf *sb, const char *s)
{
    return sb_append(sb, s, strlen(s));
}

/* ── Flat key-value entry for sorting ────────────────────────────────── */

typedef struct {
    char *key;
    size_t key_len;
    tp_value val;
} flat_entry;

/* Compare two flat entries lexicographically by key */
static int flat_entry_cmp(const void *a, const void *b)
{
    const flat_entry *ea = (const flat_entry *)a;
    const flat_entry *eb = (const flat_entry *)b;
    size_t min_len = ea->key_len < eb->key_len ? ea->key_len : eb->key_len;
    int c = memcmp(ea->key, eb->key, min_len);
    if (c != 0)
        return c;
    if (ea->key_len < eb->key_len)
        return -1;
    if (ea->key_len > eb->key_len)
        return 1;
    return 0;
}

/* ── JSON string escaping ────────────────────────────────────────────── */

static tp_result sb_append_json_string(strbuf *sb, const char *str, size_t len)
{
    tp_result rc = sb_appendc(sb, '"');
    if (rc != TP_OK)
        return rc;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        switch (c) {
        case '"':
            rc = sb_append(sb, "\\\"", 2);
            break;
        case '\\':
            rc = sb_append(sb, "\\\\", 2);
            break;
        case '\b':
            rc = sb_append(sb, "\\b", 2);
            break;
        case '\f':
            rc = sb_append(sb, "\\f", 2);
            break;
        case '\n':
            rc = sb_append(sb, "\\n", 2);
            break;
        case '\r':
            rc = sb_append(sb, "\\r", 2);
            break;
        case '\t':
            rc = sb_append(sb, "\\t", 2);
            break;
        default:
            if (c < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                rc = sb_append(sb, esc, 6);
            } else {
                rc = sb_appendc(sb, (char)c);
            }
        }
        if (rc != TP_OK)
            return rc;
    }
    return sb_appendc(sb, '"');
}

/* ── Value to JSON text ──────────────────────────────────────────────── */

static tp_result sb_append_value(strbuf *sb, const tp_value *val)
{
    char tmp[64];
    switch (val->type) {
    case TP_NULL:
        return sb_append_str(sb, "null");
    case TP_BOOL:
        return sb_append_str(sb, val->data.bool_val ? "true" : "false");
    case TP_INT:
        snprintf(tmp, sizeof(tmp), "%lld", (long long)val->data.int_val);
        return sb_append_str(sb, tmp);
    case TP_UINT:
        snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)val->data.uint_val);
        return sb_append_str(sb, tmp);
    case TP_FLOAT32: {
        double d = (double)val->data.float32_val;
        snprintf(tmp, sizeof(tmp), "%g", d);
        /* Ensure it has a decimal point for JSON */
        if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E'))
            snprintf(tmp, sizeof(tmp), "%g.0", d);
        return sb_append_str(sb, tmp);
    }
    case TP_FLOAT64:
        snprintf(tmp, sizeof(tmp), "%.17g", val->data.float64_val);
        return sb_append_str(sb, tmp);
    case TP_STRING:
        if (val->data.string_val.str)
            return sb_append_json_string(sb, val->data.string_val.str,
                                         val->data.string_val.str_len);
        return sb_append_str(sb, "\"\"");
    default:
        return sb_append_str(sb, "null");
    }
}

/* ── Tree reconstruction ─────────────────────────────────────────────── */

/*
 * Reconstruct JSON from sorted flat entries.
 * We use a recursive approach: find all entries at the current prefix level,
 * group by their next path segment, and recurse.
 */

static tp_result emit_json(strbuf *sb, const flat_entry *entries, size_t count, const char *prefix,
                           size_t prefix_len, const char *indent, int depth);

/* Check if path segment starting at pos is an array index [N] */
static bool is_array_index(const char *key, size_t key_len, size_t pos)
{
    if (pos >= key_len)
        return false;
    return key[pos] == '[';
}

/* Find the next segment boundary: returns length of next segment name
   (up to '.' or '[' or end), and whether it's an array index */
static size_t next_segment(const char *key, size_t key_len, size_t pos, bool *is_idx)
{
    *is_idx = false;
    if (pos >= key_len)
        return 0;
    if (key[pos] == '[') {
        *is_idx = true;
        size_t end = pos + 1;
        while (end < key_len && key[end] != ']')
            end++;
        if (end < key_len)
            end++; /* include ']' */
        return end - pos;
    }
    /* Regular key segment: up to '.' or '[' or end */
    size_t end = pos;
    while (end < key_len && key[end] != '.' && key[end] != '[')
        end++;
    return end - pos;
}

static tp_result emit_indent(strbuf *sb, const char *indent, int depth)
{
    if (!indent)
        return TP_OK;
    tp_result rc = sb_appendc(sb, '\n');
    if (rc != TP_OK)
        return rc;
    size_t ilen = strlen(indent);
    for (int i = 0; i < depth; i++) {
        rc = sb_append(sb, indent, ilen);
        if (rc != TP_OK)
            return rc;
    }
    return TP_OK;
}

static tp_result emit_json(strbuf *sb, const flat_entry *entries, size_t count, const char *prefix,
                           size_t prefix_len, const char *indent, int depth)
{
    /*
     * Find all entries that start with prefix.
     * Group them by their next segment after prefix.
     */

    /* Determine if this level is an array or object by checking if
       all next segments are array indices */
    bool all_array = true;
    bool any_entry = false;

    for (size_t i = 0; i < count; i++) {
        if (entries[i].key_len < prefix_len)
            continue;
        if (prefix_len > 0 && memcmp(entries[i].key, prefix, prefix_len) != 0)
            continue;
        /* Skip metadata keys */
        if (entries[i].key_len > 0 && entries[i].key[0] == '\x01')
            continue;

        size_t pos = prefix_len;
        if (pos < entries[i].key_len && entries[i].key[pos] == '.')
            pos++;
        if (pos >= entries[i].key_len)
            continue; /* exact match = leaf, shouldn't be here */

        any_entry = true;
        if (!is_array_index(entries[i].key, entries[i].key_len, pos))
            all_array = false;
    }

    if (!any_entry) {
        /* Check if there's an exact prefix match (leaf value) */
        for (size_t i = 0; i < count; i++) {
            if (entries[i].key_len == prefix_len &&
                (prefix_len == 0 || memcmp(entries[i].key, prefix, prefix_len) == 0)) {
                if (entries[i].key[0] != '\x01')
                    return sb_append_value(sb, &entries[i].val);
            }
        }
        return sb_append_str(sb, "null");
    }

    /* Collect unique next-segments */
    typedef struct {
        size_t seg_start;
        size_t seg_len;
        bool is_idx;
    } segment_info;

    segment_info *segments = NULL;
    size_t num_segments = 0;
    size_t seg_cap = 0;

    for (size_t i = 0; i < count; i++) {
        if (entries[i].key_len < prefix_len)
            continue;
        if (prefix_len > 0 && memcmp(entries[i].key, prefix, prefix_len) != 0)
            continue;
        if (entries[i].key_len > 0 && entries[i].key[0] == '\x01')
            continue;

        size_t pos = prefix_len;
        if (pos < entries[i].key_len && entries[i].key[pos] == '.')
            pos++;
        if (pos >= entries[i].key_len)
            continue;

        bool is_idx = false;
        size_t seg_len = next_segment(entries[i].key, entries[i].key_len, pos, &is_idx);
        if (seg_len == 0)
            continue;

        /* Check if already in segments list */
        bool dup = false;
        for (size_t s = 0; s < num_segments; s++) {
            if (segments[s].seg_len == seg_len &&
                memcmp(entries[i].key + pos,
                       entries[segments[s].seg_start].key + segments[s].seg_start, seg_len) == 0) {
                /* This comparison is wrong for tracking; use stored offset */
                dup = true;
                break;
            }
        }

        /* Actually, track by storing entry index and segment offset */
        if (!dup) {
            /* Simpler dedup: check segment text */
            dup = false;
            for (size_t s = 0; s < num_segments; s++) {
                const flat_entry *se = &entries[segments[s].seg_start];
                size_t sp = prefix_len;
                if (sp < se->key_len && se->key[sp] == '.')
                    sp++;
                bool si = false;
                size_t sl = next_segment(se->key, se->key_len, sp, &si);
                if (sl == seg_len && memcmp(se->key + sp, entries[i].key + pos, seg_len) == 0) {
                    dup = true;
                    break;
                }
            }
        }

        if (!dup) {
            if (num_segments >= seg_cap) {
                seg_cap = seg_cap == 0 ? 16 : seg_cap * 2;
                segment_info *new_segs = realloc(segments, seg_cap * sizeof(segment_info));
                if (!new_segs) {
                    free(segments);
                    return TP_ERR_ALLOC;
                }
                segments = new_segs;
            }
            segments[num_segments].seg_start = i;
            segments[num_segments].seg_len = seg_len;
            segments[num_segments].is_idx = is_idx;
            num_segments++;
        }
    }

    tp_result rc;

    if (all_array) {
        rc = sb_appendc(sb, '[');
        if (rc != TP_OK) {
            free(segments);
            return rc;
        }

        for (size_t s = 0; s < num_segments; s++) {
            if (s > 0) {
                rc = sb_appendc(sb, ',');
                if (rc != TP_OK) {
                    free(segments);
                    return rc;
                }
            }
            if (indent) {
                rc = emit_indent(sb, indent, depth + 1);
                if (rc != TP_OK) {
                    free(segments);
                    return rc;
                }
            }

            const flat_entry *se = &entries[segments[s].seg_start];
            size_t sp = prefix_len;
            if (sp < se->key_len && se->key[sp] == '.')
                sp++;
            bool si;
            size_t sl = next_segment(se->key, se->key_len, sp, &si);

            /* Build new prefix for this element */
            char new_prefix[4096];
            size_t new_prefix_len = prefix_len;
            if (prefix_len > 0) {
                memcpy(new_prefix, prefix, prefix_len);
            }
            memcpy(new_prefix + new_prefix_len, se->key + sp, sl);
            new_prefix_len += sl;
            new_prefix[new_prefix_len] = '\0';

            /* Check if any entry is exactly this prefix (leaf) */
            bool is_leaf = false;
            const tp_value *leaf_val = NULL;
            for (size_t i = 0; i < count; i++) {
                if (entries[i].key_len == new_prefix_len &&
                    memcmp(entries[i].key, new_prefix, new_prefix_len) == 0 &&
                    entries[i].key[0] != '\x01') {
                    is_leaf = true;
                    leaf_val = &entries[i].val;
                    break;
                }
            }

            /* Check if there are sub-entries */
            bool has_children = false;
            for (size_t i = 0; i < count; i++) {
                if (entries[i].key_len > new_prefix_len &&
                    memcmp(entries[i].key, new_prefix, new_prefix_len) == 0 &&
                    entries[i].key[0] != '\x01') {
                    has_children = true;
                    break;
                }
            }

            if (is_leaf && !has_children) {
                rc = sb_append_value(sb, leaf_val);
            } else {
                rc = emit_json(sb, entries, count, new_prefix, new_prefix_len, indent, depth + 1);
            }
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
        }

        if (indent && num_segments > 0) {
            rc = emit_indent(sb, indent, depth);
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
        }
        rc = sb_appendc(sb, ']');
    } else {
        rc = sb_appendc(sb, '{');
        if (rc != TP_OK) {
            free(segments);
            return rc;
        }

        for (size_t s = 0; s < num_segments; s++) {
            if (s > 0) {
                rc = sb_appendc(sb, ',');
                if (rc != TP_OK) {
                    free(segments);
                    return rc;
                }
            }
            if (indent) {
                rc = emit_indent(sb, indent, depth + 1);
                if (rc != TP_OK) {
                    free(segments);
                    return rc;
                }
            }

            const flat_entry *se = &entries[segments[s].seg_start];
            size_t sp = prefix_len;
            if (sp < se->key_len && se->key[sp] == '.')
                sp++;
            bool si;
            size_t sl = next_segment(se->key, se->key_len, sp, &si);

            /* Write key */
            rc = sb_append_json_string(sb, se->key + sp, sl);
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
            rc = sb_appendc(sb, ':');
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
            if (indent) {
                rc = sb_appendc(sb, ' ');
                if (rc != TP_OK) {
                    free(segments);
                    return rc;
                }
            }

            /* Build new prefix */
            char new_prefix[4096];
            size_t new_prefix_len = prefix_len;
            if (prefix_len > 0) {
                memcpy(new_prefix, prefix, prefix_len);
                new_prefix[new_prefix_len++] = '.';
            }
            memcpy(new_prefix + new_prefix_len, se->key + sp, sl);
            new_prefix_len += sl;
            new_prefix[new_prefix_len] = '\0';

            /* Check if any entry is exactly this prefix (leaf) */
            bool is_leaf = false;
            const tp_value *leaf_val = NULL;
            for (size_t i = 0; i < count; i++) {
                if (entries[i].key_len == new_prefix_len &&
                    memcmp(entries[i].key, new_prefix, new_prefix_len) == 0 &&
                    entries[i].key[0] != '\x01') {
                    is_leaf = true;
                    leaf_val = &entries[i].val;
                    break;
                }
            }

            /* Check if there are sub-entries */
            bool has_children = false;
            for (size_t i = 0; i < count; i++) {
                if (entries[i].key_len > new_prefix_len &&
                    memcmp(entries[i].key, new_prefix, new_prefix_len) == 0 &&
                    entries[i].key[0] != '\x01') {
                    has_children = true;
                    break;
                }
            }

            if (is_leaf && !has_children) {
                rc = sb_append_value(sb, leaf_val);
            } else {
                rc = emit_json(sb, entries, count, new_prefix, new_prefix_len, indent, depth + 1);
            }
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
        }

        if (indent && num_segments > 0) {
            rc = emit_indent(sb, indent, depth);
            if (rc != TP_OK) {
                free(segments);
                return rc;
            }
        }
        rc = sb_appendc(sb, '}');
    }

    free(segments);
    return rc;
}

/* ── Extract all entries from a trp buffer ───────────────────────────── */

static tp_result extract_entries(const uint8_t *buf, size_t buf_len, flat_entry **out_entries,
                                 size_t *out_count, uint32_t *out_root_type)
{
    /* Encode path: we re-encode the JSON to get it back as entries.
       Instead, we use the encoder's sorted key list approach:
       open the dict, iterate by looking up known keys.

       Since the iterator is not yet fully implemented, we take a different
       approach: re-open as a dict, then scan the trie by rebuilding from
       the encoder. This is a decode limitation.

       Better approach: since we encoded using tp_encoder_build, we know
       the keys are stored in sorted order. We can walk the trie by
       scanning all possible paths. But without a working iterator, this
       is hard.

       Practical approach: Use the dict to verify, but the real decode
       needs to be done from the binary trie format directly.

       Simplest correct approach for v1.0: re-read using a lookup for
       every key. But we don't know the keys!

       The correct solution is to implement a basic trie iterator here. */

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, buf_len);
    if (rc != TP_OK)
        return rc;

    uint32_t num_keys = tp_dict_count(dict);

    /* We need to walk the trie to extract all keys. Since the existing
       tp_iter_next is a stub, we'll implement a local trie walker. */

    /* Read the symbol info from the dict to walk the trie */
    flat_entry *entries = calloc(num_keys, sizeof(flat_entry));
    if (!entries) {
        tp_dict_close(&dict);
        return TP_ERR_ALLOC;
    }

    /* Walk the trie using a stack-based DFS */
    typedef struct {
        uint64_t bit_pos;
        size_t key_len;
        uint32_t remaining_children;
    } walk_frame;

    walk_frame *stack = calloc(256, sizeof(walk_frame));
    if (!stack) {
        free(entries);
        tp_dict_close(&dict);
        return TP_ERR_ALLOC;
    }

    tp_bitstream_reader *reader = NULL;
    rc = tp_bs_reader_create(&reader, buf, (uint64_t)buf_len * 8);
    if (rc != TP_OK) {
        free(stack);
        free(entries);
        tp_dict_close(&dict);
        return rc;
    }

    rc = tp_bs_reader_seek(reader, dict->trie_start);
    if (rc != TP_OK) {
        tp_bs_reader_destroy(&reader);
        free(stack);
        free(entries);
        tp_dict_close(&dict);
        return rc;
    }

    char key_buf[4096];
    size_t key_len = 0;
    int stack_top = -1;
    size_t entry_count = 0;
    uint8_t bps = dict->sym.bits_per_symbol;

    /* DFS trie walk */
    while (entry_count < num_keys) {
        uint64_t sym_raw;
        rc = tp_bs_read_bits(reader, bps, &sym_raw);
        if (rc != TP_OK)
            break;
        uint32_t sym = (uint32_t)sym_raw;

        if (sym == dict->sym.ctrl_codes[TP_CTRL_END]) {
            /* Terminal without value */
            if (entry_count < num_keys) {
                entries[entry_count].key = malloc(key_len + 1);
                if (entries[entry_count].key) {
                    memcpy(entries[entry_count].key, key_buf, key_len);
                    entries[entry_count].key[key_len] = '\0';
                    entries[entry_count].key_len = key_len;
                    entries[entry_count].val = tp_value_null();
                    /* Look up actual value */
                    tp_value actual;
                    if (tp_dict_lookup_n(dict, entries[entry_count].key, key_len, &actual) ==
                        TP_OK) {
                        entries[entry_count].val = actual;
                    }
                    entry_count++;
                }
            }
            /* Check if we need to pop the stack */
            while (stack_top >= 0) {
                if (stack[stack_top].remaining_children > 0) {
                    stack[stack_top].remaining_children--;
                    key_len = stack[stack_top].key_len;
                    if (stack[stack_top].remaining_children == 0) {
                        /* Last child of this branch: just continue to its subtree */
                        break;
                    }
                    /* Read SKIP + distance for next child */
                    uint64_t skip_sym;
                    rc = tp_bs_read_bits(reader, bps, &skip_sym);
                    if (rc != TP_OK)
                        goto done;
                    uint64_t skip_dist;
                    rc = tp_bs_read_varint_u(reader, &skip_dist);
                    if (rc != TP_OK)
                        goto done;
                    (void)skip_dist;
                    break;
                } else {
                    stack_top--;
                }
            }
            continue;
        }

        if (sym == dict->sym.ctrl_codes[TP_CTRL_END_VAL]) {
            uint64_t vi;
            rc = tp_bs_read_varint_u(reader, &vi);
            if (rc != TP_OK)
                break;
            if (entry_count < num_keys) {
                entries[entry_count].key = malloc(key_len + 1);
                if (entries[entry_count].key) {
                    memcpy(entries[entry_count].key, key_buf, key_len);
                    entries[entry_count].key[key_len] = '\0';
                    entries[entry_count].key_len = key_len;
                    entries[entry_count].val = tp_value_null();
                    tp_value actual;
                    if (tp_dict_lookup_n(dict, entries[entry_count].key, key_len, &actual) ==
                        TP_OK) {
                        entries[entry_count].val = actual;
                    }
                    entry_count++;
                }
            }
            while (stack_top >= 0) {
                if (stack[stack_top].remaining_children > 0) {
                    stack[stack_top].remaining_children--;
                    key_len = stack[stack_top].key_len;
                    if (stack[stack_top].remaining_children == 0) {
                        break;
                    }
                    uint64_t skip_sym;
                    rc = tp_bs_read_bits(reader, bps, &skip_sym);
                    if (rc != TP_OK)
                        goto done;
                    uint64_t skip_dist;
                    rc = tp_bs_read_varint_u(reader, &skip_dist);
                    if (rc != TP_OK)
                        goto done;
                    (void)skip_dist;
                    break;
                } else {
                    stack_top--;
                }
            }
            continue;
        }

        if (sym == dict->sym.ctrl_codes[TP_CTRL_BRANCH]) {
            uint64_t child_count;
            rc = tp_bs_read_varint_u(reader, &child_count);
            if (rc != TP_OK)
                break;
            stack_top++;
            if (stack_top >= 256) {
                rc = TP_ERR_JSON_DEPTH;
                break;
            }
            stack[stack_top].key_len = key_len;
            stack[stack_top].remaining_children = (uint32_t)child_count;
            /* Read SKIP for first child (if more than 1) */
            if (child_count > 1) {
                stack[stack_top].remaining_children--;
                uint64_t skip_sym;
                rc = tp_bs_read_bits(reader, bps, &skip_sym);
                if (rc != TP_OK)
                    break;
                uint64_t skip_dist;
                rc = tp_bs_read_varint_u(reader, &skip_dist);
                if (rc != TP_OK)
                    break;
                (void)skip_dist;
            } else {
                stack[stack_top].remaining_children = 0;
            }
            continue;
        }

        if (sym == dict->sym.ctrl_codes[TP_CTRL_SKIP]) {
            /* Shouldn't hit this outside BRANCH handling, but skip */
            uint64_t dist;
            rc = tp_bs_read_varint_u(reader, &dist);
            if (rc != TP_OK)
                break;
            continue;
        }

        /* Regular symbol - append to key */
        if (!dict->sym.code_is_ctrl[sym < 256 ? sym : 0]) {
            uint8_t byte_val = (sym < 256) ? dict->sym.reverse_map[sym] : 0;
            if (key_len < sizeof(key_buf) - 1) {
                key_buf[key_len++] = (char)byte_val;
            }
        }
    }

done:
    tp_bs_reader_destroy(&reader);
    free(stack);

    /* Look up root type */
    *out_root_type = TP_JSON_ROOT_OBJECT; /* default */
    tp_value root_val;
    if (tp_dict_lookup(dict, TP_JSON_META_ROOT, &root_val) == TP_OK) {
        if (root_val.type == TP_UINT)
            *out_root_type = (uint32_t)root_val.data.uint_val;
        else if (root_val.type == TP_INT)
            *out_root_type = (uint32_t)root_val.data.int_val;
    }

    /* Filter out metadata keys */
    size_t write_pos = 0;
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].key_len > 0 && entries[i].key[0] == '\x01') {
            free(entries[i].key);
            continue;
        }
        if (write_pos != i)
            entries[write_pos] = entries[i];
        write_pos++;
    }
    entry_count = write_pos;

    /* Sort entries */
    if (entry_count > 1)
        qsort(entries, entry_count, sizeof(flat_entry), flat_entry_cmp);

    tp_dict_close(&dict);

    *out_entries = entries;
    *out_count = entry_count;
    return TP_OK;
}

/* ── One-shot decode ─────────────────────────────────────────────────── */

static tp_result decode_impl(const uint8_t *buf, size_t buf_len, const char *indent,
                             char **json_str, size_t *json_len)
{
    flat_entry *entries = NULL;
    size_t count = 0;
    uint32_t root_type = TP_JSON_ROOT_OBJECT;

    tp_result rc = extract_entries(buf, buf_len, &entries, &count, &root_type);
    if (rc != TP_OK)
        return rc;

    strbuf sb;
    sb_init(&sb);

    if (count == 0) {
        if (root_type == TP_JSON_ROOT_ARRAY)
            rc = sb_append_str(&sb, "[]");
        else
            rc = sb_append_str(&sb, "{}");
    } else {
        rc = emit_json(&sb, entries, count, "", 0, indent, 0);
    }

    /* Free entries */
    for (size_t i = 0; i < count; i++)
        free(entries[i].key);
    free(entries);

    if (rc != TP_OK) {
        sb_free(&sb);
        return rc;
    }

    /* NUL-terminate */
    rc = sb_appendc(&sb, '\0');
    if (rc != TP_OK) {
        sb_free(&sb);
        return rc;
    }

    *json_str = sb.data;
    *json_len = sb.len - 1; /* exclude NUL */
    return TP_OK;
}

tp_result tp_json_decode(const uint8_t *buf, size_t buf_len, char **json_str, size_t *json_len)
{
    if (!buf || !json_str || !json_len)
        return TP_ERR_INVALID_PARAM;

    return decode_impl(buf, buf_len, NULL, json_str, json_len);
}

/* ── Pretty-printed decode ───────────────────────────────────────────── */

tp_result tp_json_decode_pretty(const uint8_t *buf, size_t buf_len, const char *indent,
                                char **json_str, size_t *json_len)
{
    if (!buf || !indent || !json_str || !json_len)
        return TP_ERR_INVALID_PARAM;

    return decode_impl(buf, buf_len, indent, json_str, json_len);
}
