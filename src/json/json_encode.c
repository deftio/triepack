/**
 * @file json_encode.c
 * @brief Encode a JSON string into a compressed .trp buffer.
 *
 * Minimal recursive-descent parser that flattens JSON into dot-path keys.
 * Objects: {"a":{"b":1}} -> key "a.b", value int(1)
 * Arrays:  {"x":[10,20]} -> keys "x[0]", "x[1]"
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "json_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Parser state ────────────────────────────────────────────────────── */

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int depth;
    tp_encoder *enc;
    char path[4096]; /* current dot-path buffer */
    size_t path_len;
} json_parser;

/* ── Whitespace / peek helpers ───────────────────────────────────────── */

static void skip_ws(json_parser *p)
{
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            p->pos++;
        else
            break;
    }
}

static char peek(json_parser *p)
{
    skip_ws(p);
    if (p->pos >= p->len)
        return '\0';
    return p->src[p->pos];
}

static bool expect(json_parser *p, char c)
{
    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == c) {
        p->pos++;
        return true;
    }
    return false;
}

/* ── Path manipulation ───────────────────────────────────────────────── */

static void path_push_key(json_parser *p, const char *key, size_t key_len, size_t *saved_len)
{
    *saved_len = p->path_len;
    if (p->path_len > 0) {
        p->path[p->path_len++] = '.';
    }
    if (p->path_len + key_len < sizeof(p->path)) {
        memcpy(p->path + p->path_len, key, key_len);
        p->path_len += key_len;
    }
    p->path[p->path_len] = '\0';
}

static void path_push_index(json_parser *p, uint32_t idx, size_t *saved_len)
{
    *saved_len = p->path_len;
    char tmp[16];
    int n = snprintf(tmp, sizeof(tmp), "[%u]", (unsigned)idx);
    if (n > 0 && p->path_len + (size_t)n < sizeof(p->path)) {
        memcpy(p->path + p->path_len, tmp, (size_t)n);
        p->path_len += (size_t)n;
    }
    p->path[p->path_len] = '\0';
}

static void path_restore(json_parser *p, size_t saved_len)
{
    p->path_len = saved_len;
    p->path[p->path_len] = '\0';
}

/* ── Forward declaration ─────────────────────────────────────────────── */

static tp_result parse_value(json_parser *p);

/* ── String parsing ──────────────────────────────────────────────────── */

static tp_result parse_string_into(json_parser *p, char *out, size_t out_cap, size_t *out_len)
{
    if (!expect(p, '"'))
        return TP_ERR_JSON_SYNTAX;

    size_t w = 0;
    while (p->pos < p->len) {
        char c = p->src[p->pos++];
        if (c == '"') {
            if (out && w < out_cap)
                out[w] = '\0';
            *out_len = w;
            return TP_OK;
        }
        if (c == '\\') {
            if (p->pos >= p->len)
                return TP_ERR_JSON_SYNTAX;
            char esc = p->src[p->pos++];
            switch (esc) {
            case '"':
            case '\\':
            case '/':
                c = esc;
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                /* Parse 4 hex digits -> UTF-8 */
                if (p->pos + 4 > p->len)
                    return TP_ERR_JSON_SYNTAX;
                uint32_t cp = 0;
                for (int i = 0; i < 4; i++) {
                    char h = p->src[p->pos++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9')
                        cp |= (uint32_t)(h - '0');
                    else if (h >= 'a' && h <= 'f')
                        cp |= (uint32_t)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F')
                        cp |= (uint32_t)(h - 'A' + 10);
                    else
                        return TP_ERR_JSON_SYNTAX;
                }
                /* Handle surrogate pairs */
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (p->pos + 6 > p->len || p->src[p->pos] != '\\' || p->src[p->pos + 1] != 'u')
                        return TP_ERR_JSON_SYNTAX;
                    p->pos += 2;
                    uint32_t lo = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = p->src[p->pos++];
                        lo <<= 4;
                        if (h >= '0' && h <= '9')
                            lo |= (uint32_t)(h - '0');
                        else if (h >= 'a' && h <= 'f')
                            lo |= (uint32_t)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F')
                            lo |= (uint32_t)(h - 'A' + 10);
                        else
                            return TP_ERR_JSON_SYNTAX;
                    }
                    if (lo < 0xDC00 || lo > 0xDFFF)
                        return TP_ERR_JSON_SYNTAX;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                /* Encode codepoint as UTF-8 */
                if (cp < 0x80) {
                    if (out && w < out_cap)
                        out[w] = (char)cp;
                    w++;
                } else if (cp < 0x800) {
                    if (out && w + 1 < out_cap) {
                        out[w] = (char)(0xC0 | (cp >> 6));
                        out[w + 1] = (char)(0x80 | (cp & 0x3F));
                    }
                    w += 2;
                } else if (cp < 0x10000) {
                    if (out && w + 2 < out_cap) {
                        out[w] = (char)(0xE0 | (cp >> 12));
                        out[w + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[w + 2] = (char)(0x80 | (cp & 0x3F));
                    }
                    w += 3;
                } else if (cp < 0x110000) {
                    if (out && w + 3 < out_cap) {
                        out[w] = (char)(0xF0 | (cp >> 18));
                        out[w + 1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                        out[w + 2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[w + 3] = (char)(0x80 | (cp & 0x3F));
                    }
                    w += 4;
                }
                continue; /* already handled, skip the default append */
            }
            default:
                return TP_ERR_JSON_SYNTAX;
            }
        }
        if (out && w < out_cap)
            out[w] = c;
        w++;
    }
    return TP_ERR_JSON_SYNTAX; /* unterminated string */
}

/* ── Number parsing ──────────────────────────────────────────────────── */

static tp_result parse_number(json_parser *p, tp_value *val)
{
    size_t start = p->pos;
    bool is_float = false;
    bool is_neg = false;

    if (p->pos < p->len && p->src[p->pos] == '-') {
        is_neg = true;
        p->pos++;
    }

    /* Integer part */
    if (p->pos >= p->len || !isdigit((unsigned char)p->src[p->pos]))
        return TP_ERR_JSON_SYNTAX;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        p->pos++;

    /* Fractional part */
    if (p->pos < p->len && p->src[p->pos] == '.') {
        is_float = true;
        p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->src[p->pos]))
            return TP_ERR_JSON_SYNTAX;
        while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
            p->pos++;
    }

    /* Exponent part */
    if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) {
        is_float = true;
        p->pos++;
        if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-'))
            p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->src[p->pos]))
            return TP_ERR_JSON_SYNTAX;
        while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
            p->pos++;
    }

    /* Convert */
    size_t numlen = p->pos - start;
    char tmp[64];
    if (numlen >= sizeof(tmp))
        numlen = sizeof(tmp) - 1;
    memcpy(tmp, p->src + start, numlen);
    tmp[numlen] = '\0';

    if (is_float) {
        double d = strtod(tmp, NULL);
        *val = tp_value_float64(d);
    } else if (is_neg) {
        long long ll = strtoll(tmp, NULL, 10);
        *val = tp_value_int((int64_t)ll);
    } else {
        unsigned long long ull = strtoull(tmp, NULL, 10);
        if (ull > (unsigned long long)INT64_MAX) {
            *val = tp_value_uint((uint64_t)ull);
        } else {
            *val = tp_value_int((int64_t)ull);
        }
    }
    return TP_OK;
}

/* ── Value parsing (recursive) ───────────────────────────────────────── */

static tp_result parse_object(json_parser *p)
{
    p->depth++;
    if (p->depth > TP_MAX_NESTING_DEPTH)
        return TP_ERR_JSON_DEPTH;

    if (!expect(p, '{'))
        return TP_ERR_JSON_SYNTAX; /* LCOV_EXCL_LINE */

    if (peek(p) == '}') {
        p->pos++;
        p->depth--;
        return TP_OK;
    }

    while (true) {
        /* Parse key */
        char key[1024];
        size_t key_len = 0;
        tp_result rc = parse_string_into(p, key, sizeof(key) - 1, &key_len);
        if (rc != TP_OK)
            return rc;
        key[key_len] = '\0';

        skip_ws(p);
        if (!expect(p, ':'))
            return TP_ERR_JSON_SYNTAX;

        /* Push key onto path */
        size_t saved;
        path_push_key(p, key, key_len, &saved);

        /* Parse value */
        rc = parse_value(p);
        if (rc != TP_OK)
            return rc;

        path_restore(p, saved);

        skip_ws(p);
        if (peek(p) == '}') {
            p->pos++;
            break;
        }
        if (!expect(p, ','))
            return TP_ERR_JSON_SYNTAX;
    }

    p->depth--;
    return TP_OK;
}

static tp_result parse_array(json_parser *p)
{
    p->depth++;
    if (p->depth > TP_MAX_NESTING_DEPTH)
        return TP_ERR_JSON_DEPTH;

    if (!expect(p, '['))
        return TP_ERR_JSON_SYNTAX; /* LCOV_EXCL_LINE */

    if (peek(p) == ']') {
        p->pos++;
        p->depth--;
        return TP_OK;
    }

    uint32_t idx = 0;
    while (true) {
        size_t saved;
        path_push_index(p, idx, &saved);

        tp_result rc = parse_value(p);
        if (rc != TP_OK)
            return rc;

        path_restore(p, saved);
        idx++;

        skip_ws(p);
        if (peek(p) == ']') {
            p->pos++;
            break;
        }
        if (!expect(p, ','))
            return TP_ERR_JSON_SYNTAX;
    }

    p->depth--;
    return TP_OK;
}

static tp_result parse_value(json_parser *p)
{
    skip_ws(p);
    if (p->pos >= p->len)
        return TP_ERR_JSON_SYNTAX;

    char c = p->src[p->pos];

    if (c == '{')
        return parse_object(p);

    if (c == '[')
        return parse_array(p);

    if (c == '"') {
        /* String value — parse then store at current path */
        char str[4096];
        size_t slen = 0;
        tp_result rc = parse_string_into(p, str, sizeof(str) - 1, &slen);
        if (rc != TP_OK)
            return rc;
        str[slen] = '\0';
        tp_value val = tp_value_string_n(str, slen);
        return tp_encoder_add_n(p->enc, p->path, p->path_len, &val);
    }

    if (c == '-' || isdigit((unsigned char)c)) {
        tp_value val;
        tp_result rc = parse_number(p, &val);
        if (rc != TP_OK)
            return rc;
        return tp_encoder_add_n(p->enc, p->path, p->path_len, &val);
    }

    if (p->pos + 4 <= p->len && memcmp(p->src + p->pos, "true", 4) == 0) {
        p->pos += 4;
        tp_value val = tp_value_bool(true);
        return tp_encoder_add_n(p->enc, p->path, p->path_len, &val);
    }

    if (p->pos + 5 <= p->len && memcmp(p->src + p->pos, "false", 5) == 0) {
        p->pos += 5;
        tp_value val = tp_value_bool(false);
        return tp_encoder_add_n(p->enc, p->path, p->path_len, &val);
    }

    if (p->pos + 4 <= p->len && memcmp(p->src + p->pos, "null", 4) == 0) {
        p->pos += 4;
        tp_value val = tp_value_null();
        return tp_encoder_add_n(p->enc, p->path, p->path_len, &val);
    }

    return TP_ERR_JSON_SYNTAX;
}

/* ── One-shot encode ─────────────────────────────────────────────────── */

tp_result tp_json_encode(const char *json_str, size_t json_len, uint8_t **buf, size_t *buf_len)
{
    if (!json_str || !buf || !buf_len)
        return TP_ERR_INVALID_PARAM;

    /* Allocation failure paths are excluded from coverage (LCOV_EXCL). */
    tp_encoder *enc = NULL;
    tp_result rc = tp_encoder_create(&enc);
    if (rc != TP_OK)
        return rc; /* LCOV_EXCL_LINE */

    json_parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.src = json_str;
    parser.len = json_len;
    parser.pos = 0;
    parser.depth = 0;
    parser.enc = enc;
    parser.path_len = 0;
    parser.path[0] = '\0';

    /* Determine root type and parse */
    skip_ws(&parser);
    if (parser.pos >= parser.len) {
        tp_encoder_destroy(&enc);
        return TP_ERR_JSON_SYNTAX;
    }

    uint32_t root_type;
    char root_c = parser.src[parser.pos];
    if (root_c == '{') {
        root_type = TP_JSON_ROOT_OBJECT;
        rc = parse_object(&parser);
    } else if (root_c == '[') {
        root_type = TP_JSON_ROOT_ARRAY;
        rc = parse_array(&parser);
    } else {
        tp_encoder_destroy(&enc);
        return TP_ERR_JSON_SYNTAX;
    }

    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_encoder_destroy(&enc);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    /* Store root type metadata */
    tp_value root_val = tp_value_uint(root_type);
    rc = tp_encoder_add(enc, TP_JSON_META_ROOT, &root_val);
    if (rc != TP_OK) {
        /* LCOV_EXCL_START */
        tp_encoder_destroy(&enc);
        return rc;
        /* LCOV_EXCL_STOP */
    }

    rc = tp_encoder_build(enc, buf, buf_len);
    tp_encoder_destroy(&enc);
    return rc;
}
