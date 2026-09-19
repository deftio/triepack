/**
 * @file v2_prototype.c
 * @brief Working prototype of the v2 format core, for measurement.
 *
 * Builds the structures described in docs/internals/format-spec-v2.md --
 * radix trie, LOUDS bit vector with rank/select, raw byte labels, suffix-
 * merged tail pool, terminal bitmap -- serialises them, and then performs
 * every lookup *through the serialised bytes* so the numbers describe the
 * format rather than an in-memory convenience.
 *
 * It also measures what a second Huffman pass over the byte-oriented
 * sections would save, so that decision is made on data.
 *
 * This is a measurement tool, not the shipping implementation. It exists to
 * run the Phase 2 falsification gate in docs/internals/v2-implementation-plan.md
 * before anyone ports the design nine more times.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Build-time trie ─────────────────────────────────────────────────── */

typedef struct node {
    uint8_t label; /* first byte of the edge entering this node */
    uint8_t *tail; /* bytes after the first, or NULL */
    uint32_t tail_len;
    struct node **kids; /* sorted by label */
    uint32_t nkids, cap;
    bool terminal;
    uint64_t value; /* meaningful when terminal */
    uint32_t id;    /* BFS index, assigned after compression */
} node;

static node *node_new(uint8_t label)
{
    node *n = calloc(1, sizeof(*n));
    if (!n) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    n->label = label;
    return n;
}

/* Children are kept sorted so the serialised label run is ascending, which
   is what lets descent binary-search and what makes output deterministic. */
static node *child_get_or_add(node *p, uint8_t label)
{
    uint32_t lo = 0, hi = p->nkids;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (p->kids[mid]->label < label)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < p->nkids && p->kids[lo]->label == label)
        return p->kids[lo];

    if (p->nkids == p->cap) {
        uint32_t ncap = p->cap ? p->cap * 2 : 4;
        node **grown = realloc(p->kids, ncap * sizeof(*grown));
        if (!grown) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        p->kids = grown;
        p->cap = ncap;
    }
    memmove(&p->kids[lo + 1], &p->kids[lo], (p->nkids - lo) * sizeof(*p->kids));
    node *c = node_new(label);
    p->kids[lo] = c;
    p->nkids++;
    return c;
}

static void trie_insert(node *root, const uint8_t *key, size_t len, uint64_t value)
{
    node *cur = root;
    for (size_t i = 0; i < len; i++)
        cur = child_get_or_add(cur, key[i]);
    cur->terminal = true;
    cur->value = value;
}

/* Collapse non-branching, non-terminal chains into the parent edge's tail.
   This is the radix compression: an edge carries a first byte plus a tail. */
static void trie_compress(node *v)
{
    for (uint32_t i = 0; i < v->nkids; i++) {
        node *c = v->kids[i];
        while (c->nkids == 1 && !c->terminal) {
            node *g = c->kids[0];
            uint8_t *grown = realloc(c->tail, c->tail_len + 1 + g->tail_len);
            if (!grown) {
                fprintf(stderr, "out of memory\n");
                exit(1);
            }
            c->tail = grown;
            c->tail[c->tail_len++] = g->label;

            /* c absorbs g's tail, children and terminal flag. */
            if (g->tail_len) {
                memcpy(c->tail + c->tail_len, g->tail, g->tail_len);
                c->tail_len += g->tail_len;
                free(g->tail);
            }
            free(c->kids);
            c->kids = g->kids;
            c->nkids = g->nkids;
            c->cap = g->cap;
            c->terminal = g->terminal;
            c->value = g->value;
            free(g);
        }
        trie_compress(c);
    }
}

static void trie_free(node *v)
{
    for (uint32_t i = 0; i < v->nkids; i++)
        trie_free(v->kids[i]);
    free(v->kids);
    free(v->tail);
    free(v);
}

/* ── Ranked bit vector (spec §5) ─────────────────────────────────────── */

typedef struct {
    uint8_t *bits;
    uint64_t nbits, cap_bits;
    uint64_t *super; /* rank1 before each 2048-bit superblock */
    uint16_t *block; /* rank1 within superblock, before each 256-bit block */
} bitvec;

static void bv_push(bitvec *b, int bit)
{
    if (b->nbits == b->cap_bits) {
        uint64_t ncap = b->cap_bits ? b->cap_bits * 2 : 4096;
        uint8_t *grown = realloc(b->bits, (ncap + 7) / 8);
        if (!grown) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        b->bits = grown;
        b->cap_bits = ncap;
        memset(b->bits + (b->nbits + 7) / 8, 0, (ncap + 7) / 8 - (b->nbits + 7) / 8);
    }
    if (bit)
        b->bits[b->nbits >> 3] |= (uint8_t)(0x80 >> (b->nbits & 7));
    b->nbits++;
}

static int bv_get(const bitvec *b, uint64_t i)
{
    return (b->bits[i >> 3] >> (7 - (i & 7))) & 1;
}

static void bv_build_index(bitvec *b)
{
    uint64_t nsuper = (b->nbits + 2047) / 2048 + 1;
    uint64_t nblock = (b->nbits + 255) / 256 + 1;
    b->super = calloc(nsuper, sizeof(uint64_t));
    b->block = calloc(nblock, sizeof(uint16_t));
    /* Run to <= nbits so the last entries follow the same rule as the rest.
       Separate terminator writes give them different semantics and double
       count where a superblock boundary lands exactly at the end -- caught
       by tests/test_v2_bitvec.c. */
    (void)nsuper;
    (void)nblock;
    uint64_t total = 0, within = 0;
    for (uint64_t i = 0; i <= b->nbits; i++) {
        if (i % 2048 == 0) {
            b->super[i / 2048] = total;
            within = 0;
        }
        if (i % 256 == 0)
            b->block[i / 256] = (uint16_t)within;
        if (i < b->nbits && bv_get(b, i)) {
            total++;
            within++;
        }
    }
}

/* Popcount the tail of the block rather than walking it a bit at a time.
   Spec §5.2 allows a builtin or a portable fallback; both must agree. */
static int popcount64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
    return (int)((x * 0x0101010101010101ull) >> 56);
#endif
}

static uint64_t bv_rank1(const bitvec *b, uint64_t i)
{
    uint64_t c = b->super[i / 2048] + b->block[i / 256];
    uint64_t j = (i / 256) * 256;

    /* whole 64-bit words */
    for (; j + 64 <= i; j += 64) {
        uint64_t w = 0;
        for (int k = 0; k < 8; k++)
            w = (w << 8) | b->bits[(j >> 3) + k];
        c += (uint64_t)popcount64(w);
    }
    /* whole bytes */
    for (; j + 8 <= i; j += 8)
        c += (uint64_t)popcount64((uint64_t)b->bits[j >> 3]);
    /* trailing bits */
    for (; j < i; j++)
        c += (uint64_t)bv_get(b, j);
    return c;
}

/* Position of the k-th 0 bit, 0-indexed. Binary search, per spec §5.3. */
static uint64_t bv_select0(const bitvec *b, uint64_t k)
{
    uint64_t lo = 0, hi = b->nbits;
    while (lo < hi) {
        uint64_t mid = lo + (hi - lo) / 2;
        if ((mid + 1) - bv_rank1(b, mid + 1) <= k)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static uint64_t bv_index_bytes(const bitvec *b)
{
    return ((b->nbits + 2047) / 2048 + 1) * 8 + ((b->nbits + 255) / 256 + 1) * 2;
}

/* ── Tail pool with suffix merging (spec §8.1) ───────────────────────── */

typedef struct {
    uint8_t *bytes;
    uint32_t len;
    uint32_t off;
} tail;

static int tail_cmp_reversed(const void *a, const void *b)
{
    const tail *x = a, *y = b;
    uint32_t i = 0;
    while (i < x->len && i < y->len) {
        uint8_t xb = x->bytes[x->len - 1 - i], yb = y->bytes[y->len - 1 - i];
        if (xb != yb)
            return xb < yb ? -1 : 1;
        i++;
    }
    if (x->len != y->len)
        return x->len < y->len ? -1 : 1;
    return 0;
}

static bool is_suffix_of(const tail *s, const tail *big)
{
    return s->len < big->len && memcmp(big->bytes + big->len - s->len, s->bytes, s->len) == 0;
}

/* ── Bit writer (spec §3.1: packed arrays, most significant bit first) ── */

typedef struct {
    uint8_t *buf;
    uint64_t nbits, cap_bits;
} bitwriter;

static void bw_put(bitwriter *w, uint64_t value, int width)
{
    if (w->nbits + (uint64_t)width > w->cap_bits) {
        uint64_t ncap = w->cap_bits ? w->cap_bits * 2 : 65536;
        while (ncap < w->nbits + (uint64_t)width)
            ncap *= 2;
        uint8_t *grown = realloc(w->buf, (ncap + 7) / 8);
        if (!grown) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        w->buf = grown;
        memset(w->buf + (w->nbits + 7) / 8, 0, (ncap + 7) / 8 - (w->nbits + 7) / 8);
        w->cap_bits = ncap;
    }
    for (int b = width - 1; b >= 0; b--) {
        if ((value >> b) & 1)
            w->buf[w->nbits >> 3] |= (uint8_t)(0x80 >> (w->nbits & 7));
        w->nbits++;
    }
}

static void bw_bytes(bitwriter *w, const uint8_t *data, uint64_t n)
{
    for (uint64_t i = 0; i < n; i++)
        bw_put(w, data[i], 8);
}

/* ── Value store ─────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *buf;
    uint64_t len, cap;
} bytebuf;

static void bb_push(bytebuf *b, uint8_t byte)
{
    if (b->len == b->cap) {
        uint64_t ncap = b->cap ? b->cap * 2 : 4096;
        uint8_t *grown = realloc(b->buf, ncap);
        if (!grown) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        b->buf = grown;
        b->cap = ncap;
    }
    b->buf[b->len++] = byte;
}

static void leb128_write(bytebuf *b, uint64_t v)
{
    do {
        uint8_t byte = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v)
            byte |= 0x80;
        bb_push(b, byte);
    } while (v);
}

static uint64_t leb128_read(const uint8_t *buf, uint64_t *pos)
{
    uint64_t v = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = buf[(*pos)++];
        v |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return v;
}

/* ── Huffman cost model ──────────────────────────────────────────────── */

/* Optimal code lengths for the given frequencies, by repeated merge. Used
   only to answer "would a second Huffman pass pay?", so it returns the coded
   size in bytes rather than building a codec. */
static double huffman_bytes(const uint64_t freq[256], uint64_t *out_symbols)
{
    uint64_t w[512];
    int left[512], right[512], nn = 0;
    int nsym = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i]) {
            w[nn] = freq[i];
            left[nn] = right[nn] = -1;
            nsym++;
            nn++;
        }
    }
    if (out_symbols)
        *out_symbols = (uint64_t)nsym;
    if (nsym == 0)
        return 0.0;
    if (nsym == 1)
        return (double)w[0] / 8.0; /* 1 bit per occurrence */

    bool used[512] = {false};
    int remaining = nsym;
    while (remaining > 1) {
        int a = -1, b = -1;
        for (int i = 0; i < nn; i++) {
            if (used[i])
                continue;
            if (a < 0 || w[i] < w[a]) {
                b = a;
                a = i;
            } else if (b < 0 || w[i] < w[b]) {
                b = i;
            }
        }
        used[a] = used[b] = true;
        w[nn] = w[a] + w[b];
        left[nn] = a;
        right[nn] = b;
        nn++;
        remaining--;
    }
    /* Total coded bits = sum of internal node weights. */
    double bits = 0;
    for (int i = 0; i < nn; i++)
        if (left[i] >= 0)
            bits += (double)w[i];
    return bits / 8.0;
}

/* Bits needed to index n distinct values. */
static int bits_needed(uint64_t n)
{
    int b = 1;
    while (n > (1ull << b))
        b++;
    return b;
}

/* ── Main ────────────────────────────────────────────────────────────── */

typedef struct {
    uint64_t louds_bits, louds_idx;
    uint64_t labels;
    uint64_t tail_presence_bits, tail_presence_idx, tail_refs, tail_pool;
    uint64_t term_bits, term_idx;
} sizes;

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "tests/data/common_words_10k.txt";
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    /* On-disk size of the input, so the report can state a ratio against
       something the reader can check with `ls`. */
    fseek(f, 0, SEEK_END);
    long file_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    node *root = node_new(0);
    char line[4096];
    uint64_t nwords = 0, raw_bytes = 0;
    uint8_t **words = NULL;
    size_t *wlens = NULL;
    size_t wcap = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (!n)
            continue;
        if (nwords == wcap) {
            wcap = wcap ? wcap * 2 : 1024;
            words = realloc(words, wcap * sizeof(*words));
            wlens = realloc(wlens, wcap * sizeof(*wlens));
        }
        /* strdup is POSIX, not C99; under -std=c99 gcc implicitly declares it
           as returning int and truncates the pointer. Copy it by hand. */
        uint8_t *copy = malloc(n + 1);
        if (!copy) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        memcpy(copy, line, n + 1);
        words[nwords] = copy;
        wlens[nwords] = n;
        trie_insert(root, (const uint8_t *)line, n, nwords);
        raw_bytes += n;
        nwords++;
    }
    fclose(f);

    clock_t t0 = clock();
    trie_compress(root);

    /* BFS numbering, which is the order LOUDS encodes. */
    node **bfs = malloc(sizeof(*bfs) * (nwords * 4 + 16));
    uint32_t head = 0, tailq = 0;
    root->id = 0;
    bfs[tailq++] = root;
    while (head < tailq) {
        node *v = bfs[head++];
        for (uint32_t i = 0; i < v->nkids; i++) {
            v->kids[i]->id = tailq;
            bfs[tailq++] = v->kids[i];
        }
    }
    uint32_t nnodes = tailq;

    /* Serialise. */
    bitvec louds = {0}, tails_present = {0}, terminals = {0};
    bytebuf values = {0};
    uint64_t *value_off = malloc(sizeof(uint64_t) * (nwords ? nwords : 1));
    uint64_t nvalues = 0, max_val_width = 0;
    uint8_t *labels = malloc(nnodes ? nnodes : 1);
    uint64_t nlabels = 0;

    tail *tl = calloc(nnodes ? nnodes : 1, sizeof(tail));
    uint32_t ntails = 0;

    for (uint32_t i = 0; i < nnodes; i++) {
        node *v = bfs[i];
        for (uint32_t k = 0; k < v->nkids; k++)
            bv_push(&louds, 1);
        bv_push(&louds, 0);
        bv_push(&terminals, v->terminal ? 1 : 0);
        if (v->terminal) {
            /* Values go in terminal order, so the ordinal is rank1 over the
               terminal bitmap and nothing is stored per key. */
            value_off[nvalues] = values.len;
            leb128_write(&values, v->value);
            uint64_t w = values.len - value_off[nvalues];
            if (w > max_val_width)
                max_val_width = w;
            nvalues++;
        }
        if (i > 0) {
            labels[nlabels++] = v->label;
            bv_push(&tails_present, v->tail_len ? 1 : 0);
            if (v->tail_len) {
                tl[ntails].bytes = v->tail;
                tl[ntails].len = v->tail_len;
                ntails++;
            }
        }
    }
    bv_build_index(&louds);
    bv_build_index(&tails_present);
    bv_build_index(&terminals);

    /* Tail pool, suffix-merged per spec §8.1. */
    tail *distinct = malloc((ntails ? ntails : 1) * sizeof(tail));
    uint32_t ndistinct = 0;
    for (uint32_t i = 0; i < ntails; i++)
        distinct[ndistinct++] = tl[i];
    qsort(distinct, ndistinct, sizeof(tail), tail_cmp_reversed);
    /* drop duplicates */
    uint32_t uniq = 0;
    for (uint32_t i = 0; i < ndistinct; i++) {
        if (i && distinct[i].len == distinct[i - 1].len &&
            memcmp(distinct[i].bytes, distinct[i - 1].bytes, distinct[i].len) == 0)
            continue;
        distinct[uniq++] = distinct[i];
    }
    ndistinct = uniq;

    uint8_t *pool = NULL;
    uint64_t pool_len = 0, pool_cap = 0;
    int prev = -1;
    for (int i = (int)ndistinct - 1; i >= 0; i--) {
        if (prev >= 0 && is_suffix_of(&distinct[i], &distinct[prev])) {
            distinct[i].off = distinct[prev].off + distinct[prev].len - distinct[i].len;
            continue;
        }
        if (pool_len + distinct[i].len > pool_cap) {
            pool_cap = (pool_cap ? pool_cap * 2 : 4096) + distinct[i].len;
            pool = realloc(pool, pool_cap);
        }
        distinct[i].off = (uint32_t)pool_len;
        memcpy(pool + pool_len, distinct[i].bytes, distinct[i].len);
        pool_len += distinct[i].len;
        prev = i;
    }
    double build_s = (double)(clock() - t0) / CLOCKS_PER_SEC;

    /* Reference array widths, declared per file (spec §8). */
    uint32_t max_tail = 0;
    for (uint32_t i = 0; i < ndistinct; i++)
        if (distinct[i].len > max_tail)
            max_tail = distinct[i].len;
    int off_w = (pool_len <= 0xFFFFFFFFull) ? 4 : 8;
    int len_w = (max_tail <= 0xFF) ? 1 : (max_tail <= 0xFFFF ? 2 : 4);

    sizes s = {0};
    s.louds_bits = (louds.nbits + 7) / 8;
    s.louds_idx = bv_index_bytes(&louds);
    s.labels = nlabels;
    s.tail_presence_bits = (tails_present.nbits + 7) / 8;
    s.tail_presence_idx = bv_index_bytes(&tails_present);
    s.tail_refs = (uint64_t)ntails * (uint64_t)(off_w + len_w);
    s.tail_pool = pool_len;
    s.term_bits = (terminals.nbits + 7) / 8;
    s.term_idx = bv_index_bytes(&terminals);

    uint64_t total = 80 + s.louds_bits + s.louds_idx + s.labels + s.tail_presence_bits +
                     s.tail_presence_idx + s.tail_refs + s.tail_pool + s.term_bits + s.term_idx + 4;

    /* ── Verify every key through the serialised structures ──────────── */
    uint32_t *tail_of = malloc(sizeof(uint32_t) * (nnodes ? nnodes : 1));
    for (uint32_t i = 0; i < nnodes; i++)
        tail_of[i] = UINT32_MAX;
    {
        uint32_t seen = 0;
        for (uint32_t i = 1; i < nnodes; i++) {
            if (!bv_get(&tails_present, i - 1))
                continue;
            /* distinct[] is sorted by reversed bytes, so binary search it.
               A linear scan here is O(nodes x distinct) and made the tool
               unusable on corpora where every tail is unique. */
            node *v = bfs[i];
            tail probe = {v->tail, v->tail_len, 0};
            uint32_t lo = 0, hi = ndistinct;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (tail_cmp_reversed(&distinct[mid], &probe) < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            tail_of[i] = lo;
            seen++;
        }
        (void)seen;
    }

    t0 = clock();
    uint64_t found = 0;
    for (uint64_t wi = 0; wi < nwords; wi++) {
        const uint8_t *key = words[wi];
        size_t klen = wlens[wi], i = 0;
        uint64_t v = 0;
        bool ok = true;
        while (i < klen) {
            uint64_t cb = (v == 0) ? 0 : bv_select0(&louds, v - 1) + 1;
            uint64_t cc = bv_select0(&louds, v) - cb;
            /* binary search over the child labels */
            uint64_t lo = 0, hi = cc, hit = cc;
            while (lo < hi) {
                uint64_t mid = (lo + hi) / 2;
                uint64_t cid = bv_rank1(&louds, cb + mid) + 1;
                uint8_t lb = labels[cid - 1];
                if (lb < key[i])
                    lo = mid + 1;
                else {
                    hi = mid;
                    if (lb == key[i])
                        hit = mid;
                }
            }
            if (hit == cc) {
                ok = false;
                break;
            }
            uint64_t cid = bv_rank1(&louds, cb + hit) + 1;
            i++;
            if (bv_get(&tails_present, cid - 1)) {
                const tail *t = &distinct[tail_of[cid]];
                if (i + t->len > klen || memcmp(key + i, pool + t->off, t->len) != 0) {
                    ok = false;
                    break;
                }
                i += t->len;
            }
            v = cid;
        }
        if (ok && bv_get(&terminals, v))
            found++;
    }
    double lookup_us = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / (double)nwords;

    /* ── Huffman cost of the byte-oriented sections ──────────────────── */
    uint64_t lf[256] = {0}, pf[256] = {0}, bothf[256] = {0};
    for (uint64_t i = 0; i < nlabels; i++) {
        lf[labels[i]]++;
        bothf[labels[i]]++;
    }
    for (uint64_t i = 0; i < pool_len; i++) {
        pf[pool[i]]++;
        bothf[pool[i]]++;
    }
    uint64_t lsym = 0, psym = 0;
    double lh = huffman_bytes(lf, &lsym);
    double ph = huffman_bytes(pf, &psym);
    double bh = huffman_bytes(bothf, NULL);

    /* ── Values: sizes and the cost of reaching one ──────────────────── */

    /* Sampled offset index, spec §9.2: one offset every 32 values. */
    const uint64_t SAMPLE = 32;
    uint64_t nsamples = (nvalues + SAMPLE - 1) / SAMPLE;
    uint64_t *sample = malloc(sizeof(uint64_t) * (nsamples ? nsamples : 1));
    for (uint64_t i = 0; i < nsamples; i++)
        sample[i] = value_off[i * SAMPLE];
    int samp_w = bits_needed(values.len) <= 32 ? 4 : 8;
    uint64_t sampled_idx_bytes = nsamples * (uint64_t)samp_w;

    /* Fixed-width alternative: pad every value to the widest. */
    uint64_t fixed_bytes = nvalues * max_val_width;

    /* Every key's value, fetched the way a reader would: ordinal by rank over
       the terminal bitmap, then at most 31 varint skips from a sample. */
    t0 = clock();
    uint64_t vmismatch = 0;
    for (uint64_t wi = 0; wi < nwords; wi++) {
        const uint8_t *key = words[wi];
        size_t klen = wlens[wi], i = 0;
        uint64_t v = 0;
        bool ok = true;
        while (i < klen) {
            uint64_t cb = (v == 0) ? 0 : bv_select0(&louds, v - 1) + 1;
            uint64_t cc = bv_select0(&louds, v) - cb;
            uint64_t lo = 0, hi = cc, hit = cc;
            while (lo < hi) {
                uint64_t mid = (lo + hi) / 2;
                uint64_t cid = bv_rank1(&louds, cb + mid) + 1;
                uint8_t lb = labels[cid - 1];
                if (lb < key[i])
                    lo = mid + 1;
                else {
                    hi = mid;
                    if (lb == key[i])
                        hit = mid;
                }
            }
            if (hit == cc) {
                ok = false;
                break;
            }
            uint64_t cid = bv_rank1(&louds, cb + hit) + 1;
            i++;
            if (bv_get(&tails_present, cid - 1)) {
                const tail *t = &distinct[tail_of[cid]];
                if (i + t->len > klen || memcmp(key + i, pool + t->off, t->len) != 0) {
                    ok = false;
                    break;
                }
                i += t->len;
            }
            v = cid;
        }
        if (!ok || !bv_get(&terminals, v)) {
            vmismatch++;
            continue;
        }
        uint64_t ordinal = bv_rank1(&terminals, v);
        uint64_t pos = sample[ordinal / SAMPLE];
        for (uint64_t k = 0; k < ordinal % SAMPLE; k++)
            (void)leb128_read(values.buf, &pos);
        uint64_t got = leb128_read(values.buf, &pos);

        /* A repeated key is stored once, last write winning, so the value is
           the index of its *final* occurrence rather than this one. Check
           that the winning index names the same key -- corpora with repeated
           lines (enwik9 has 103k of them) are the normal case, not an
           error. */
        if (got >= nwords || wlens[got] != wlens[wi] ||
            memcmp(words[got], words[wi], wlens[wi]) != 0)
            vmismatch++;
    }
    double lookup_val_us = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / (double)nwords;

    /* ── Alternative encodings, measured ─────────────────────────────── */

    /* The spec as written stores a (pool offset, length) pair per tail
       *occurrence*. With 696 distinct tails behind 7,056 occurrences that
       spends 5 bytes to point at a 2 KB pool -- the references cost more
       than the data. Two cheaper encodings, measured rather than assumed. */

    uint64_t alpha[256] = {0};
    uint32_t nalpha = 0;
    for (uint64_t i = 0; i < nlabels; i++)
        if (!alpha[labels[i]]++)
            nalpha++;

    int tail_idx_bits = bits_needed(ndistinct);
    int pool_off_bits = bits_needed(pool_len);
    int tail_len_bits = bits_needed(max_tail + 1);
    int label_bits = bits_needed(nalpha);

    /* B: reference the distinct tail by index; a small dictionary maps the
       index to (offset, length). */
    uint64_t refs_b = ((uint64_t)ntails * (uint64_t)tail_idx_bits + 7) / 8;
    uint64_t dict_b = ((uint64_t)ndistinct * (uint64_t)(pool_off_bits + tail_len_bits) + 7) / 8;
    uint64_t total_b = total - s.tail_refs + refs_b + dict_b;

    /* C: B, plus labels coded at the width the alphabet actually needs
       rather than a whole byte each. Random access is preserved: a label is
       still at a computable bit offset. */
    uint64_t labels_c = ((uint64_t)nlabels * (uint64_t)label_bits + 7) / 8 + nalpha;
    uint64_t total_c = total_b - s.labels + labels_c;

    /* D: C, plus the tail pool coded in that same alphabet. Tail bytes are
       drawn from the same key text as labels, so storing them raw spends 8
       bits where the alphabet needs 5 -- and on path-like corpora the pool
       is more than half the file. A pool byte stays at a computable bit
       offset, so comparing a tail against a key is still a bounded loop. */
    uint64_t pool_d = ((uint64_t)s.tail_pool * (uint64_t)label_bits + 7) / 8;
    uint64_t total_d = total_c - s.tail_pool + pool_d;

    printf("## Encoding variants\n\n");
    printf(
        "| Variant | Tail refs | Labels | Total | %% of raw keys |\n|---|---:|---:|---:|---:|\n");
    printf("| A: spec as written | %llu | %llu | %llu | %.0f%% |\n",
           (unsigned long long)s.tail_refs, (unsigned long long)s.labels, (unsigned long long)total,
           100.0 * (double)total / (double)raw_bytes);
    printf("| B: tail index (%d bits) + dictionary | %llu | %llu | %llu | %.0f%% |\n",
           tail_idx_bits, (unsigned long long)(refs_b + dict_b), (unsigned long long)s.labels,
           (unsigned long long)total_b, 100.0 * (double)total_b / (double)raw_bytes);
    printf("| C: B + %d-bit labels (%u symbols) | %llu | %llu | %llu | %.0f%% |\n", label_bits,
           nalpha, (unsigned long long)(refs_b + dict_b), (unsigned long long)labels_c,
           (unsigned long long)total_c, 100.0 * (double)total_c / (double)raw_bytes);
    printf("| D: C + %d-bit tail pool (%llu -> %llu B) | %llu | %llu | %llu | %.0f%% |\n",
           label_bits, (unsigned long long)s.tail_pool, (unsigned long long)pool_d,
           (unsigned long long)(refs_b + dict_b), (unsigned long long)labels_c,
           (unsigned long long)total_d, 100.0 * (double)total_d / (double)raw_bytes);
    printf("\n");

    /* ── Report ──────────────────────────────────────────────────────── */
    printf("# TriePack v2 prototype\n\n");
    /* Naive baselines, so the compression ratio is against something
       concrete rather than against the previous version of ourselves.

       "JSON object" is the size of {"key":N,...} with the same values: two
       quotes and a colon and a comma per entry, plus the decimal value. */
    uint64_t json_bytes = 2; /* { } */
    for (uint64_t i = 0; i < nwords; i++) {
        uint64_t v = i, digits = 1;
        while (v >= 10) {
            v /= 10;
            digits++;
        }
        json_bytes += wlens[i] + 2 /* quotes */ + 1 /* colon */ + digits;
        if (i + 1 < nwords)
            json_bytes++; /* comma */
    }
    /* A length-prefixed flat list of keys: the smallest honest "just store
       them" encoding, one LEB128 length plus the bytes. */
    uint64_t flat_bytes = 0;
    for (uint64_t i = 0; i < nwords; i++)
        flat_bytes += wlens[i] + (wlens[i] < 128 ? 1 : 2);

    printf("input                %s\n", path);
    printf("keys                 %llu\n", (unsigned long long)nwords);
    printf("\n| Uncompressed baseline | Bytes |\n|---|---:|\n");
    printf("| input file as-is | %llu |\n", (unsigned long long)file_bytes);
    printf("| raw key bytes (no separators) | %llu |\n", (unsigned long long)raw_bytes);
    printf("| flat list, length-prefixed | %llu |\n", (unsigned long long)flat_bytes);
    printf("| JSON object with the values | %llu |\n", (unsigned long long)json_bytes);
    printf("\n");
    printf("trie nodes           %u  (after radix compression)\n", nnodes);
    printf("distinct tails       %u of %u occurrences\n", ndistinct, ntails);
    printf("tail widths          off_w=%d len_w=%d (max tail %u)\n", off_w, len_w, max_tail);
    printf("build                %.2f s\n\n", build_s);

    printf("| Section | Bytes | %% |\n|---|---:|---:|\n");
#define ROW(name, val)                                                  \
    printf("| %s | %llu | %.1f%% |\n", name, (unsigned long long)(val), \
           100.0 * (double)(val) / (double)total)
    ROW("header", 80);
    ROW("LOUDS bits", s.louds_bits);
    ROW("LOUDS rank index", s.louds_idx);
    ROW("labels", s.labels);
    ROW("tail presence bits", s.tail_presence_bits);
    ROW("tail presence index", s.tail_presence_idx);
    ROW("tail refs", s.tail_refs);
    ROW("tail pool", s.tail_pool);
    ROW("terminal bits", s.term_bits);
    ROW("terminal rank index", s.term_idx);
    ROW("CRC", 4);
#undef ROW
    printf("| **total** | **%llu** | |\n\n", (unsigned long long)total);

    printf("keys verified        %llu / %llu %s\n", (unsigned long long)found,
           (unsigned long long)nwords, found == nwords ? "OK" : "** MISMATCH **");
    printf("lookup               %.2f us/key (through serialised bytes)\n\n", lookup_us);

    /* ── Values ──────────────────────────────────────────────────────── */

    uint64_t keys_only_c = total_c;
    uint64_t with_values = total_c + values.len + sampled_idx_bytes;

    printf("## With values (one uint per key)\n\n");
    printf("| Section | Bytes |\n|---|---:|\n");
    printf("| keys only (variant C) | %llu |\n", (unsigned long long)keys_only_c);
    printf("| value store (LEB128, terminal order) | %llu |\n", (unsigned long long)values.len);
    printf("| sampled offset index (1 per %llu) | %llu |\n", (unsigned long long)SAMPLE,
           (unsigned long long)sampled_idx_bytes);
    printf("| **total with values** | **%llu** |\n", (unsigned long long)with_values);
    printf("| fixed-width alternative (%llu B/value) | %llu |\n", (unsigned long long)max_val_width,
           (unsigned long long)(total_c + fixed_bytes));
    printf("\n");
    printf("values verified      %llu / %llu %s\n", (unsigned long long)(nwords - vmismatch),
           (unsigned long long)nwords, vmismatch == 0 ? "OK" : "** MISMATCH **");
    printf("lookup + value       %.2f us/key  (vs %.2f us/key keys-only)\n\n", lookup_val_us,
           lookup_us);

    printf("## Second-pass Huffman over the byte sections\n\n");
    printf("| Section | Raw | Huffman | Saving | Distinct bytes |\n|---|---:|---:|---:|---:|\n");
    printf("| labels | %llu | %.0f | %.1f%% | %llu |\n", (unsigned long long)nlabels, lh,
           nlabels ? 100.0 * (1.0 - lh / (double)nlabels) : 0.0, (unsigned long long)lsym);
    printf("| tail pool | %llu | %.0f | %.1f%% | %llu |\n", (unsigned long long)pool_len, ph,
           pool_len ? 100.0 * (1.0 - ph / (double)pool_len) : 0.0, (unsigned long long)psym);
    double raw_both = (double)(nlabels + pool_len);
    printf("| both, shared tree | %.0f | %.0f | %.1f%% | |\n", raw_both, bh,
           raw_both ? 100.0 * (1.0 - bh / raw_both) : 0.0);
    /* The honest comparison is against variant C, where labels are already
       coded at the alphabet width. Most of Huffman's apparent saving over
       raw bytes is just "8 bits -> %d bits"; what is left is the gain from
       frequency skew, and it costs random access to collect. */
    double labels_fixed = (double)labels_c - (double)nalpha;
    double pool_raw = (double)pool_len;
    double c_sections = labels_fixed + pool_raw;
    double c_huff = lh + ph;
    printf("\n");
    printf("| Compared against variant C | Bytes |\n|---|---:|\n");
    printf("| labels at %d bits + tail pool raw | %.0f |\n", label_bits, c_sections);
    printf("| the same, Huffman-coded | %.0f |\n", c_huff);
    printf("| further saving | %.1f%% |\n", 100.0 * (1.0 - c_huff / c_sections));
    double huff_total = (double)total_c - (c_sections - c_huff);

    printf("\n## Compression ratios\n\n");
    printf("| Against | Baseline | Keys only | +values | x (keys) | x (+values) |\n");
    printf("|---|---:|---:|---:|---:|---:|\n");
#define RATIO(label, base)                                                                     \
    printf("| %s | %llu | %llu | %llu | %.2fx | %.2fx |\n", label, (unsigned long long)(base), \
           (unsigned long long)total_c, (unsigned long long)with_values,                       \
           (double)(base) / (double)total_c, (double)(base) / (double)with_values)
    RATIO("input file as-is", (uint64_t)file_bytes);
    RATIO("raw key bytes", raw_bytes);
    RATIO("flat list, length-prefixed", flat_bytes);
    RATIO("JSON object with values", json_bytes);
#undef RATIO
    printf("\nwith optional Huffman  %.0f bytes keys-only (%.2fx the input file)\n", huff_total,
           (double)file_bytes / huff_total);
    /* ── Emit the real bytes, when asked ─────────────────────────────── */

    /* argv[2] names a file to write the variant D payload to: every section
       packed as §3.1 specifies, so downstream tools have actual bytes rather
       than a computed size. The header, per-section alignment and CRC are
       omitted -- they are a fixed ~100 bytes and would only blur a
       compressibility measurement. The size is checked against the computed
       total so the two cannot drift. */
    if (argc > 2) {
        /* Map each byte value to its alphabet code. */
        uint32_t code_of[256];
        uint32_t next_code = 0;
        for (int b = 0; b < 256; b++)
            code_of[b] = alpha[b] ? next_code++ : 0;

        bitwriter w = {0};
        bw_bytes(&w, louds.bits, (louds.nbits + 7) / 8);
        for (int b = 0; b < 256; b++)
            if (alpha[b])
                bw_put(&w, (uint64_t)b, 8);
        for (uint64_t i = 0; i < nlabels; i++)
            bw_put(&w, code_of[labels[i]], label_bits);
        bw_bytes(&w, tails_present.bits, (tails_present.nbits + 7) / 8);
        for (uint32_t i = 0; i < ntails; i++) {
            /* reference: index of this tail in the distinct table */
            tail probe = {tl[i].bytes, tl[i].len, 0};
            uint32_t lo = 0, hi = ndistinct;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (tail_cmp_reversed(&distinct[mid], &probe) < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            bw_put(&w, lo, tail_idx_bits);
        }
        for (uint32_t i = 0; i < ndistinct; i++) {
            bw_put(&w, distinct[i].off, pool_off_bits);
            bw_put(&w, distinct[i].len, tail_len_bits);
        }
        for (uint64_t i = 0; i < pool_len; i++)
            bw_put(&w, code_of[pool[i]], label_bits);
        bw_bytes(&w, terminals.bits, (terminals.nbits + 7) / 8);
        /* Keys only, matching the variant D figure. The value store is
           measured separately because a key-list corpus has no values and a
           JSON corpus supplies its own. */

        /* Rank indices, which are stored in the file rather than rebuilt. */
        uint64_t idx_bytes =
            bv_index_bytes(&louds) + bv_index_bytes(&tails_present) + bv_index_bytes(&terminals);
        for (uint64_t i = 0; i < idx_bytes; i++)
            bw_put(&w, 0, 8); /* placeholder: size is what matters here */

        uint64_t emitted = (w.nbits + 7) / 8 + 80 + 4; /* header + CRC */
        FILE *out = fopen(argv[2], "wb");
        if (out) {
            fwrite(w.buf, 1, (size_t)((w.nbits + 7) / 8), out);
            fclose(out);
            /* The analytic figure rounds each section up to a byte
               independently, so it reads a little high; anything beyond that
               means the writer and the model disagree. */
            long drift = (long)emitted - (long)total_d;
            printf("emitted              %s (%llu bytes, %+ld vs computed %llu)\n", argv[2],
                   (unsigned long long)emitted, drift, (unsigned long long)total_d);
            if (drift > 64 || drift < -64) {
                fprintf(stderr, "writer disagrees with the size model by %ld bytes\n", drift);
                return 1;
            }
        }
        free(w.buf);
    }

    /* Free everything: this runs in CI under LeakSanitizer. A build-time
       tool may allocate freely (North Star §4.1) but it should not leak. */
    for (uint64_t i = 0; i < nwords; i++)
        free(words[i]);
    free(words);
    free(wlens);
    trie_free(root);
    free(labels);
    free(tl);
    free(distinct);
    free(pool);
    free(bfs);
    free(tail_of);
    free(values.buf);
    free(value_off);
    free(sample);
    free(louds.bits);
    free(louds.super);
    free(louds.block);
    free(tails_present.bits);
    free(tails_present.super);
    free(tails_present.block);
    free(terminals.bits);
    free(terminals.super);
    free(terminals.block);

    return (found == nwords && vmismatch == 0) ? 0 : 1;
}
