/**
 * @file v2_bitvec.c
 * @brief Ranked bit vector and LOUDS navigation — format v2 §5 and §6.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/v2_bitvec.h"

#include <stdlib.h>
#include <string.h>

/* §5.1 layout constants. Conventional rather than measured; see the open
   questions in the format spec. */
#define TP2_SUPER_BITS 2048
#define TP2_BLOCK_BITS 256

struct tp2_bitvec {
    uint8_t *bits;
    uint64_t nbits;
    uint64_t *super; /* rank1 of everything before this superblock */
    uint16_t *block; /* rank1 within the superblock, before this block */
    uint64_t nsuper, nblock;
};

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

tp2_bitvec *tp2_bv_create(const uint8_t *bits, uint64_t nbits)
{
    tp2_bitvec *bv = calloc(1, sizeof(*bv));
    if (!bv)
        return NULL;

    bv->nbits = nbits;
    size_t nbytes = (size_t)((nbits + 7) / 8);
    /* One spare byte so a word-at-a-time read near the end stays in bounds. */
    bv->bits = calloc(nbytes + 8, 1);
    if (!bv->bits) {
        free(bv);
        return NULL;
    }
    if (bits && nbytes)
        memcpy(bv->bits, bits, nbytes);

    /* Trailing bits of the final byte are not part of the vector and must
       not be counted; zero them so rank never sees them. */
    if (nbits % 8)
        bv->bits[nbytes - 1] &= (uint8_t)(0xFF << (8 - (nbits % 8)));

    bv->nsuper = (nbits + TP2_SUPER_BITS - 1) / TP2_SUPER_BITS + 1;
    bv->nblock = (nbits + TP2_BLOCK_BITS - 1) / TP2_BLOCK_BITS + 1;
    bv->super = calloc((size_t)bv->nsuper, sizeof(*bv->super));
    bv->block = calloc((size_t)bv->nblock, sizeof(*bv->block));
    if (!bv->super || !bv->block) {
        tp2_bv_destroy(&bv);
        return NULL;
    }

    /* Run to <= nbits so the final entries are produced by the same rule as
       every other one. Writing them afterwards as separate "terminators"
       gives them different semantics, and where a superblock boundary lands
       exactly at the end both entries hold the same count and rank adds it
       twice -- at n = 2048, rank1(2048) returned 1366 instead of 683. */
    uint64_t total = 0, within = 0;
    for (uint64_t i = 0; i <= nbits; i++) {
        if (i % TP2_SUPER_BITS == 0) {
            bv->super[i / TP2_SUPER_BITS] = total;
            within = 0;
        }
        if (i % TP2_BLOCK_BITS == 0)
            bv->block[i / TP2_BLOCK_BITS] = (uint16_t)within;
        if (i < nbits && tp2_bv_get(bv, i)) {
            total++;
            within++;
        }
    }
    return bv;
}

void tp2_bv_destroy(tp2_bitvec **bv)
{
    if (!bv || !*bv)
        return;
    free((*bv)->bits);
    free((*bv)->super);
    free((*bv)->block);
    free(*bv);
    *bv = NULL;
}

uint64_t tp2_bv_size(const tp2_bitvec *bv)
{
    return bv ? bv->nbits : 0;
}

int tp2_bv_get(const tp2_bitvec *bv, uint64_t i)
{
    return (bv->bits[i >> 3] >> (7 - (i & 7))) & 1;
}

uint64_t tp2_bv_rank1(const tp2_bitvec *bv, uint64_t i)
{
    if (!bv || i == 0)
        return 0;
    if (i > bv->nbits)
        i = bv->nbits;

    uint64_t c = bv->super[i / TP2_SUPER_BITS] + bv->block[i / TP2_BLOCK_BITS];
    uint64_t j = (i / TP2_BLOCK_BITS) * TP2_BLOCK_BITS;

    /* Whole 64-bit words, then whole bytes, then the ragged tail. */
    for (; j + 64 <= i; j += 64) {
        uint64_t w = 0;
        for (int k = 0; k < 8; k++)
            w = (w << 8) | bv->bits[(j >> 3) + (uint64_t)k];
        c += (uint64_t)popcount64(w);
    }
    for (; j + 8 <= i; j += 8)
        c += (uint64_t)popcount64((uint64_t)bv->bits[j >> 3]);
    for (; j < i; j++)
        c += (uint64_t)tp2_bv_get(bv, j);
    return c;
}

uint64_t tp2_bv_rank0(const tp2_bitvec *bv, uint64_t i)
{
    if (!bv)
        return 0;
    if (i > bv->nbits)
        i = bv->nbits;
    return i - tp2_bv_rank1(bv, i);
}

/* §5.3: binary search over the rank index, no extra stored structure. The
   predicate is monotonic in i, which is what makes the search valid. */
static uint64_t bv_select(const tp2_bitvec *bv, uint64_t k, int want_one)
{
    if (!bv || bv->nbits == 0)
        return 0;
    uint64_t available = want_one ? tp2_bv_rank1(bv, bv->nbits) : tp2_bv_rank0(bv, bv->nbits);
    if (k >= available)
        return bv->nbits; /* defined "not found" */

    uint64_t lo = 0, hi = bv->nbits;
    while (lo < hi) {
        uint64_t mid = lo + (hi - lo) / 2;
        uint64_t seen = want_one ? tp2_bv_rank1(bv, mid + 1) : tp2_bv_rank0(bv, mid + 1);
        if (seen <= k)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

uint64_t tp2_bv_select1(const tp2_bitvec *bv, uint64_t k)
{
    return bv_select(bv, k, 1);
}

uint64_t tp2_bv_select0(const tp2_bitvec *bv, uint64_t k)
{
    return bv_select(bv, k, 0);
}

uint64_t tp2_bv_index_bytes(const tp2_bitvec *bv)
{
    if (!bv)
        return 0;
    return bv->nsuper * sizeof(uint64_t) + bv->nblock * sizeof(uint16_t);
}

/* ── LOUDS (§6.2) ────────────────────────────────────────────────────── */

uint64_t tp2_louds_child_begin(const tp2_bitvec *bv, uint64_t v)
{
    return (v == 0) ? 0 : tp2_bv_select0(bv, v - 1) + 1;
}

uint64_t tp2_louds_child_count(const tp2_bitvec *bv, uint64_t v)
{
    uint64_t begin = tp2_louds_child_begin(bv, v);
    uint64_t end = tp2_bv_select0(bv, v);
    return (end > begin) ? end - begin : 0;
}

uint64_t tp2_louds_child(const tp2_bitvec *bv, uint64_t v, uint64_t k)
{
    return tp2_bv_rank1(bv, tp2_louds_child_begin(bv, v) + k) + 1;
}
