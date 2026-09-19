/**
 * @file test_terseml.c
 * @brief Conformance and unit tests for the C implementation.
 *
 * The corpus in vectors.h is generated from the same source as vectors.json,
 * which the Python and JavaScript suites read. Three implementations agreeing
 * with one file is worth more than three agreeing with each other.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "terseml.h"
#include "vectors.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                                                           \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(cond)) {                                                                             \
            failures++;                                                                            \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                                   \
            fprintf(stderr, __VA_ARGS__);                                                          \
            fputc('\n', stderr);                                                                   \
        }                                                                                          \
    } while (0)

static void hexdump(FILE *f, const uint8_t *s, size_t n)
{
    size_t i;
    for (i = 0; i < n && i < 64; i++)
        fputc((s[i] >= 0x20 && s[i] < 0x7f) ? (char)s[i] : '.', f);
    if (n > 64) fputs("...", f);
}

/* ── §8 -- the shared corpus ─────────────────────────────────────────── */

static void test_vectors(void)
{
    size_t i;
    for (i = 0; i < TSML_VECTOR_COUNT; i++) {
        const tsml_vector *v = &TSML_VECTORS[i];
        tsml_doc *doc = NULL;
        uint8_t *out = NULL;
        size_t out_len = 0;
        tsml_result rc = tsml_decode(v->wire, v->len, &doc);

        CHECK(rc == TSML_OK, "%s: decode: %s", v->name, tsml_strerror(rc));
        if (rc != TSML_OK) continue;

        rc = tsml_encode(tsml_doc_root(doc), &out, &out_len);
        CHECK(rc == TSML_OK, "%s: encode: %s", v->name, tsml_strerror(rc));
        if (rc == TSML_OK) {
            int same = out_len == v->len && memcmp(out, v->wire, v->len) == 0;
            CHECK(same, "%s: re-encode differs", v->name);
            if (!same) {
                fputs("  want: ", stderr);
                hexdump(stderr, v->wire, v->len);
                fputs("\n  got:  ", stderr);
                hexdump(stderr, out, out_len);
                fputc('\n', stderr);
            }
            free(out);
        }
        tsml_doc_free(&doc);
    }
}

/* ── §4 -- structure ─────────────────────────────────────────────────── */

static int tag_is(const tsml_node *n, const char *s)
{
    return n && n->kind == TSML_ELEMENT && n->tag_len == strlen(s) &&
           memcmp(n->tag, s, n->tag_len) == 0;
}

static int text_is(const tsml_node *n, const char *s)
{
    return n && n->kind == TSML_TEXT && n->text_len == strlen(s) &&
           memcmp(n->text, s, n->text_len) == 0;
}

static tsml_doc *dec(const char *lit, size_t len)
{
    tsml_doc *doc = NULL;
    tsml_result rc = tsml_decode((const uint8_t *)lit, len, &doc);
    if (rc != TSML_OK) return NULL;
    return doc;
}

#define DEC(lit) dec(lit, sizeof(lit) - 1)

static void test_structure(void)
{
    tsml_doc *d;
    const tsml_node *r;

    d = DEC("{p,hello}");
    CHECK(d != NULL, "simple element decodes");
    if (d) {
        r = tsml_doc_root(d);
        CHECK(tag_is(r, "p"), "tag is p");
        CHECK(r->nchildren == 1 && text_is(r->children[0], "hello"), "one text child");
        CHECK(r->nattrs == 0, "no attributes");
        tsml_doc_free(&d);
    }

    /* §4.3: {foo} is content under the implicit tag; {foo,} is tag foo. */
    d = DEC("{foo}");
    CHECK(d != NULL, "singleton decodes");
    if (d) {
        r = tsml_doc_root(d);
        CHECK(tag_is(r, TSML_TAG_TEXT), "singleton takes the implicit tag");
        CHECK(r->nchildren == 1 && text_is(r->children[0], "foo"), "singleton body is content");
        tsml_doc_free(&d);
    }

    d = DEC("{foo,}");
    CHECK(d != NULL, "empty content decodes");
    if (d) {
        r = tsml_doc_root(d);
        CHECK(tag_is(r, "foo"), "trailing comma names the tag");
        CHECK(r->nchildren == 0, "and leaves no children");
        tsml_doc_free(&d);
    }

    d = DEC("{a,[k:v,k2:v2],body}");
    CHECK(d != NULL, "attributes decode");
    if (d) {
        r = tsml_doc_root(d);
        CHECK(r->nattrs == 2, "two attributes, got %zu", r ? r->nattrs : (size_t)0);
        if (r && r->nattrs == 2) {
            CHECK(r->attrs[0].key_len == 1 && r->attrs[0].key[0] == 'k', "first key");
            CHECK(r->attrs[1].val_len == 2 && memcmp(r->attrs[1].val, "v2", 2) == 0, "second val");
        }
        CHECK(r && r->nchildren == 1 && text_is(r->children[0], "body"), "body survives attrs");
        tsml_doc_free(&d);
    }

    d = DEC("{ul,{li,a}{li,b}}");
    CHECK(d != NULL, "nesting decodes");
    if (d) {
        r = tsml_doc_root(d);
        CHECK(r->nchildren == 2, "two children");
        if (r->nchildren == 2) {
            CHECK(tag_is(r->children[0], "li") && text_is(r->children[0]->children[0], "a"),
                  "first li");
            CHECK(tag_is(r->children[1], "li") && text_is(r->children[1]->children[0], "b"),
                  "second li");
        }
        tsml_doc_free(&d);
    }
}

/* ── §6 -- reject rather than guess ──────────────────────────────────── */

static void reject(const char *lit, size_t len, const char *why)
{
    tsml_doc *doc = NULL;
    tsml_result rc = tsml_decode((const uint8_t *)lit, len, &doc);
    CHECK(rc != TSML_OK, "should reject (%s): ", why);
    if (rc == TSML_OK) tsml_doc_free(&doc);
}

#define REJECT(lit, why) reject(lit, sizeof(lit) - 1, why)

static void test_rejection(void)
{
    REJECT("{p,unterminated", "truncation must not look like a short document");
    REJECT("{p,bad\\escape}", "an unknown escape means a version mismatch");
    REJECT("{a,[nokeyvalue],x}", "attribute pair without ':'");
    REJECT("{p,ok}trailing", "trailing bytes");
    REJECT("", "empty input is not a document");
    REJECT("{", "bare open brace");
    REJECT("p,no-brace}", "must open with a brace");
    REJECT("{p,\\", "escape at end of input");
    REJECT("{a,[k:v", "unterminated attribute block");

    /* Depth: the bound must exist and be stated. */
    {
        size_t n = 400, i;
        size_t len = n * 3 + 1 + n;
        char *deep = malloc(len);
        char *p = deep;
        tsml_doc *doc = NULL;
        for (i = 0; i < n; i++) { *p++ = '{'; *p++ = 'a'; *p++ = ','; }
        *p++ = 'x';
        for (i = 0; i < n; i++) *p++ = '}';
        CHECK(tsml_decode((const uint8_t *)deep, len, &doc) == TSML_ERR_DEPTH,
              "400 levels exceeds TSML_MAX_DEPTH");
        if (doc) tsml_doc_free(&doc);
        free(deep);
    }
}

/* ── §5.1 -- binary runs ─────────────────────────────────────────────── */

static void test_binary(void)
{
    tsml_doc *a = NULL;
    tsml_node *el, *t;
    uint8_t all[256];
    uint8_t *wire = NULL;
    size_t wire_len = 0, i;
    tsml_doc *back = NULL;
    const tsml_node *r;

    for (i = 0; i < 256; i++) all[i] = (uint8_t)i;

    CHECK(tsml_builder_create(&a) == TSML_OK, "builder");
    el = tsml_element(a, (const uint8_t *)"b", 1);
    t = tsml_text(a, all, sizeof(all));
    CHECK(el && t, "nodes allocate");
    CHECK(tsml_add_child(a, el, t) == TSML_OK, "add child");
    CHECK(tsml_encode(el, &wire, &wire_len) == TSML_OK, "encode 256 bytes");

    CHECK(tsml_decode(wire, wire_len, &back) == TSML_OK, "decode 256 bytes");
    if (back) {
        r = tsml_doc_root(back);
        CHECK(r->nchildren == 1, "one child back");
        if (r->nchildren == 1) {
            const tsml_node *c = r->children[0];
            CHECK(c->kind == TSML_TEXT && c->text_len == 256 &&
                      memcmp(c->text, all, 256) == 0,
                  "every byte survives the round trip");
        }
        tsml_doc_free(&back);
    }
    free(wire);
    tsml_doc_free(&a);

    /* A run of NULs: the counted form should win over escaping each one. */
    {
        uint8_t nuls[64];
        memset(nuls, 0, sizeof(nuls));
        CHECK(tsml_builder_create(&a) == TSML_OK, "builder 2");
        el = tsml_element(a, (const uint8_t *)"n", 1);
        tsml_add_child(a, el, tsml_text(a, nuls, sizeof(nuls)));
        CHECK(tsml_encode(el, &wire, &wire_len) == TSML_OK, "encode NUL run");
        CHECK(wire_len < 64 * 2, "counted form beats escaping (%zu bytes)", wire_len);
        CHECK(tsml_decode(wire, wire_len, &back) == TSML_OK, "NUL run decodes");
        if (back) {
            const tsml_node *c = tsml_doc_root(back)->children[0];
            CHECK(c->text_len == 64 && memcmp(c->text, nuls, 64) == 0, "NULs survive");
            tsml_doc_free(&back);
        }
        free(wire);
        tsml_doc_free(&a);
    }

    /* A corrupt length must be refused before it is trusted. */
    REJECT("{b,\\B\xff\xff\xff\xff\x0f}", "declared length exceeds the input");
}

/* ── §7 -- canonical form ────────────────────────────────────────────── */

static void expect_encoding(const char *in, size_t in_len, const char *want, size_t want_len,
                            const char *why)
{
    tsml_doc *doc = NULL;
    uint8_t *out = NULL;
    size_t out_len = 0;
    if (tsml_decode((const uint8_t *)in, in_len, &doc) != TSML_OK) {
        CHECK(0, "%s: input did not decode", why);
        return;
    }
    CHECK(tsml_encode(tsml_doc_root(doc), &out, &out_len) == TSML_OK, "%s: encode", why);
    CHECK(out_len == want_len && memcmp(out, want, want_len) == 0,
          "%s: got %.*s want %.*s", why, (int)out_len, (char *)out, (int)want_len, want);
    free(out);
    tsml_doc_free(&doc);
}

#define EXPECT_ENC(in, want, why) expect_encoding(in, sizeof(in) - 1, want, sizeof(want) - 1, why)

static void test_canonical(void)
{
    /* Rule 2: the trailing comma inside [...] is accepted, never emitted. */
    EXPECT_ENC("{a,[k:v,],x}", "{a,[k:v],x}", "trailing comma in attrs dropped");
    /* Rule 3: an element with no attributes omits the slot entirely. */
    EXPECT_ENC("{a,[],x}", "{a,x}", "empty attribute slot dropped");
    /* Rule 6: adjacent text children are merged -- nothing separates them. */
    EXPECT_ENC("{p,a{b,c}}", "{p,a{b,c}}", "text then element");

    /* A comma outside [...] is ordinary text, in every position. §7 rule 2
       is about the attribute slot and nothing else. */
    EXPECT_ENC("{p,hello,}", "{p,hello,}", "trailing comma in content is text");
    EXPECT_ENC("{p,,hello}", "{p,,hello}", "leading comma in content is text");

    /* §4.3: {foo} is accepted but is not what the encoder writes -- the
       canonical spelling names the implicit tag. Asserted so the asymmetry
       stays deliberate rather than becoming a surprise. */
    EXPECT_ENC("{foo}", "{#text,foo}", "singleton is accepted, not emitted");

    /* Encoding is a function of the tree: same tree, same bytes, every time. */
    {
        size_t i;
        uint8_t *first = NULL;
        size_t first_len = 0;
        for (i = 0; i < TSML_VECTOR_COUNT; i++) {
            const tsml_vector *v = &TSML_VECTORS[i];
            tsml_doc *doc = NULL;
            uint8_t *out = NULL;
            size_t out_len = 0;
            if (tsml_decode(v->wire, v->len, &doc) != TSML_OK) continue;
            tsml_encode(tsml_doc_root(doc), &out, &out_len);
            if (i == 0) { first = out; first_len = out_len; }
            else free(out);
            tsml_doc_free(&doc);
        }
        (void)first_len;
        free(first);
    }
}

/* ── the API's own edges ─────────────────────────────────────────────── */

static void test_api(void)
{
    tsml_doc *doc = NULL;
    uint8_t *out = NULL;
    size_t out_len = 0;

    CHECK(tsml_decode(NULL, 0, &doc) == TSML_ERR_PARAM, "NULL buffer is a parameter error");
    CHECK(tsml_decode((const uint8_t *)"{p,x}", 5, NULL) == TSML_ERR_PARAM, "NULL out");
    CHECK(tsml_encode(NULL, &out, &out_len) == TSML_ERR_PARAM, "NULL node");
    CHECK(tsml_doc_root(NULL) == NULL, "root of NULL is NULL");
    CHECK(tsml_doc_count(NULL) == 0, "count of NULL is 0");
    CHECK(tsml_doc_node(NULL, 0) == NULL, "node of NULL is NULL");
    tsml_doc_free(NULL);
    tsml_doc_free(&doc); /* already NULL */
    CHECK(strlen(tsml_strerror(TSML_OK)) > 0, "strerror(OK)");
    CHECK(strlen(tsml_strerror((tsml_result)-99)) > 0, "strerror(unknown)");

    /* A multi-element document. */
    CHECK(tsml_decode_document((const uint8_t *)"{a,1}{b,2}", 10, &doc) == TSML_OK,
          "two top-level elements");
    if (doc) {
        CHECK(tsml_doc_count(doc) == 2, "count is 2, got %zu", tsml_doc_count(doc));
        CHECK(tag_is(tsml_doc_node(doc, 0), "a"), "first is a");
        CHECK(tag_is(tsml_doc_node(doc, 1), "b"), "second is b");
        CHECK(tsml_doc_node(doc, 2) == NULL, "past the end is NULL");
        tsml_doc_free(&doc);
    }
}

/* ── build, encode, decode ───────────────────────────────────────────── */

static void test_builder(void)
{
    tsml_doc *a = NULL;
    tsml_node *ul, *li;
    uint8_t *out = NULL;
    size_t out_len = 0;
    static const char want[] = "{ul,[id:x],{li,one}{li,two}}";

    CHECK(tsml_builder_create(&a) == TSML_OK, "builder create");
    ul = tsml_element(a, (const uint8_t *)"ul", 2);
    CHECK(tsml_add_attr(a, ul, (const uint8_t *)"id", 2, (const uint8_t *)"x", 1) == TSML_OK,
          "add attr");
    li = tsml_element(a, (const uint8_t *)"li", 2);
    tsml_add_child(a, li, tsml_text(a, (const uint8_t *)"one", 3));
    tsml_add_child(a, ul, li);
    li = tsml_element(a, (const uint8_t *)"li", 2);
    tsml_add_child(a, li, tsml_text(a, (const uint8_t *)"two", 3));
    tsml_add_child(a, ul, li);

    CHECK(tsml_encode(ul, &out, &out_len) == TSML_OK, "encode built tree");
    CHECK(out_len == sizeof(want) - 1 && memcmp(out, want, out_len) == 0,
          "built tree encodes to %s, got %.*s", want, (int)out_len, (char *)out);
    free(out);

    /* Many children, to push the child array past its first growth. */
    {
        size_t i;
        tsml_node *big = tsml_element(a, (const uint8_t *)"big", 3);
        for (i = 0; i < 1000; i++)
            CHECK(tsml_add_child(a, big, tsml_text(a, (const uint8_t *)"x", 1)) == TSML_OK,
                  "add child %zu", i);
        CHECK(big->nchildren == 1000, "1000 children");
        for (i = 0; i < 300; i++)
            tsml_add_attr(a, big, (const uint8_t *)"k", 1, (const uint8_t *)"v", 1);
        CHECK(big->nattrs == 300, "300 attributes");
    }

    CHECK(tsml_add_child(a, NULL, NULL) == TSML_ERR_PARAM, "add_child rejects NULL");
    CHECK(tsml_add_attr(a, NULL, NULL, 0, NULL, 0) == TSML_ERR_PARAM, "add_attr rejects NULL");
    tsml_doc_free(&a);
}

/* ── escaping is positional (§5) ─────────────────────────────────────── */

static void test_positional_escaping(void)
{
    /* A comma in text is not structural: only the first one after the tag is. */
    EXPECT_ENC("{p,a,b,c}", "{p,a,b,c}", "commas in text are literal");
    /* A bracket only matters where attributes may begin. */
    EXPECT_ENC("{p,a[b]c}", "{p,a[b]c}", "brackets in mid-text are literal");
    /* ... but a leading bracket does, and must be escaped on the way out. */
    EXPECT_ENC("{p,\\[b]c}", "{p,\\[b]c}", "leading bracket stays escaped");

    /* Braces always matter, in every position. */
    {
        tsml_doc *a = NULL;
        uint8_t *out = NULL;
        size_t out_len = 0;
        tsml_doc *back = NULL;
        tsml_node *el;
        static const char body[] = "a{b}c";
        CHECK(tsml_builder_create(&a) == TSML_OK, "builder");
        el = tsml_element(a, (const uint8_t *)"p", 1);
        tsml_add_child(a, el, tsml_text(a, (const uint8_t *)body, sizeof(body) - 1));
        CHECK(tsml_encode(el, &out, &out_len) == TSML_OK, "encode braces");
        CHECK(tsml_decode(out, out_len, &back) == TSML_OK, "braces round-trip");
        if (back) {
            const tsml_node *c = tsml_doc_root(back)->children[0];
            CHECK(c->kind == TSML_TEXT && c->text_len == sizeof(body) - 1 &&
                      memcmp(c->text, body, c->text_len) == 0,
                  "braces survive");
            tsml_doc_free(&back);
        }
        free(out);
        tsml_doc_free(&a);
    }
}

/* terseml follows the TriePack release version while it lives in this
   repository; the constant is only useful if something checks it. */
static void test_version(void)
{
    FILE *f = fopen("../triepack-version.txt", "r");
    char want[64] = {0};
    if (!f) f = fopen("triepack-version.txt", "r");
    if (!f) {
        /* Run from somewhere else -- skip rather than fail the whole suite;
           the Python and JavaScript suites check the same thing. */
        fprintf(stderr, "note: triepack-version.txt not found, version check skipped\n");
        return;
    }
    if (fgets(want, sizeof(want), f)) {
        size_t n = strlen(want);
        while (n && (want[n - 1] == '\n' || want[n - 1] == '\r' || want[n - 1] == ' '))
            want[--n] = '\0';
        CHECK(strcmp(TSML_VERSION, want) == 0,
              "TSML_VERSION is %s, triepack-version.txt says %s -- run ./scripts/sync_version.sh",
              TSML_VERSION, want);
    }
    fclose(f);
}

int main(void)
{
    test_version();
    test_vectors();
    test_structure();
    test_rejection();
    test_binary();
    test_canonical();
    test_api();
    test_builder();
    test_positional_escaping();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
