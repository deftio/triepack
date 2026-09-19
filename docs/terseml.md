---
layout: default
title: terseml
---

# terseml

<!-- Copyright (c) 2026 M. A. Chatterjee -->

**A sibling subproject, not a TriePack feature.** terseml lives in this
repository because it came out of TriePack's work on how documents spend
their bytes, and because the two are often useful on the same data. It links
nothing from TriePack and TriePack links nothing from it. It is not published
to any registry.

The full standalone description, including build instructions, is
[here](pages/terseml.md). The source and its specification are in
[`terseml/`](https://github.com/deftio/triepack/tree/main/terseml), with
[`GRAMMAR.md`](https://github.com/deftio/triepack/blob/main/terseml/GRAMMAR.md)
as the authority.

## What it is

A positional encoding for tag / attribute / content trees — the same
information an XML element or a JsonML array carries, with the field names
removed because position already says what each slot is.

```
<a href="/x">click</a>          25 bytes  XML
["a",{"href":"/x"},"click"]     27 bytes  JsonML
{a,[href:/x],click}             19 bytes  terseml
```

It is a **byte format that happens to be readable**, not a text format: a
document is a byte string, keys and content may hold NUL or any other byte,
and nothing assumes UTF-8.

## How it works

An element is `{tag,[attrs],content}`. Position carries the meaning, so three
things follow:

1. **The tag is whatever precedes the first comma.** No quotes, no closing
   tag, no repetition of the name at the end.
2. **Attributes are a single optional `[...]` block**, and only in the slot
   right after that comma. `[` anywhere else is an ordinary byte.
3. **Everything after is content** — text and nested elements, in order,
   with nothing separating them.

The consequence that matters for size is that **reserved bytes are
positional**. Only the first comma of an element is structural; a comma in
text is just a comma. `[` is reserved only as the first byte of the content
slot. Escaping more than the grammar requires still round-trips, which is
why it is easy to get wrong and expensive when you do — over-escaping 16 MB
of Wikipedia cost 888,576 bytes, 5.3% of the file, and inverted a comparison
against JsonML.

Binary payloads have two run forms so that no payload has to be escaped into
something larger than itself: `\b` (NUL-terminated, escapes only NUL and
backslash) and `\B` (length-prefixed, escapes nothing and can be skipped
without inspection). An encoder **must** emit whichever of inline, `\b` and
`\B` is shortest, so one tree always produces one byte string.

## What it is for

A transport and preprocessing format. **It is not a compressor.** It removes
the bytes a tree spends on field names and leaves a compact, unambiguous,
byte-transparent stream for whatever comes next.

That gives a single rule, which holds on every corpus measured:

> **Compression tracks how much of the document is field names, and nothing
> else.**

A product catalogue that is 26.6% field names encodes to 0.628 of its
original size — more than the field names, because JSON's quotes around
values go too. Wikipedia XML, which is 2.7% tags and the rest prose, encodes
to 0.979, and no format that preserves the text can do better because there
is nothing else to remove.

## Relationship to TriePack

They solve different problems on the same data and **do not pipe into each
other**:

| | TriePack | terseml |
|---|---|---|
| Model | dictionary: keys with optional values | tree: ordered children, attributes, mixed content |
| Job | compress and query in place | remove field names for transport |
| Output | `.trp`, queried without decompression | `.tsml`, a stream for the next stage |
| Status | published, v1 format stable | unpublished, in-repo |

TriePack stores a dictionary; terseml emits a tree. Feeding one to the other
would need a design that does not exist yet — a symbol table for tags and
attribute names, replaced by ordinals in the stream. That is a third thing,
and it has not been built or measured.

## Implementations

C, Python and JavaScript, each written independently rather than as bindings
over one another, all checked against a shared conformance corpus generated
from one source. That is the point: implementations that *can* disagree,
measured against a common file, are the only way to find out whether a
grammar is actually unambiguous.

It wasn't, three times. Each contradiction is recorded in
[`GRAMMAR.md`](https://github.com/deftio/triepack/blob/main/terseml/GRAMMAR.md)
with the measurement that found it.

See [the standalone page](pages/terseml.md) for the API, build instructions,
code size and throughput.
