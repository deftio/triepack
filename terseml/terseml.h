/**
 * @file terseml.h
 * @brief terseml encoder and decoder -- C99.
 *
 * Implements GRAMMAR.md. Where this and the grammar disagree, the grammar is
 * right and this is a bug.
 *
 * Decoding is arena-backed: one document owns one allocation chain, freed in
 * a single call. Text that contains no escapes is not copied at all -- the
 * node points into the caller's buffer, which must outlive the document.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

#ifndef TERSEML_H
#define TERSEML_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Version. terseml tracks the TriePack release version for now, because one
 * number across a single repository is less confusing than two. It is not a
 * TriePack component and does not link one; when it is spun out into its own
 * repository it will start versioning on its own and these will diverge.
 *
 * The grammar has its own version, and it is not this one -- see GRAMMAR.md.
 */
#define TSML_VERSION "2.0.0"

#define TSML_MAX_DEPTH 256

/** Reserved tags (GRAMMAR.md §4.3, §9). */
#define TSML_TAG_TEXT    "#text"
#define TSML_TAG_COMMENT "#comment"
#define TSML_TAG_PI      "#pi"

typedef enum {
    TSML_OK = 0,
    TSML_ERR_MALFORMED = -1, /**< the input is not a terseml document */
    TSML_ERR_DEPTH = -2,     /**< nesting past TSML_MAX_DEPTH */
    TSML_ERR_ALLOC = -3,
    TSML_ERR_PARAM = -4
} tsml_result;

const char *tsml_strerror(tsml_result rc);

typedef enum { TSML_TEXT, TSML_ELEMENT } tsml_kind;

typedef struct {
    const uint8_t *key;
    size_t key_len;
    const uint8_t *val;
    size_t val_len;
} tsml_attr;

typedef struct tsml_node tsml_node;

struct tsml_node {
    tsml_kind kind;

    /* TSML_TEXT: the bytes. May point into the decoded buffer. */
    const uint8_t *text;
    size_t text_len;

    /* TSML_ELEMENT */
    const uint8_t *tag;
    size_t tag_len;
    tsml_attr *attrs;
    size_t nattrs;
    tsml_node **children;
    size_t nchildren;
};

/** A decoded document, owning every node and copied string in it. */
typedef struct tsml_doc tsml_doc;

/**
 * @brief Decode one element.
 *
 * @p buf must outlive @p out: unescaped text points into it rather than being
 * copied. Rejects rather than guesses -- see GRAMMAR.md §6.
 */
tsml_result tsml_decode(const uint8_t *buf, size_t len, tsml_doc **out);

/** Decode a document of consecutive elements. */
tsml_result tsml_decode_document(const uint8_t *buf, size_t len, tsml_doc **out);

/** Root of a decoded document; for a multi-element document, the first. */
const tsml_node *tsml_doc_root(const tsml_doc *doc);

/** Number of top-level elements. */
size_t tsml_doc_count(const tsml_doc *doc);

/** The @p i-th top-level element. */
const tsml_node *tsml_doc_node(const tsml_doc *doc, size_t i);

/** Release a document and everything it owns. Safe on NULL. */
void tsml_doc_free(tsml_doc **doc);

/**
 * @brief Encode a node into canonical bytes (GRAMMAR.md §7).
 *
 * The caller frees @p out with free(). The same tree always produces the same
 * bytes; that is the property everything cross-implementation rests on.
 */
tsml_result tsml_encode(const tsml_node *node, uint8_t **out, size_t *len);

/* ── Building a tree to encode ───────────────────────────────────────── */

/** An arena for nodes the caller builds. Free with tsml_doc_free(). */
tsml_result tsml_builder_create(tsml_doc **out);

/** A text node. The bytes are copied into the arena. */
tsml_node *tsml_text(tsml_doc *arena, const uint8_t *text, size_t len);

/** An element. The tag is copied into the arena. */
tsml_node *tsml_element(tsml_doc *arena, const uint8_t *tag, size_t len);

/** Append an attribute; both strings are copied. */
tsml_result tsml_add_attr(tsml_doc *arena, tsml_node *el, const uint8_t *key, size_t key_len,
                          const uint8_t *val, size_t val_len);

/** Append a child. */
tsml_result tsml_add_child(tsml_doc *arena, tsml_node *el, tsml_node *child);

#ifdef __cplusplus
}
#endif

#endif /* TERSEML_H */
