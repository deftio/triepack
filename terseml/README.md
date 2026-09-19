# terseml

A positional encoding for tag / attribute / content trees. The
same information an XML element or a JSON object carries, with the field
names removed because position already says what each slot is.

```
<a href="/x">click</a>          25 bytes  XML
["a",{"href":"/x"},"click"]     27 bytes  JsonML
{a,[href:/x],click}             19 bytes  terseml
```

It is a **byte format that happens to be readable**, not a text format: a
document is a byte string, keys and content may contain NUL or any other
byte, and nothing assumes UTF-8.

## Status

Working implementations in C, Python and JavaScript, a complete
[grammar](GRAMMAR.md), and a shared conformance corpus all three must
reproduce byte for byte. Not published to any registry.

This lives beside triepack because it came out of that work, but it is a
separate thing — triepack is a binary dictionary format, terseml is a tree
serialisation.

## Why

Two observations, both measured rather than assumed:

**A document's field names may be most of it, or almost none of it.** On a
product-catalog JSON, field names and their quotes are **26.6%** of the
bytes; on Wikipedia XML, markup is **2.7%**. Removing them is transformative
for the first and noise for the second. terseml is worth reaching for when
your keys repeat.

**XML's model is the larger one.** It has mixed content
(`<p>Hello <b>world</b>!</p>`) and repeated sibling names, neither of which
JSON expresses without a convention. terseml carries the **XML infoset**;
JSON is a lossy projection of it, not a peer. That is the honest direction of
the arrow.

## Use

```python
from terseml import Element, encode, decode

el = Element(b"a", [(b"href", b"/x")], [b"click"])
wire = encode(el)                      # b'{a,[href:/x],click}'
back = decode(wire)
```

```javascript
const { Element, encode, decode } = require('./terseml');

const el = new Element('a', [['href', '/x']], ['click']);
const wire = encode(el);               // <Buffer 7b 61 2c ...>
const back = decode(wire);
```

```c
#include "terseml.h"

tsml_doc *doc;                                  /* owns every node */
if (tsml_decode(buf, len, &doc) == TSML_OK) {
    const tsml_node *root = tsml_doc_root(doc); /* tag, attrs, children */
    uint8_t *wire; size_t n;
    tsml_encode(root, &wire, &n);               /* canonical bytes back */
    free(wire);
    tsml_doc_free(&doc);                        /* one call frees the tree */
}
```

Decoding is arena-backed: one document is one allocation chain, dropped in a
single call rather than walked. Text holding no escapes is not copied at all
— the node points into the buffer you passed, which must outlive the
document. `terseml.h` is `extern "C"`, and a C++ build of it is part of the
test suite rather than a claim in a comment.

All three implementations are independent — neither the JavaScript nor the C
one is a binding over another. That is deliberate: implementations that *can*
disagree, checked against a shared corpus, is the only way to find out
whether a grammar is actually unambiguous. It wasn't, three times; see below.

## What it is for, and what it is not

**terseml is a transport and preprocessing format, not a compressor.** It
removes the field names a tree spends its bytes on and leaves a compact,
unambiguous, byte-transparent stream for the next stage — whether that is a
wire, a parser, or a format like triepack that will re-encode it anyway.

It is not competing with gzip or zstd and should not be measured against
them. If you want small bytes for something you will expand in one go,
compress. If you want a stream that is already smaller, parses in one pass,
and survives arbitrary bytes, this.

The two are orthogonal: you can gzip a terseml stream, and on a corpus with
real redundancy you should.

## Measured

16 MB of Wikipedia XML (an `enwik9` prefix trimmed to whole `<page>`
elements): 15,966,815 bytes, 2,151 pages. Reproduce with
`python3 bench_corpus.py path/to/enwik9`.

| format | bytes | of XML |
|---|---:|---:|
| XML | 15,966,815 | 1.000 |
| JsonML | 16,031,935 | 1.004 |
| **terseml** | **15,628,686** | **0.979** |

Once gzipped the three converge, which is the honest headline:

| | bytes | of XML |
|---|---:|---:|
| gzip(XML) | 5,903,088 | 0.370 |
| gzip(JsonML) | 5,906,526 | 0.370 |
| gzip(terseml) | 5,874,231 | 0.368 |

Wikipedia is 97.3% article text, so there is almost no markup to remove and
2% is close to the ceiling. Schema-heavy data has more to give
(`python3 bench_corpus.py --json ../tests/data/benchmark_100k.json`):

| | bytes | of raw |
|---|---:|---:|
| JSON | 202,408 | 1.000 |
| field names in it | 53,930 | 0.266 |
| **terseml** | **127,014** | **0.628** |
| gzip(JSON) | 16,626 | 0.082 |
| gzip(terseml) | 14,551 | 0.072 |

It takes out more than the field names, because JSON's quotes around values
go too. And gzip beating it eight times over is exactly why this is not a
compressor — the value is that the removal is cheap and reversible and leaves
a *format*, not that it competes with an entropy coder.

**Compression tracks how much of your document is field names, and nothing
else.** That is the whole rule, and it is why this is worth reaching for on
repetitive schemas and not worth reaching for on prose.

## Command line

```bash
python3 xml_bridge.py encode doc.xml  > doc.tsml
python3 xml_bridge.py decode doc.tsml > doc.xml
cat doc.xml | python3 xml_bridge.py encode > doc.tsml
```

The bridge keeps comments and processing instructions, which map to the
reserved tags `#comment` and `#pi` — a convention on top of the single node
type the grammar defines, not an extension to it. Round-tripping
`<r a="1"><!-- note --><?pi go?><p>Hello <b>world</b>!</p><e/></r>` returns
exactly those bytes.

## Speed

Measured on 15 MB of Wikipedia XML converted by the bridge, one process, warm
cache:

| | encode | decode |
|---|---:|---:|
| **C** | **959 MB/s** | **2,656 MB/s** |
| JavaScript | 73 MB/s | 175 MB/s |
| Python | 25 MB/s | 5 MB/s |

The C decoder is the reason the format claims to be cheap. It is single pass
with one byte of lookahead, it does not copy text that contains no escapes —
the node points into the caller's buffer — and it allocates every node from
one arena freed in a single call. A gigabyte document decodes in
1.3 seconds, and the tree it builds costs 0.43× the document on top of the
document itself.

At `-Oz` the object is 5.7 KB of `.text` on arm64 and **3,952 bytes on
Thumb-2** (3,978 on Cortex-M0), with no `.bss` and five libc symbols:
`malloc`, `calloc`, `realloc`, `free`, `memcpy`. `-O2` buys 38% on decode for
roughly double the code, and buys the encoder nothing — it is bound by
`memcpy` and buffer growth, not by its own instructions.

The Python and JavaScript numbers were far slower before profiling: the Python decoder was walking the
buffer a byte at a time through method calls, and rebuilding a `set` of stop
bytes **once per byte** — 1.7 million set constructions on a 2 MB document.
Escaping is now a handful of bulk `replace` calls and scanning is a compiled
regex that jumps to the next structural byte, so the common case of text with
no delimiters is one search and one slice. That is 5× on encode and 11× on
decode.

There is still no Rust implementation.

**The C implementation asked malloc for 16 GB on a 15 MB file** before its
allocations were sized properly. Both unescaping paths — `read_escaped` and
`read_binary` — buffered at the remaining length of the *input* rather than
the length of the *run*, so every escaped text node and every `\b` run
reserved a whole document's worth of arena. Wikipedia markup is brace-dense,
so nearly everything took one of those paths. Measuring the run first took
allocation from 16,029 MB to 6.7 MB, decode from 1,170 to 2,656 MB/s, and the
resident set for a gigabyte from 8.6 GB to 1.5 GB.

Throughput hid it completely: the format was already "fast" while allocating
two thousand times what it needed. Worse, fixing the first path made the
second one *invisible* — the total looked reasonable, and only attributing
arena bytes to individual call sites showed 23,844 element allocations
averaging 993 bytes where 80 was the answer. Two of those calls were 10 MB
each.

## Testing

```bash
make check                                                # all of the below
make test                                                 # C: 1,163 checks
make sanitize                                             # C under ASan+UBSan
make cxx                                                  # the header from C++
python3 -m pytest -q test_terseml.py test_xml_bridge.py   # 82 tests
node test_terseml.js                                      # 63 checks
make vectors                                              # regenerate the corpus
```

`vectors.json` holds 26 cases as (tree, expected bytes), and `vectors.h` is
the same corpus for C — one generator, so the three cannot drift apart by
testing different files. Each implementation encodes every tree and must
produce exactly those bytes, then decodes those bytes and recovers the tree.
Three implementations that all match a fourth file cannot quietly agree with
each other on something the grammar does not say.

Beyond the vectors: every row of the grammar's escaping table, every
rejection it requires, all 256 byte values in every position, 50,000-element
and megabyte-payload documents, truncation at every offset, and a randomised
round-trip over 3,000 trees built from adversarial byte strings.

**The randomised test found two bugs in the grammar itself**, neither by
inspection:

- §2 allowed four escape bytes while §5 required seven, so the encoder wrote
  `\]` inside an attribute value and the decoder rejected its own output.
- Nothing separates adjacent text nodes on the wire, so `["a","b"]` and
  `["ab"]` are necessarily the same bytes. Now a stated canonical-form rule
  rather than something two implementations could disagree about.

**A differential test against the C implementation found a third** — a real
round-trip bug that had been in Python and JavaScript from the start. Both
wrote the short form `{foo}` for a bare text node. Inside `{foo}` there is no
content slot yet, so a comma there *is* structural: the text `a,b` encoded to
`{a,b}`, which reads back as an element tagged `a`. Canonical-form rule 7 now
says the short form is accepted and never emitted. It cost six bytes on a
construct a document reaches only at its root, and it was worth none of them.

That bug survived a 3,000-tree randomised round-trip in each implementation,
because each one decoded its own output the way it had encoded it. Only
running one implementation's bytes through another's parser exposed it.

## Design notes

**Three binary forms, and the encoder does not get to choose.** Payloads may
be inline (escaped), `\b` (NUL-terminated) or `\B` (length-prefixed), and
each wins on a different data shape — measured in [GRAMMAR.md §5.1](GRAMMAR.md).
Since three legal encodings of one payload would mean three legal files for
one document, the shortest is **mandatory**, with ties broken toward `\B`
because it is the only form a parser can skip without reading.

**One escape byte, two meanings, no ambiguity.** `\` followed by punctuation
is a literal; `\` followed by a letter is a command (`\b`, `\B`). The
reserved bytes are all punctuation, so the sets cannot collide, and an
unknown letter is rejected — a future version fails loudly instead of being
misread.

**Reserved bytes are positional.** Only the *first* comma in an element is
structural; `[` matters only in the attribute slot. `a,b,c` needs no escaping
at all. Getting this wrong costs real bytes: over-escaping Wikipedia text
inflated it by 888,576 bytes, 5.3%, and inverted a comparison.

## Relationship to JsonML

[JsonML](http://www.jsonml.org/) is the same idea in valid JSON:
`["tag", {attrs}, child, ...]`. It needs no custom parser, which is a real
advantage. terseml trades that for the bytes JSON spends on quotes — 0.979
against JsonML's 1.004 on the Wikipedia corpus above.

Use JsonML when generic JSON tooling will touch the document — that is a real
advantage and worth 2%. Use terseml when the stream is going to your own next
stage, when content may contain arbitrary bytes, or when you need one
document to have exactly one encoding.

## Name and file extension

`terseml` — terse markup — file extension **`.tsml`**. Clear on npm, PyPI
and crates.io, one unrelated GitHub substring match, and no hits as a format,
language or library in a web search.

Near neighbours, none of them a conflict but worth knowing about:
[`terse`](https://pkg.go.dev/github.com/acsellers/multitemplate/terse) (Go
HTML templating), IBM's TERSE compression (`.trs`, 1984), and
[`telml`](https://github.com/aisamanra/telml) — one letter away and in the
same space.

Names considered and rejected:

| | why not |
|---|---|
| `tpack` | taken: npm 3.0.1 and PyPI 1.0.2 |
| `xjpack` / `xjpak` | **XJTAG's XJPack** — commercial boundary-scan software that packages project data into a portable `.xjp` file. Invisible to package registries, and adjacent enough in purpose to confuse. Their whole product line is `XJ`-prefixed, so any `xj*` name reads as theirs. |
| `jsonpack` | misleading: this carries the XML infoset, not JSON's model |
| `tacpack` | taken: **VRS TacPack**, a combat-simulation framework for Microsoft Flight Simulator X and Prepar3D, with an SDK and third-party developers |
| `xjarchive` | free, but the name says *archive* while this is explicitly not one — an archive bundles and compresses files; this serialises one tree. It also shares XJTAG's prefix. |

Both misses were **commercial desktop software**, which no package registry
indexes. Checking npm/PyPI/crates/GitHub is necessary and not sufficient; a
plain web search caught what four registries missed, twice.

XJTAG's product line is XJ-prefixed (XJPack, XJRunner, XJInvestigator,
XJAnalyser) and XJPack packages project data into a portable `.xjp` file, so
that whole prefix is occupied by software doing something adjacent. The
`tac-` prefix is likewise crowded in defence and simulation: TacPack,
Tacview, TactWare.

**Extension: `.tsml`.** Rejected `.tpk` (ArcGIS tile packages, TI
calculators), `.tac` (Python's Twisted), `.tmz` (Medigraph Compressed
Database, and a brand that owns the search results) and `.tml` — which is not
taken outright but is shared by Apache Tapestry templates, PADGen,
ThoughtSpot and several unrelated "markup language" uses, so it would not
identify anything. `.tsml` had no hits.

There is no authoritative registry of file extensions, so this is "nothing
found" rather than "verified free" — the same caveat that caught two names
above.

## Versioning

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
change to the wire format is described in [`GRAMMAR.md`](GRAMMAR.md), not by
the release version.

## Files

| | |
|---|---|
| [`GRAMMAR.md`](GRAMMAR.md) | the specification; the authority |
| `terseml.c` / `terseml.h` | C99 implementation; arena-backed, zero-copy where it can be |
| `terseml.py` / `terseml.js` | independent reference implementations |
| `test_terseml.c` / `.py` / `.js` | tests written from the grammar |
| `test_terseml_cxx.cpp` | the header, built as C++ |
| `tsml_cli.c` | round-trip, check and benchmark; the differential-test harness |
| `Makefile` | builds and runs all of it standalone |
| `vectors.json` / `vectors.h` | shared conformance corpus, one generator |
| `make_vectors.py` | regenerates both |
| `bench_corpus.py` | the size table above |
| `xml_bridge.py` | XML ↔ terseml, and the command line |
| `test_xml_bridge.py` | infoset fidelity, including comments and PIs |

## Licence

BSD-2-Clause, as with the rest of this repository.
