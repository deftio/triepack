# xjarchive

A positional encoding for tag / attribute / content trees. The
same information an XML element or a JSON object carries, with the field
names removed because position already says what each slot is.

```
<a href="/x">click</a>          25 bytes  XML
["a",{"href":"/x"},"click"]     27 bytes  JsonML
{a,[href:/x],click}             19 bytes  xjarchive
```

It is a **byte format that happens to be readable**, not a text format: a
document is a byte string, keys and content may contain NUL or any other
byte, and nothing assumes UTF-8.

## Status

Working reference implementations in Python and JavaScript, a complete
[grammar](GRAMMAR.md), and a shared conformance corpus both must reproduce
byte for byte. Not published to any registry.

This lives beside triepack because it came out of that work, but it is a
separate thing — triepack is a binary dictionary format, xjarchive is a tree
serialisation.

## Why

Two observations, both measured rather than assumed:

**A document's field names may be most of it, or almost none of it.** On a
product-catalog JSON, field names and their quotes are **26.6%** of the
bytes; on Wikipedia XML, markup is **2.7%**. Removing them is transformative
for the first and noise for the second. xjarchive is worth reaching for when
your keys repeat.

**XML's model is the larger one.** It has mixed content
(`<p>Hello <b>world</b>!</p>`) and repeated sibling names, neither of which
JSON expresses without a convention. xjarchive carries the **XML infoset**;
JSON is a lossy projection of it, not a peer. That is the honest direction of
the arrow.

## Use

```python
from xjarchive import Element, encode, decode

el = Element(b"a", [(b"href", b"/x")], [b"click"])
wire = encode(el)                      # b'{a,[href:/x],click}'
back = decode(wire)
```

```javascript
const { Element, encode, decode } = require('./xjarchive');

const el = new Element('a', [['href', '/x']], ['click']);
const wire = encode(el);               // <Buffer 7b 61 2c ...>
const back = decode(wire);
```

Both implementations are independent — the JavaScript one is not a binding
over the Python one. That is deliberate: two implementations that *can*
disagree, checked against a shared corpus, is the only way to find out
whether a grammar is actually unambiguous. It wasn't, twice; see below.

## What it is for, and what it is not

**xjarchive is a transport and preprocessing format, not a compressor.** It
removes the field names a tree spends its bytes on and leaves a compact,
unambiguous, byte-transparent stream for the next stage — whether that is a
wire, a parser, or a format like triepack that will re-encode it anyway.

It is not competing with gzip or zstd and should not be measured against
them. If you want small bytes for something you will expand in one go,
compress. If you want a stream that is already smaller, parses in one pass,
and survives arbitrary bytes, this.

The two are orthogonal: you can gzip a xjarchive stream, and on a corpus with
real redundancy you should.

## Measured

16 MB of Wikipedia XML (`enwik9` prefix), 2,267 `<page>` elements, same
content three ways:

| format | bytes | of XML | parser |
|---|---:|---:|---|
| XML | 4,190,960 | 1.000 | ElementTree (C) |
| JsonML | 4,190,361 | 1.000 | `json` (C) |
| **xjarchive** | **4,104,036** | **0.979** | pure Python |

**All 2,267 pages round-trip**, at 11.7 MB/s encode and 3.4 MB/s decode.

Two things that number does *not* say. Wikipedia is 97.3% article text, so
there is almost no markup to remove and 2% is close to the ceiling; on
schema-heavy data the same idea reaches **0.373**, because field names there
are 26.6% of the file. And the parse rate is a pure-Python decoder measured
against two C parsers — that compares implementations, not formats. The
grammar is single-pass with one byte of lookahead, so a C decoder should sit
with the others; nobody has written one.

**Compression tracks how much of your document is field names, and nothing
else.** That is the whole rule, and it is why this is worth reaching for on
repetitive schemas and not worth reaching for on prose.

## Testing

```bash
python3 -m pytest -q test_xjarchive.py     # 64 tests
node test_xjarchive.js                     # 62 checks
python3 make_vectors.py > vectors.json   # regenerate the shared corpus
```

`vectors.json` holds 26 cases as (tree, expected bytes). Both
implementations encode each tree and must produce exactly those bytes, then
decode those bytes and recover the tree. Two implementations that both match
a third file cannot quietly agree with each other on something the grammar
does not say.

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
advantage. xjarchive trades that for the bytes JSON spends on quotes — 0.979
against JsonML's 0.998 on the Wikipedia corpus.

Use JsonML when generic JSON tooling will touch the document — that is a real
advantage and worth 2%. Use xjarchive when the stream is going to your own next
stage, when content may contain arbitrary bytes, or when you need one
document to have exactly one encoding.

## Name and file extension

`xjarchive`, file extension **`.xja`**. Clear on npm, PyPI, crates.io and
GitHub repository search, and no hits as a format or product in a web
search.

Names considered and rejected:

| | why not |
|---|---|
| `tpack` | taken: npm 3.0.1 and PyPI 1.0.2 |
| `xjpack` / `xjpak` | **XJTAG's XJPack** — commercial boundary-scan software that packages project data into a portable `.xjp` file. Invisible to package registries, and adjacent enough in purpose to confuse. Their whole product line is `XJ`-prefixed, so any `xj*` name reads as theirs. |
| `jsonpack` | misleading: this carries the XML infoset, not JSON's model |
| `xjarchive` — **t**ag/**a**ttribute/**c**ontent packed | taken: **VRS TacPack**, a combat-simulation framework for Microsoft Flight Simulator X and Prepar3D, with an SDK and third-party developers |

Both misses were **commercial desktop software**, which no package registry
indexes. Checking npm/PyPI/crates/GitHub is necessary and not sufficient; a
plain web search caught what four registries missed, twice.

**Adjacency worth knowing about.** XJTAG's product line is XJ-prefixed
(XJPack, XJRunner, XJInvestigator, XJAnalyser), and XJPack packages project
data into a portable `.xjp` file. This name shares that prefix and is
adjacent in purpose. No conflict was found, but the neighbourhood is
occupied.

**Extension: `.xja`.** Rejected `.tpk` (ArcGIS tile packages, TI
calculators) and `.tac` (Python's Twisted application configuration). There
is no authoritative registry of file extensions, so this is chosen for being
unclaimed as far as a search shows rather than verified free.

## Files

| | |
|---|---|
| [`GRAMMAR.md`](GRAMMAR.md) | the specification; the authority |
| `xjarchive.py` / `xjarchive.js` | independent reference implementations |
| `test_xjarchive.py` / `test_xjarchive.js` | tests written from the grammar |
| `vectors.json` | shared conformance corpus |
| `make_vectors.py` | regenerates it |

## Licence

BSD-2-Clause, as with the rest of this repository.
