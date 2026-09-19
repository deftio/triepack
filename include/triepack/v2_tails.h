/**
 * @file v2_tails.h
 * @brief Suffix-merged tail pool — format v2 §8.
 *
 * A tail is the run of bytes on a trie edge after its first byte: the radix
 * compression of a non-branching chain. Storing every occurrence in full is
 * what v1 did, because the suffix table it reserved space for was never
 * built. Here the distinct tails are merged so that a tail which is a suffix
 * of another costs no bytes at all — "ing" lives inside "string".
 *
 * §8.1 is normative and this implements it exactly. The procedure is
 * specified rather than described because ten implementations pursuing a
 * goal produce ten different files (North Star §4.2).
 *
 * This is v2 groundwork and does not appear in a `.trp` file yet; see
 * docs/internals/format-spec-v2.md.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#ifndef TRIEPACK_V2_TAILS_H
#define TRIEPACK_V2_TAILS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Where a tail lives in the pool. */
typedef struct tp2_tail_ref {
    uint64_t offset; /**< byte offset into the pool */
    uint32_t length; /**< length in bytes */
} tp2_tail_ref;

/** Opaque built pool. */
typedef struct tp2_tailpool tp2_tailpool;

/**
 * @brief Build a suffix-merged pool from @p count tails.
 *
 * @p tails and @p lens are parallel arrays; tails are arbitrary bytes and may
 * contain NUL. Both may be NULL when @p count is 0. The bytes are copied.
 *
 * The result does not depend on the order the tails are given in: §8.1 sorts
 * by reversed byte sequence before emitting, which is what makes output
 * byte-identical across implementations.
 *
 * @return NULL on allocation failure.
 */
tp2_tailpool *tp2_tailpool_build(const uint8_t *const *tails, const uint32_t *lens, uint64_t count);

/** Destroy and set the pointer to NULL. Safe on NULL. */
void tp2_tailpool_destroy(tp2_tailpool **tp);

/** The pool bytes. Not NUL-terminated; use tp2_tailpool_size(). */
const uint8_t *tp2_tailpool_bytes(const tp2_tailpool *tp);

/** Size of the pool in bytes. */
uint64_t tp2_tailpool_size(const tp2_tailpool *tp);

/** Number of distinct tails, which is what the reference width is sized from
 *  (§8: `tail_index_bits = ceil(log2(distinct))`). */
uint64_t tp2_tailpool_distinct(const tp2_tailpool *tp);

/** Reference for the @p i-th input tail, in the order they were given. */
tp2_tail_ref tp2_tailpool_ref(const tp2_tailpool *tp, uint64_t i);

#ifdef __cplusplus
}
#endif

#endif /* TRIEPACK_V2_TAILS_H */
