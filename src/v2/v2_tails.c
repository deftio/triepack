/**
 * @file v2_tails.c
 * @brief Suffix-merged tail pool — format v2 §8.1.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause — see LICENSE.txt
 */

#include "triepack/v2_tails.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    uint32_t len;
    uint64_t offset;      /* assigned during emit */
    uint64_t seq;         /* index in the caller's array, to map refs back */
    uint64_t distinct_of; /* index into the distinct table */
} entry;

struct tp2_tailpool {
    uint8_t *pool;
    uint64_t pool_len;
    uint64_t distinct;
    tp2_tail_ref *refs; /* one per input tail, in input order */
    uint64_t count;
};

/* §8.1 step 2: order by reversed byte sequence, ascending — compare the last
   byte first. Bytes compare as *unsigned*; a signed char here would sort 0x80
   below 0x01 and silently reorder the pool for any non-ASCII data. */
static int cmp_reversed(const void *a, const void *b)
{
    const entry *x = a, *y = b;
    uint32_t i = 0;
    while (i < x->len && i < y->len) {
        uint8_t xb = x->bytes[x->len - 1 - i];
        uint8_t yb = y->bytes[y->len - 1 - i];
        if (xb != yb)
            return xb < yb ? -1 : 1;
        i++;
    }
    if (x->len != y->len)
        return x->len < y->len ? -1 : 1;
    /* Equal content: order by input position so the sort is total and the
       result cannot depend on the sort implementation's stability. */
    return (x->seq < y->seq) ? -1 : (x->seq > y->seq);
}

static int same_tail(const entry *a, const entry *b)
{
    return a->len == b->len && memcmp(a->bytes, b->bytes, a->len) == 0;
}

/* Is `s` a proper suffix of `big`? */
static int is_proper_suffix(const entry *s, const entry *big)
{
    return s->len < big->len && memcmp(big->bytes + big->len - s->len, s->bytes, s->len) == 0;
}

tp2_tailpool *tp2_tailpool_build(const uint8_t *const *tails, const uint32_t *lens, uint64_t count)
{
    tp2_tailpool *tp = calloc(1, sizeof(*tp));
    if (!tp)
        return NULL;
    tp->count = count;
    if (count == 0) {
        tp->pool = calloc(1, 1);
        return tp->pool ? tp : (free(tp), NULL);
    }

    tp->refs = calloc((size_t)count, sizeof(*tp->refs));
    entry *sorted = calloc((size_t)count, sizeof(*sorted));
    if (!tp->refs || !sorted) {
        free(sorted);
        tp2_tailpool_destroy(&tp);
        return NULL;
    }

    for (uint64_t i = 0; i < count; i++) {
        sorted[i].bytes = tails[i];
        sorted[i].len = lens[i];
        sorted[i].seq = i;
    }

    /* Step 2: sort by reversed bytes. */
    qsort(sorted, (size_t)count, sizeof(*sorted), cmp_reversed);

    /* Step 1 (completed): collapse to the *distinct* tails. Equal tails are
       adjacent after the sort, so this is a single pass. */
    uint64_t ndistinct = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (i > 0 && same_tail(&sorted[i], &sorted[i - 1]))
            sorted[i].distinct_of = ndistinct - 1;
        else
            sorted[i].distinct_of = ndistinct++;
    }

    entry *distinct = calloc((size_t)ndistinct, sizeof(*distinct));
    if (!distinct) {
        free(sorted);
        tp2_tailpool_destroy(&tp);
        return NULL;
    }
    for (uint64_t i = 0; i < count; i++)
        distinct[sorted[i].distinct_of] = sorted[i];

    /* Worst case the pool is every distinct tail concatenated. */
    uint64_t cap = 0;
    for (uint64_t i = 0; i < ndistinct; i++)
        cap += distinct[i].len;
    tp->pool = calloc((size_t)(cap ? cap : 1), 1);
    if (!tp->pool) {
        free(sorted);
        free(distinct);
        tp2_tailpool_destroy(&tp);
        return NULL;
    }

    /* Step 3: walk from last to first, tracking the most recently emitted
       tail. A tail that is a proper suffix of it costs no bytes. */
    int64_t prev = -1;
    for (int64_t i = (int64_t)ndistinct - 1; i >= 0; i--) {
        if (prev >= 0 && is_proper_suffix(&distinct[i], &distinct[prev])) {
            distinct[i].offset = distinct[prev].offset + distinct[prev].len - distinct[i].len;
            continue;
        }
        distinct[i].offset = tp->pool_len;
        memcpy(tp->pool + tp->pool_len, distinct[i].bytes, distinct[i].len);
        tp->pool_len += distinct[i].len;
        prev = i;
    }

    /* Map every input tail back to its distinct entry's placement. */
    for (uint64_t i = 0; i < count; i++) {
        const entry *d = &distinct[sorted[i].distinct_of];
        tp->refs[sorted[i].seq].offset = d->offset;
        tp->refs[sorted[i].seq].length = d->len;
    }

    tp->distinct = ndistinct;
    free(sorted);
    free(distinct);
    return tp;
}

void tp2_tailpool_destroy(tp2_tailpool **tp)
{
    if (!tp || !*tp)
        return;
    free((*tp)->pool);
    free((*tp)->refs);
    free(*tp);
    *tp = NULL;
}

const uint8_t *tp2_tailpool_bytes(const tp2_tailpool *tp)
{
    return tp ? tp->pool : NULL;
}

uint64_t tp2_tailpool_size(const tp2_tailpool *tp)
{
    return tp ? tp->pool_len : 0;
}

uint64_t tp2_tailpool_distinct(const tp2_tailpool *tp)
{
    return tp ? tp->distinct : 0;
}

tp2_tail_ref tp2_tailpool_ref(const tp2_tailpool *tp, uint64_t i)
{
    tp2_tail_ref none = {0, 0};
    if (!tp || !tp->refs || i >= tp->count)
        return none;
    return tp->refs[i];
}
