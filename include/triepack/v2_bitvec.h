/**
 * @file v2_bitvec.h
 * @brief Ranked bit vector and LOUDS navigation — format v2 §5 and §6.
 *
 * One primitive, used three times: the tree, the terminal map and the tail
 * presence map are all ranked bit vectors. Specifying and testing it once is
 * the single biggest lever on the cost of ten implementations
 * (North Star §5).
 *
 * This is v2 groundwork. It is not used by the v1 encoder or decoder and
 * does not appear in a `.trp` file yet; see
 * docs/internals/format-spec-v2.md.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_V2_BITVEC_H
#define TRIEPACK_V2_BITVEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque ranked bit vector. */
typedef struct tp2_bitvec tp2_bitvec;

/**
 * @brief Build a ranked bit vector over @p nbits bits of @p bits.
 *
 * Bit @c i is `bits[i>>3] & (0x80 >> (i&7))` — most significant bit first.
 * The bits are copied; the caller keeps ownership of @p bits. @p bits may be
 * NULL when @p nbits is 0.
 *
 * The rank index is built here rather than at read time, because in a `.trp`
 * file it is stored alongside the bits (North Star §4.3).
 *
 * @return NULL on allocation failure.
 */
tp2_bitvec *tp2_bv_create(const uint8_t *bits, uint64_t nbits);

/** Destroy and set the pointer to NULL. Safe on NULL and on an already
 *  destroyed vector. */
void tp2_bv_destroy(tp2_bitvec **bv);

/** Number of bits in the vector. */
uint64_t tp2_bv_size(const tp2_bitvec *bv);

/** Bit at @p i, 0 or 1. @p i must be < size. */
int tp2_bv_get(const tp2_bitvec *bv, uint64_t i);

/** Number of 1 bits **strictly before** @p i. `rank1(0)` is 0; `rank1(size)`
 *  is the population count. */
uint64_t tp2_bv_rank1(const tp2_bitvec *bv, uint64_t i);

/** Number of 0 bits strictly before @p i, i.e. `i - rank1(i)`. */
uint64_t tp2_bv_rank0(const tp2_bitvec *bv, uint64_t i);

/** Position of the @p k-th 1 bit, counting from 0. Returns `size` when there
 *  is no such bit — a defined answer, because ten implementations cannot
 *  agree on undefined behaviour. */
uint64_t tp2_bv_select1(const tp2_bitvec *bv, uint64_t k);

/** Position of the @p k-th 0 bit, counting from 0. Returns `size` when there
 *  is no such bit. */
uint64_t tp2_bv_select0(const tp2_bitvec *bv, uint64_t k);

/** Bytes the rank index occupies, which is part of the format's stored cost:
 *  64 bits per 2048 plus 16 bits per 256, about 9.4% of the bit vector. */
uint64_t tp2_bv_index_bytes(const tp2_bitvec *bv);

/* ── LOUDS navigation (§6.2) ──────────────────────────────────────────
 *
 * Nodes are numbered 0..n-1 in breadth-first order, node 0 is the root, and
 * the bit vector is `1^d 0` per node in that order. No super-root prefix.
 */

/** Bit position where node @p v's children begin. */
uint64_t tp2_louds_child_begin(const tp2_bitvec *bv, uint64_t v);

/** Number of children of node @p v. */
uint64_t tp2_louds_child_count(const tp2_bitvec *bv, uint64_t v);

/** BFS index of the @p k-th child of node @p v, `0 <= k < child_count(v)`.
 *  Children are contiguous, so `child(v, k+1) == child(v, k) + 1`. */
uint64_t tp2_louds_child(const tp2_bitvec *bv, uint64_t v, uint64_t k);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_V2_BITVEC_H */
