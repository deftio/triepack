---
layout: default
title: terseml — complete reference
---

# terseml

<!-- Copyright (c) 2026 M. A. Chatterjee -->

**A positional encoding for tag / attribute / content trees.** The same
information an XML element or a JSON object carries, with the field names
removed because position already says what each slot is.

```
<a href="/x">click</a>          25 bytes  XML
["a",{"href":"/x"},"click"]     27 bytes  JsonML
{a,[href:/x],click}             19 bytes  terseml
```

This page is self-contained: what the format is, how it is specified, how to
build it, how to use it from C, Python and JavaScript, what it costs and what
it does not do.

> **Status.** Working implementations in C, Python and JavaScript; a complete
> grammar; a shared conformance corpus all three reproduce byte for byte.
> **Not published to npm, PyPI, crates.io or anywhere else.** It lives in the
> [TriePack repository](https://github.com/deftio/triepack/tree/main/terseml)
> as a subproject and is used from source.

---

## 1. Why it exists

Two observations, both measured rather than assumed.

**A tree costs more in field names than in data.** `<price>12.99</price>` is
twenty-one bytes carrying five. `{"price":"12.99"}` is seventeen carrying the
same five. The tag appears twice in XML and the quotes and colon are pure
structure in JSON. Position can carry what those bytes carry.

**Generic compressors are good at this, and still leave something on the
table.** gzip finds the repeated tag names, but it has to represent them
somehow. Removing them before compression means there is nothing to find.
More usefully, the removal is *reversible and cheap*, so the result stays a
format rather than becoming an archive.

The rule that came out of measuring it, which has held on every corpus since:

> **Compression tracks how much of the document is field names, and nothing
> else.**

| corpus | field names | terseml / original |
|---|---:|---:|
| product catalogue (JSON), 202 KB | 26.6% | **0.628** |
| Wikipedia XML, 16 MB | 2.7% | 0.979 |

Both rows are reproducible: `python3 bench_corpus.py path/to/enwik9` and
`python3 bench_corpus.py --json tests/data/benchmark_100k.json`, which print
the full tables including JsonML and gzip.

The catalogue loses **more** than its field names because JSON's quotes
around values go too. Neither row is a compression claim — gzip takes that
same catalogue to 0.082, and terseml's only contribution there is the 0.010
it saves gzip afterwards.

Nothing that preserves the text can do better on the second one. There is
nothing else to remove.

---

## 2. The format

### 2.1 An element

```
{tag,[attrs],content}
```

Position carries the meaning, so:

1. **The tag is whatever precedes the first comma.** No quotes, no closing
   tag, no repeating the name at the end.
2. **Attributes are a single optional `[...]` block**, and only in the slot
   immediately after that comma. A `[` anywhere else is an ordinary byte.
3. **Everything after is content** — text and nested elements in order,
   with nothing separating them.

```
{p,hello}                      text
{p,}                           an element with empty content
{a,[href:/x,rel:me],click}     two attributes
{ul,{li,one}{li,two}}          nested
{p,Hello {b,world}!}           mixed content
```

An element *always* has a comma, which leaves `{foo}` free to mean something
else: content under the document's implicit tag. That short form is
**accepted on input and never emitted** — see §2.4.

### 2.2 It is a byte format

A document is a byte string. Tags, keys, values and content may contain NUL
or any other byte value, and nothing assumes UTF-8. This is the property that
makes it safe to put arbitrary payloads in a document without a separate
encoding layer.

### 2.3 Reserved bytes are positional

This is the part that decides whether an implementation is correct *and*
whether it is any good. The reserved set is `{` `}` `[` `]` `,` `:` `\`, but
**where** each one matters differs:

| In | Escape | Leave alone |
|---|---|---|
| tag | `,` `{` `}` `\` | everything else |
| attribute key | `:` `,` `]` `\` | `{` `}` `[` |
| attribute value | `,` `]` `\` | `:` `{` `}` `[` |
| text content | `{` `}` `\`, and `[` **only as the first byte** | `,` `:` `]`, and `[` elsewhere |

So `{p,a,b,c}` is the text `a,b,c` — only the *first* comma was structural.
`{p,see [[link]]}` needs no escape, because `[` is not the first byte.
`{p,\[start]}` does.

Escaping more than this still round-trips, which is why it is easy to get
wrong and quietly expensive. On 16 MB of Wikipedia, escaping the commas and
brackets that never needed it cost **888,576 bytes** — 5.3% of the file —
and inverted a comparison against JsonML.

The escape production accepts *every* reserved byte in every position, even
though no position needs all of them, because a decoder cannot tell which
position an escape came from without accepting them all.

### 2.4 Binary runs

Escaping costs one byte per reserved byte, which is fine for prose and bad
for binary. Two run forms exist so that no payload has to be escaped into
something larger than itself.

**`\b` — NUL-terminated.** `\b <bytes> 00`. Inside, only NUL (which ends it)
and `\` are special; `\0` is a literal NUL and `\\` a literal backslash.
Every other byte, *including* `{` `}` `[` `]` `,` `:`, is itself.

**`\B` — length-prefixed.** `\B <varint length> <that many bytes>`. Nothing
inside is escaped, any byte may appear, and it is the only construct a parser
can traverse without inspecting its contents.

All three encodings of a payload are legal, which would admit three files for
one document. So the choice is not the encoder's:

> An encoder **must** emit whichever of inline, `\b` and `\B` is strictly
> shortest. Ties break `\B`, then `\b`, then inline.

```
inline  = n + count of { } \ in the payload
\b      = 2 + n + count of NUL and \ + 1
\B      = 2 + varint_len(n) + n
```

Each form owns a region. Measured on 64 KB payloads:

| payload | inline | `\b` | `\B` | shortest |
|---|---:|---:|---:|---|
| plain ASCII | **65,536** | 65,539 | 65,541 | inline |
| wiki markup | 65,668 | **65,539** | 65,541 | `\b` |
| brace-dense | 131,072 | **65,539** | 65,541 | `\b` |
| random binary | 66,267 | 66,052 | **65,541** | `\B` |
| 90% NUL | 66,219 | 72,578 | **65,541** | `\B` |

### 2.5 Canonical form

One tree must produce one byte string, or nothing cross-implementation
works. Seven rules, all in [`GRAMMAR.md`](https://github.com/deftio/triepack/blob/main/terseml/GRAMMAR.md) §7:
attributes in the order given; no trailing comma inside `[...]`; no empty
attribute slot; nothing escaped that the table above does not require; the
shortest binary form; adjacent text children merged; and the `{foo}` short
form accepted but never written.

### 2.6 A decoder must reject, not guess

Truncation, an unknown escape, an attribute pair with no `:`, nesting past
the stated depth bound, a `\B` length longer than the remaining input, an
unterminated `\b` run, a varint over ten groups. Silently repairing a
malformed document is what makes two implementations disagree.

The `\B` length is the dangerous one, and the grammar says so explicitly:
**validate the length against the input before allocating.** That exact
shape — `read_bytes(n)` allocating `n` before checking `n` bytes exist —
produced a real denial of service in a sibling project, aborting the process
on a ten-byte input.

---

## 3. Building

terseml has no dependencies. It does not link TriePack, and TriePack does not
link it.

```bash
git clone https://github.com/deftio/triepack.git
cd triepack/terseml
make check
```

`make check` builds the C implementation, runs it, runs it again under
AddressSanitizer and UndefinedBehaviorSanitizer, links the header from C++,
then runs the Python and JavaScript suites. Individual targets:

| target | what it does |
|---|---|
| `make` | builds `tsml`, the command-line tool |
| `make test` | C conformance and unit tests |
| `make sanitize` | the same under ASan + UBSan |
| `make cxx` | builds and runs the C++ link check |
| `make vectors` | regenerates `vectors.json` and `vectors.h` |
| `make bench FILE=x.tsml` | times decode and encode |
| `make clean` | removes build output |

The C sources are two files, `terseml.c` and `terseml.h`, C99 with no
configuration. Drop them into a project and compile:

```bash
cc -O2 -std=c99 -c terseml.c
```

Python and JavaScript are single files with no packaging and no
dependencies — `terseml.py` and `terseml.js`, imported from the directory.

### Cross-compiling

The only libc symbols required are `malloc`, `calloc`, `realloc`, `free` and
`memcpy`. No stdio, no locale, no floating point, no libm.

```bash
clang --target=thumbv7m-none-eabi -Oz -std=c99 -ffreestanding -c terseml.c
```

---

## 4. Using it

### C

```c
#include "terseml.h"

tsml_doc *doc;                                  /* owns every node */
if (tsml_decode(buf, len, &doc) == TSML_OK) {
    const tsml_node *root = tsml_doc_root(doc);
    for (size_t i = 0; i < root->nchildren; i++) {
        const tsml_node *c = root->children[i];
        if (c->kind == TSML_TEXT) fwrite(c->text, 1, c->text_len, stdout);
    }
    uint8_t *wire; size_t n;
    tsml_encode(root, &wire, &n);               /* canonical bytes back */
    free(wire);
    tsml_doc_free(&doc);                        /* one call frees the tree */
}
```

Building a tree to encode:

```c
tsml_doc *arena;
tsml_builder_create(&arena);
tsml_node *el = tsml_element(arena, (const uint8_t *)"a", 1);
tsml_add_attr(arena, el, (const uint8_t *)"href", 4, (const uint8_t *)"/x", 2);
tsml_add_child(arena, el, tsml_text(arena, (const uint8_t *)"click", 5));

uint8_t *wire; size_t n;
tsml_encode(el, &wire, &n);                     /* {a,[href:/x],click} */
free(wire);
tsml_doc_free(&arena);
```

Decoding is arena-backed: one document is one allocation chain, dropped in a
single call rather than walked. **Text containing no escapes is not copied at
all** — the node points into the buffer you passed, which must outlive the
document. `terseml.h` is `extern "C"`, and a C++ build of it is part of the
test suite rather than a claim in a comment.

### Python

```python
from terseml import Element, encode, decode

el = Element(b"a", [(b"href", b"/x")], [b"click"])
wire = encode(el)                      # b'{a,[href:/x],click}'
back = decode(wire)
```

### JavaScript

```javascript
const { Element, encode, decode } = require('./terseml');

const el = new Element('a', [['href', '/x']], ['click']);
const wire = encode(el);               // <Buffer 7b 61 2c ...>
const back = decode(wire);
```

### XML bridge and command line

```bash
python3 xml_bridge.py encode doc.xml  > doc.tsml
python3 xml_bridge.py decode doc.tsml > doc.xml
```

The bridge preserves comments and processing instructions, mapping them to
the reserved tags `#comment` and `#pi` — a convention on top of the single
node type the grammar defines, not an extension to it.

The C tool round-trips, validates and benchmarks:

```bash
./tsml roundtrip < in.tsml > out.tsml   # decode, re-encode canonically
./tsml check     < in.tsml              # non-zero if it does not parse
./tsml bench 10  < in.tsml              # time ten decode+encode passes
```

`roundtrip` is also the differential-test harness: feed it bytes another
implementation produced and compare what comes back.

---

## 5. Cost

### Throughput

15 MB of Wikipedia XML converted by the bridge, one process, warm cache:

| | encode | decode |
|---|---:|---:|
| **C** | **959 MB/s** | **2,656 MB/s** |
| JavaScript | 73 MB/s | 175 MB/s |
| Python | 25 MB/s | 5 MB/s |

A gigabyte decodes in 1.3 seconds. The tree costs 0.43× the document on top
of the document itself.

### Code size

`.text` of `terseml.o`, both encoder and decoder:

| target | `-Oz` | `-O2` |
|---|---:|---:|
| **Thumb-2** (Cortex-M3/M4/M7) | **3,952** | 9,248 |
| Thumb-1 (Cortex-M0) | 3,978 | 8,416 |
| ARM (v7-A, ARM mode) | 6,162 | 11,782 |
| arm64 | 5,678 | 10,560 |
| x86-64 | 6,349 | 10,622 |

No `.bss`. Roughly 60% of it is the decoder and 40% the encoder; the two are
separate functions, so `--gc-sections` drops the half you do not call.

`-O2` buys 38% on decode for about double the code, and buys the **encoder
nothing** — it is bound by `memcpy` and buffer growth, not by its own
instructions. `-Os` is worse than `-Oz` on this code in both size and speed;
`-O3` is bigger than `-O2` and slower to decode. The useful choices are
`-Oz` and `-O2`.

---

## 6. What it is not

**It is not a compressor.** It removes the bytes a tree spends on field names
and leaves a stream for whatever comes next — gzip, a trie, a radio link. On
prose it will not help, and on a schema-heavy catalogue gzip beats it eight
times over. The measurements above say so rather than burying it.

**It is not an archive.** One tree in, one tree out. No file bundling, no
directory, no index.

**It is not a schema.** Nothing validates. The implicit tag and the
`#comment` / `#pi` conventions are *profile* decisions a user of the format
states; the grammar defines one node type and stops.

**It is not published.** No npm, no PyPI, no crates.io. Use it from source.

### Against JsonML

[JsonML](http://www.jsonml.org/) expresses the same idea in valid JSON:
`["tag", {attributes}, child, ...]`. It needs no custom parser, which is a
real advantage. terseml trades that for the bytes JSON spends on quotes:
measured on 16 MB of Wikipedia XML, **terseml 0.979 against JsonML's
1.004** — though once gzipped the three converge: 0.368, 0.370 and 0.370 of
the XML. Reproduce with `python3 bench_corpus.py path/to/enwik9`.

Choose JsonML when generic JSON tooling will handle the document. Choose
terseml when it is going into something that will re-encode it anyway.

---

## 7. Correctness

The grammar is the authority, and the tests are written from it rather than
from any implementation.

```bash
make test                                                 # C: 1,163 checks
make sanitize                                             # the same, ASan+UBSan
make cxx                                                  # the header from C++
python3 -m pytest -q test_terseml.py test_xml_bridge.py   # 82 tests
node test_terseml.js                                      # 63 checks
```

`vectors.json` holds 26 cases as (tree, expected bytes); `vectors.h` is the
same corpus for C. **One generator produces both**, so the three
implementations cannot drift apart by testing different files. Each encodes
every tree and must produce exactly those bytes, then decodes those bytes and
recovers the tree.

Beyond the vectors: every row of the escaping table, every rejection the
grammar requires, all 256 byte values in every position, 50,000-element and
megabyte-payload documents, truncation at every offset, and a randomised
round-trip over 3,000 trees built from adversarial byte strings.

### Three bugs the grammar had, and how each was found

None by inspection.

**A randomised round-trip found two.** §2 allowed four escape bytes while §5
required seven, so the encoder wrote `\]` inside an attribute value and the
decoder rejected its own output. And nothing separates adjacent text nodes on
the wire, so `["a","b"]` and `["ab"]` are necessarily the same bytes — now a
stated canonical rule rather than something two implementations could
disagree about.

**A differential test against a third implementation found the third**, a
real round-trip bug that had been in Python and JavaScript from the start.
Both wrote the short form `{foo}` for a bare text node. Inside `{foo}` there
is no content slot yet, so a comma there *is* structural: the text `a,b`
encoded to `{a,b}`, which reads back as an element tagged `a`.

That one survived a 3,000-tree randomised round-trip in *each* implementation,
because each decoded its own output the way it had encoded it. Only running
one implementation's bytes through another's parser exposed it. It is now
canonical rule 7 — the short form is accepted and never emitted — and it cost
six bytes on a construct a document reaches only at its root.

### And two the C implementation had

**It asked malloc for 16 GB on a 15 MB file.** Both unescaping paths buffered
at the remaining length of the *input* rather than the length of the *run*,
so every escaped text node and every `\b` run reserved a whole document's
worth of arena. Wikipedia markup is brace-dense, so nearly everything took
one of those paths.

Throughput hid it completely — the format was already fast while allocating
two thousand times what it needed. Fixing the first path made the second
*less* visible, not more: the total looked reasonable, and only attributing
arena bytes to individual call sites showed 23,844 element allocations
averaging 993 bytes where 80 was the answer. Measuring each run first took
allocation from 16,029 MB to 6.7 MB, decode from 1,170 to 2,656 MB/s, and the
resident set for a gigabyte input from 8.6 GB to 1.5 GB.

---

## 8. Versioning

terseml reports the **TriePack release version** — `TSML_VERSION` in C,
`__version__` in Python, `VERSION` in JavaScript — and
`./scripts/sync_version.sh` keeps all three in step with
`triepack-version.txt`. Each implementation's test suite asserts it, so a
stale constant fails a build rather than shipping.

This is a convenience, not a statement that terseml is part of TriePack. One
number across one repository is less confusing than two while both live here.
**When terseml is spun out it will version on its own and the two will
diverge** — at which point those three entries come out of `sync_version.sh`.

The grammar version is a separate thing again, and is not this number: a
change to the wire format is described in [`GRAMMAR.md`](https://github.com/deftio/triepack/blob/main/terseml/GRAMMAR.md), not by
the release version.

## 9. Files

| | |
|---|---|
| `GRAMMAR.md` | the specification; the authority |
| `terseml.c` / `terseml.h` | C99 implementation; arena-backed, zero-copy where it can be |
| `terseml.py` / `terseml.js` | independent implementations, not bindings |
| `test_terseml.c` / `.py` / `.js` | tests written from the grammar |
| `test_terseml_cxx.cpp` | the header, built as C++ |
| `tsml_cli.c` | round-trip, check and benchmark; the differential-test harness |
| `Makefile` | builds and runs all of it standalone |
| `vectors.json` / `vectors.h` | shared conformance corpus, one generator |
| `make_vectors.py` | regenerates both |
| `bench_corpus.py` | the size table quoted in these docs |
| `xml_bridge.py` | XML ↔ terseml, and the command line |
| `test_xml_bridge.py` | infoset fidelity, including comments and PIs |
| `README.md` | the repository-facing version of this page |

## 10. Licence

BSD-2-Clause, the same as the repository it sits in.
