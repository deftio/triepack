/**
 * @file terseml.c
 * @brief terseml encoder and decoder -- C99.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

#include "terseml.h"

#include <stdlib.h>
#include <string.h>

#define BS 0x5C   /* backslash */
#define LBR 0x7B  /* { */
#define RBR 0x7D  /* } */
#define LSQ 0x5B  /* [ */
#define RSQ 0x5D  /* ] */
#define COM 0x2C  /* , */
#define COL 0x3A  /* : */

const char *tsml_strerror(tsml_result rc)
{
    switch (rc) {
    case TSML_OK: return "OK";
    case TSML_ERR_MALFORMED: return "malformed document";
    case TSML_ERR_DEPTH: return "nesting too deep";
    case TSML_ERR_ALLOC: return "out of memory";
    case TSML_ERR_PARAM: return "invalid parameter";
    }
    return "unknown";
}

/* ── Arena ───────────────────────────────────────────────────────────────
 *
 * One document owns one chain of blocks. Decoding allocates constantly --
 * nodes, child arrays, the occasional unescaped string -- and individually
 * freeing all of it is both slower and easier to get wrong than dropping the
 * chain at the end.
 */

typedef struct arena_block {
    struct arena_block *next;
    size_t used, cap;
    uint8_t data[1];
} arena_block;

struct tsml_doc {
    arena_block *blocks;
    tsml_node **roots;
    size_t nroots, roots_cap;
};

#define ARENA_MIN (64 * 1024)

static void *arena_alloc(tsml_doc *d, size_t n)
{
    n = (n + 15u) & ~(size_t)15u; /* keep everything pointer-aligned */
    arena_block *b = d->blocks;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > ARENA_MIN ? n : ARENA_MIN;
        arena_block *nb = malloc(sizeof(arena_block) + cap);
        if (!nb)
            return NULL;
        nb->next = d->blocks;
        nb->used = 0;
        nb->cap = cap;
        d->blocks = nb;
        b = nb;
    }
    void *p = b->data + b->used;
    b->used += n;
    return p;
}

static const uint8_t *arena_dup(tsml_doc *d, const uint8_t *src, size_t n)
{
    if (!n)
        return (const uint8_t *)"";
    uint8_t *p = arena_alloc(d, n);
    if (p)
        memcpy(p, src, n);
    return p;
}

static tsml_result doc_new(tsml_doc **out)
{
    tsml_doc *d = calloc(1, sizeof(*d));
    if (!d)
        return TSML_ERR_ALLOC;
    *out = d;
    return TSML_OK;
}

void tsml_doc_free(tsml_doc **doc)
{
    if (!doc || !*doc)
        return;
    arena_block *b = (*doc)->blocks;
    while (b) {
        arena_block *next = b->next;
        free(b);
        b = next;
    }
    free((*doc)->roots);
    free(*doc);
    *doc = NULL;
}

const tsml_node *tsml_doc_root(const tsml_doc *doc)
{
    return (doc && doc->nroots) ? doc->roots[0] : NULL;
}

size_t tsml_doc_count(const tsml_doc *doc)
{
    return doc ? doc->nroots : 0;
}

const tsml_node *tsml_doc_node(const tsml_doc *doc, size_t i)
{
    return (doc && i < doc->nroots) ? doc->roots[i] : NULL;
}

static tsml_result push_root(tsml_doc *d, tsml_node *n)
{
    if (d->nroots == d->roots_cap) {
        size_t cap = d->roots_cap ? d->roots_cap * 2 : 8;
        tsml_node **g = realloc(d->roots, cap * sizeof(*g));
        if (!g)
            return TSML_ERR_ALLOC;
        d->roots = g;
        d->roots_cap = cap;
    }
    d->roots[d->nroots++] = n;
    return TSML_OK;
}

/* ── Byte buffer, for encoding ───────────────────────────────────────── */

typedef struct {
    uint8_t *p;
    size_t len, cap;
    int oom;
} buf;

static void buf_need(buf *b, size_t n)
{
    if (b->oom || b->cap - b->len >= n)
        return;
    size_t cap = b->cap ? b->cap * 2 : 4096;
    while (cap - b->len < n)
        cap *= 2;
    uint8_t *g = realloc(b->p, cap);
    if (!g) {
        b->oom = 1;
        return;
    }
    b->p = g;
    b->cap = cap;
}

static void buf_put(buf *b, uint8_t c)
{
    buf_need(b, 1);
    if (!b->oom)
        b->p[b->len++] = c;
}

static void buf_write(buf *b, const uint8_t *src, size_t n)
{
    buf_need(b, n);
    if (!b->oom && n) {
        memcpy(b->p + b->len, src, n);
        b->len += n;
    }
}

/* ── Escaping ────────────────────────────────────────────────────────────
 *
 * Reserved bytes are positional (GRAMMAR.md §5): only the first comma of an
 * element is structural, '[' matters only in the attribute slot. Escaping
 * more than required still round-trips, which is why it is easy to get wrong
 * and expensive when you do -- over-escaping Wikipedia text cost 5.3%.
 */

typedef enum { CTX_TAG, CTX_KEY, CTX_VAL, CTX_TEXT } esc_ctx;

static int is_reserved(esc_ctx ctx, uint8_t c)
{
    if (c == BS)
        return 1;
    switch (ctx) {
    case CTX_TAG: return c == COM || c == LBR || c == RBR;
    case CTX_KEY: return c == COL || c == COM || c == RSQ;
    case CTX_VAL: return c == COM || c == RSQ;
    case CTX_TEXT: return c == LBR || c == RBR;
    }
    return 0;
}

/* Scan in runs: most content has no reserved bytes, so the common path is a
   single memcpy rather than a per-byte test. */
static void write_escaped(buf *b, esc_ctx ctx, const uint8_t *s, size_t n, int lead_bracket)
{
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = s[i];
        int esc = is_reserved(ctx, c) || (i == 0 && lead_bracket && c == LSQ);
        if (!esc)
            continue;
        buf_write(b, s + start, i - start);
        buf_put(b, BS);
        buf_put(b, c);
        start = i + 1;
    }
    buf_write(b, s + start, n - start);
}

static int valid_escape(uint8_t c)
{
    return c == LBR || c == RBR || c == LSQ || c == RSQ || c == COM || c == COL || c == BS;
}

/* ── varint (LEB128) ─────────────────────────────────────────────────── */

static size_t varint_len(uint64_t n)
{
    size_t k = 1;
    while (n >= 0x80) {
        n >>= 7;
        k++;
    }
    return k;
}

static void varint_put(buf *b, uint64_t n)
{
    for (;;) {
        uint8_t c = (uint8_t)(n & 0x7F);
        n >>= 7;
        buf_put(b, (uint8_t)(n ? (c | 0x80) : c));
        if (!n)
            return;
    }
}

/* ── Binary run selection (GRAMMAR.md §5.1) ──────────────────────────────
 *
 * Shortest wins, ties break counted / terminated / inline. Mandatory, not
 * advisory: three legal encodings of one payload would be three legal files
 * for one document.
 */

typedef enum { FORM_INLINE, FORM_TERMINATED, FORM_COUNTED } bin_form;

static bin_form choose_form(const uint8_t *s, size_t n, int lead_bracket)
{
    size_t inl = n, term = n + 3, cnt = 2 + varint_len(n) + n;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == LBR || s[i] == RBR || s[i] == BS)
            inl++;
        if (s[i] == 0x00 || s[i] == BS)
            term++;
    }
    if (lead_bracket && n && s[0] == LSQ)
        inl++;
    if (cnt <= term && cnt <= inl)
        return FORM_COUNTED;
    if (term <= inl)
        return FORM_TERMINATED;
    return FORM_INLINE;
}

static void write_binary(buf *b, const uint8_t *s, size_t n, bin_form form)
{
    if (form == FORM_COUNTED) {
        buf_put(b, BS);
        buf_put(b, 'B');
        varint_put(b, n);
        buf_write(b, s, n);
        return;
    }
    buf_put(b, BS);
    buf_put(b, 'b');
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] != 0x00 && s[i] != BS)
            continue;
        buf_write(b, s + start, i - start);
        buf_put(b, BS);
        buf_put(b, s[i] == 0x00 ? (uint8_t)'0' : (uint8_t)BS);
        start = i + 1;
    }
    buf_write(b, s + start, n - start);
    buf_put(b, 0x00);
}

/* ── Encoding ────────────────────────────────────────────────────────── */

static void encode_node(buf *b, const tsml_node *n, int depth)
{
    if (depth > TSML_MAX_DEPTH) {
        b->oom = 1; /* reported as ALLOC; the caller sees a failure either way */
        return;
    }

    buf_put(b, LBR);

    if (n->kind == TSML_TEXT) {
        /* §7 rule 7: a bare text node is written with its tag spelled out.
           The short form {foo} would make a comma in the content structural
           -- {a,b} reads back as an element -- for six saved bytes. */
        bin_form f;
        buf_write(b, (const uint8_t *)TSML_TAG_TEXT, 5);
        buf_put(b, COM);
        f = choose_form(n->text, n->text_len, 1);
        if (f == FORM_INLINE)
            write_escaped(b, CTX_TEXT, n->text, n->text_len, 1);
        else
            write_binary(b, n->text, n->text_len, f);
        buf_put(b, RBR);
        return;
    }

    write_escaped(b, CTX_TAG, n->tag, n->tag_len, 0);

    if (n->nattrs) {
        buf_put(b, COM);
        buf_put(b, LSQ);
        for (size_t i = 0; i < n->nattrs; i++) {
            if (i)
                buf_put(b, COM);
            write_escaped(b, CTX_KEY, n->attrs[i].key, n->attrs[i].key_len, 0);
            buf_put(b, COL);
            write_escaped(b, CTX_VAL, n->attrs[i].val, n->attrs[i].val_len, 0);
        }
        buf_put(b, RSQ);
    }

    if (n->nchildren) {
        buf_put(b, COM);
        for (size_t i = 0; i < n->nchildren; i++) {
            const tsml_node *c = n->children[i];
            if (c->kind == TSML_ELEMENT) {
                encode_node(b, c, depth + 1);
                continue;
            }
            int at_start = (i == 0 && n->nattrs == 0);
            bin_form f = choose_form(c->text, c->text_len, at_start);
            if (f == FORM_INLINE)
                write_escaped(b, CTX_TEXT, c->text, c->text_len, at_start);
            else
                write_binary(b, c->text, c->text_len, f);
        }
    } else {
        /* §4.3: an element always has a comma, leaving {foo} free to mean the
           singleton on the way in. */
        buf_put(b, COM);
    }

    buf_put(b, RBR);
}

tsml_result tsml_encode(const tsml_node *node, uint8_t **out, size_t *len)
{
    if (!node || !out || !len)
        return TSML_ERR_PARAM;
    buf b = {0};
    encode_node(&b, node, 0);
    if (b.oom) {
        free(b.p);
        return TSML_ERR_ALLOC;
    }
    *out = b.p;
    *len = b.len;
    return TSML_OK;
}

/* ── Decoding ────────────────────────────────────────────────────────── */

typedef struct {
    const uint8_t *buf;
    size_t len, i;
    tsml_doc *doc;
} rdr;

/* Read until an unescaped byte the caller stops on. Where there are no
   escapes -- the common case -- nothing is copied; the result points into the
   caller's buffer. */
static tsml_result read_escaped(rdr *r, esc_ctx ctx, const uint8_t **out, size_t *out_len)
{
    size_t start = r->i, i = start;
    const uint8_t *b = r->buf;
    size_t n = r->len;

    while (i < n && b[i] != BS && !is_reserved(ctx, b[i])) {
        /* is_reserved covers BS too, but checking it first keeps the hot
           loop honest about what stops it. */
        i++;
    }
    if (i >= n)
        return TSML_ERR_MALFORMED;

    if (b[i] != BS) {
        *out = b + start;
        *out_len = i - start;
        r->i = i;
        return TSML_OK;
    }

    /* There is at least one escape, so a copy is needed. Find the end of the
       run and count the escapes before allocating: sizing the buffer at the
       remaining input instead costs the arena a whole input's worth of space
       per escaped text node, which on 15 MB of Wikipedia asked malloc for
       16 GB. */
    size_t end = i, nesc = 0;
    while (end < n) {
        if (b[end] == BS) {
            if (end + 1 >= n || !valid_escape(b[end + 1]))
                return TSML_ERR_MALFORMED;
            nesc++;
            end += 2;
            continue;
        }
        if (is_reserved(ctx, b[end]))
            break;
        end++;
    }
    if (end >= n)
        return TSML_ERR_MALFORMED;

    uint8_t *dst = arena_alloc(r->doc, end - start - nesc);
    if (!dst)
        return TSML_ERR_ALLOC;

    size_t w = 0, pos = start;
    while (pos < end) {
        if (b[pos] == BS) {
            dst[w++] = b[pos + 1];
            pos += 2;
            continue;
        }
        size_t run = pos;
        while (run < end && b[run] != BS)
            run++;
        memcpy(dst + w, b + pos, run - pos);
        w += run - pos;
        pos = run;
    }

    *out = dst;
    *out_len = w;
    r->i = end;
    return TSML_OK;
}

static tsml_result read_binary(rdr *r, const uint8_t **out, size_t *out_len)
{
    const uint8_t *b = r->buf;
    size_t n = r->len;
    r->i++; /* backslash */
    if (r->i >= n)
        return TSML_ERR_MALFORMED;
    uint8_t kind = b[r->i++];

    if (kind == 'B') {
        uint64_t claim = 0;
        unsigned shift = 0, groups = 0;
        for (;;) {
            if (r->i >= n || ++groups > 10)
                return TSML_ERR_MALFORMED;
            uint8_t c = b[r->i++];
            claim |= (uint64_t)(c & 0x7F) << shift;
            shift += 7;
            if (!(c & 0x80))
                break;
        }
        /* §6: check the length against what remains BEFORE trusting it. A
           length read from the file and taken on faith is how a decoder is
           made to abort on a ten-byte input. */
        if (claim > (uint64_t)(n - r->i))
            return TSML_ERR_MALFORMED;
        *out = b + r->i;
        *out_len = (size_t)claim;
        r->i += (size_t)claim;
        return TSML_OK;
    }
    if (kind != 'b')
        return TSML_ERR_MALFORMED;

    size_t start = r->i, i = start;
    while (i < n && b[i] != 0x00 && b[i] != BS)
        i++;
    if (i < n && b[i] == 0x00) { /* no escapes: point into the buffer */
        *out = b + start;
        *out_len = i - start;
        r->i = i + 1;
        return TSML_OK;
    }

    /* Measure the run before allocating it, for the same reason read_escaped
       does: sizing at the remaining input reserves a whole document's worth
       of arena per run. Two \b runs in 15 MB of Wikipedia cost 21.8 MB. */
    size_t end = start, nesc = 0;
    while (end < n && b[end] != 0x00) {
        if (b[end] == BS) {
            if (end + 1 >= n || (b[end + 1] != '0' && b[end + 1] != BS))
                return TSML_ERR_MALFORMED;
            nesc++;
            end += 2;
            continue;
        }
        end++;
    }
    if (end >= n)
        return TSML_ERR_MALFORMED; /* unterminated run */

    uint8_t *dst = arena_alloc(r->doc, end - start - nesc);
    if (!dst)
        return TSML_ERR_ALLOC;
    size_t w = 0;
    i = start;
    while (i < n) {
        uint8_t c = b[i];
        if (c == 0x00) {
            *out = dst;
            *out_len = w;
            r->i = i + 1;
            return TSML_OK;
        }
        if (c == BS) {
            if (i + 1 >= n)
                return TSML_ERR_MALFORMED;
            uint8_t e = b[i + 1];
            if (e == '0')
                dst[w++] = 0x00;
            else if (e == BS)
                dst[w++] = BS;
            else
                return TSML_ERR_MALFORMED;
            i += 2;
            continue;
        }
        dst[w++] = c;
        i++;
    }
    return TSML_ERR_MALFORMED; /* unterminated */
}

typedef struct {
    tsml_node **v;
    size_t n, cap;
} kidvec;

typedef struct {
    tsml_attr *v;
    size_t n, cap;
} attrvec;

static tsml_result decode_element(rdr *r, int depth, tsml_node **out);

static tsml_result decode_children(rdr *r, int depth, tsml_node *el)
{
    kidvec kids = {0};
    const uint8_t *b = r->buf;
    size_t n = r->len;
    tsml_result rc;

    for (;;) {
        if (r->i >= n) {
            rc = TSML_ERR_MALFORMED;
            goto fail;
        }
        uint8_t c = b[r->i];

        if (c == RBR) {
            r->i++;
            break;
        }

        tsml_node *child = NULL;
        if (c == LBR) {
            rc = decode_element(r, depth + 1, &child);
            if (rc != TSML_OK)
                goto fail;
        } else {
            const uint8_t *t = NULL;
            size_t tl = 0;
            if (c == BS && r->i + 1 < n && (b[r->i + 1] == 'b' || b[r->i + 1] == 'B')) {
                /* §5.1: a letter after the escape byte introduces a command.
                   Reserved bytes are all punctuation, so they cannot collide. */
                rc = read_binary(r, &t, &tl);
            } else {
                rc = read_escaped(r, CTX_TEXT, &t, &tl);
            }
            if (rc != TSML_OK)
                goto fail;
            if (!tl)
                continue;
            child = arena_alloc(r->doc, sizeof(*child));
            if (!child) {
                rc = TSML_ERR_ALLOC;
                goto fail;
            }
            memset(child, 0, sizeof(*child));
            child->kind = TSML_TEXT;
            child->text = t;
            child->text_len = tl;
        }

        if (kids.n == kids.cap) {
            size_t cap = kids.cap ? kids.cap * 2 : 4;
            tsml_node **g = realloc(kids.v, cap * sizeof(*g));
            if (!g) {
                rc = TSML_ERR_ALLOC;
                goto fail;
            }
            kids.v = g;
            kids.cap = cap;
        }
        kids.v[kids.n++] = child;
    }

    if (kids.n) {
        el->children = arena_alloc(r->doc, kids.n * sizeof(*el->children));
        if (!el->children) {
            rc = TSML_ERR_ALLOC;
            goto fail;
        }
        memcpy(el->children, kids.v, kids.n * sizeof(*el->children));
        el->nchildren = kids.n;
    }
    free(kids.v);
    return TSML_OK;

fail:
    free(kids.v);
    return rc;
}

static tsml_result decode_attrs(rdr *r, tsml_node *el)
{
    attrvec av = {0};
    const uint8_t *b = r->buf;
    size_t n = r->len;
    tsml_result rc;

    r->i++; /* '[' */
    if (r->i < n && b[r->i] != RSQ) {
        for (;;) {
            tsml_attr a;
            rc = read_escaped(r, CTX_KEY, &a.key, &a.key_len);
            if (rc != TSML_OK)
                goto fail;
            if (r->i >= n || b[r->i] != COL) {
                rc = TSML_ERR_MALFORMED;
                goto fail;
            }
            r->i++;
            rc = read_escaped(r, CTX_VAL, &a.val, &a.val_len);
            if (rc != TSML_OK)
                goto fail;

            if (av.n == av.cap) {
                size_t cap = av.cap ? av.cap * 2 : 4;
                tsml_attr *g = realloc(av.v, cap * sizeof(*g));
                if (!g) {
                    rc = TSML_ERR_ALLOC;
                    goto fail;
                }
                av.v = g;
                av.cap = cap;
            }
            av.v[av.n++] = a;

            if (r->i < n && b[r->i] == COM) {
                r->i++;
                if (r->i < n && b[r->i] == RSQ) /* §7 rule 2: accepted, never written */
                    break;
                continue;
            }
            break;
        }
    }
    if (r->i >= n || b[r->i] != RSQ) {
        rc = TSML_ERR_MALFORMED;
        goto fail;
    }
    r->i++;

    if (av.n) {
        el->attrs = arena_alloc(r->doc, av.n * sizeof(*el->attrs));
        if (!el->attrs) {
            rc = TSML_ERR_ALLOC;
            goto fail;
        }
        memcpy(el->attrs, av.v, av.n * sizeof(*el->attrs));
        el->nattrs = av.n;
    }
    free(av.v);
    return TSML_OK;

fail:
    free(av.v);
    return rc;
}

static tsml_result decode_element(rdr *r, int depth, tsml_node **out)
{
    if (depth > TSML_MAX_DEPTH)
        return TSML_ERR_DEPTH;
    const uint8_t *b = r->buf;
    size_t n = r->len;
    if (r->i >= n || b[r->i] != LBR)
        return TSML_ERR_MALFORMED;
    r->i++;

    tsml_node *el = arena_alloc(r->doc, sizeof(*el));
    if (!el)
        return TSML_ERR_ALLOC;
    memset(el, 0, sizeof(*el));
    el->kind = TSML_ELEMENT;

    const uint8_t *head;
    size_t head_len;
    tsml_result rc = read_escaped(r, CTX_TAG, &head, &head_len);
    if (rc != TSML_OK)
        return rc;

    if (r->i >= n)
        return TSML_ERR_MALFORMED;

    if (b[r->i] == RBR) {
        /* §4.3: no comma -- a singleton, content with the implicit tag. */
        r->i++;
        el->tag = (const uint8_t *)TSML_TAG_TEXT;
        el->tag_len = 5;
        if (head_len) {
            tsml_node *t = arena_alloc(r->doc, sizeof(*t));
            if (!t)
                return TSML_ERR_ALLOC;
            memset(t, 0, sizeof(*t));
            t->kind = TSML_TEXT;
            t->text = head;
            t->text_len = head_len;
            el->children = arena_alloc(r->doc, sizeof(*el->children));
            if (!el->children)
                return TSML_ERR_ALLOC;
            el->children[0] = t;
            el->nchildren = 1;
        }
        *out = el;
        return TSML_OK;
    }
    if (b[r->i] == LBR)
        return TSML_ERR_MALFORMED; /* a child with no comma after the tag */
    if (b[r->i] != COM)
        return TSML_ERR_MALFORMED;
    r->i++;

    el->tag = head;
    el->tag_len = head_len;

    /* §4.2: attributes iff the byte after the comma is '['. */
    if (r->i < n && b[r->i] == LSQ) {
        rc = decode_attrs(r, el);
        if (rc != TSML_OK)
            return rc;
        if (r->i < n && b[r->i] == COM) {
            r->i++;
        } else if (r->i < n && b[r->i] == RBR) {
            r->i++;
            *out = el;
            return TSML_OK;
        }
    }

    rc = decode_children(r, depth, el);
    if (rc != TSML_OK)
        return rc;
    *out = el;
    return TSML_OK;
}

tsml_result tsml_decode(const uint8_t *buf, size_t len, tsml_doc **out)
{
    if (!buf || !out)
        return TSML_ERR_PARAM;
    tsml_doc *d = NULL;
    tsml_result rc = doc_new(&d);
    if (rc != TSML_OK)
        return rc;

    rdr r = {buf, len, 0, d};
    tsml_node *root = NULL;
    rc = decode_element(&r, 0, &root);
    if (rc == TSML_OK && r.i != len)
        rc = TSML_ERR_MALFORMED; /* trailing bytes */
    if (rc == TSML_OK)
        rc = push_root(d, root);
    if (rc != TSML_OK) {
        tsml_doc_free(&d);
        return rc;
    }
    *out = d;
    return TSML_OK;
}

tsml_result tsml_decode_document(const uint8_t *buf, size_t len, tsml_doc **out)
{
    if (!buf || !out)
        return TSML_ERR_PARAM;
    tsml_doc *d = NULL;
    tsml_result rc = doc_new(&d);
    if (rc != TSML_OK)
        return rc;

    rdr r = {buf, len, 0, d};
    while (r.i < len) {
        tsml_node *node = NULL;
        rc = decode_element(&r, 0, &node);
        if (rc == TSML_OK)
            rc = push_root(d, node);
        if (rc != TSML_OK) {
            tsml_doc_free(&d);
            return rc;
        }
    }
    if (!d->nroots) {
        tsml_doc_free(&d);
        return TSML_ERR_MALFORMED;
    }
    *out = d;
    return TSML_OK;
}

/* ── Building ────────────────────────────────────────────────────────── */

tsml_result tsml_builder_create(tsml_doc **out)
{
    if (!out)
        return TSML_ERR_PARAM;
    return doc_new(out);
}

tsml_node *tsml_text(tsml_doc *a, const uint8_t *text, size_t len)
{
    if (!a)
        return NULL;
    tsml_node *n = arena_alloc(a, sizeof(*n));
    if (!n)
        return NULL;
    memset(n, 0, sizeof(*n));
    n->kind = TSML_TEXT;
    n->text = arena_dup(a, text, len);
    n->text_len = len;
    return (len && !n->text) ? NULL : n;
}

tsml_node *tsml_element(tsml_doc *a, const uint8_t *tag, size_t len)
{
    if (!a)
        return NULL;
    tsml_node *n = arena_alloc(a, sizeof(*n));
    if (!n)
        return NULL;
    memset(n, 0, sizeof(*n));
    n->kind = TSML_ELEMENT;
    n->tag = arena_dup(a, tag, len);
    n->tag_len = len;
    return (len && !n->tag) ? NULL : n;
}

tsml_result tsml_add_attr(tsml_doc *a, tsml_node *el, const uint8_t *key, size_t key_len,
                          const uint8_t *val, size_t val_len)
{
    if (!a || !el || el->kind != TSML_ELEMENT)
        return TSML_ERR_PARAM;
    tsml_attr *grown = arena_alloc(a, (el->nattrs + 1) * sizeof(*grown));
    if (!grown)
        return TSML_ERR_ALLOC;
    if (el->nattrs)
        memcpy(grown, el->attrs, el->nattrs * sizeof(*grown));
    grown[el->nattrs].key = arena_dup(a, key, key_len);
    grown[el->nattrs].key_len = key_len;
    grown[el->nattrs].val = arena_dup(a, val, val_len);
    grown[el->nattrs].val_len = val_len;
    el->attrs = grown;
    el->nattrs++;
    return TSML_OK;
}

tsml_result tsml_add_child(tsml_doc *a, tsml_node *el, tsml_node *child)
{
    if (!a || !el || !child || el->kind != TSML_ELEMENT)
        return TSML_ERR_PARAM;
    tsml_node **grown = arena_alloc(a, (el->nchildren + 1) * sizeof(*grown));
    if (!grown)
        return TSML_ERR_ALLOC;
    if (el->nchildren)
        memcpy(grown, el->children, el->nchildren * sizeof(*grown));
    grown[el->nchildren] = child;
    el->children = grown;
    el->nchildren++;
    return TSML_OK;
}
