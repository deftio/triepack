# terseml — Grammar

<!-- Copyright (c) 2026 M. A. Chatterjee -->

**Status: proposal.** A positional encoding for tag/attribute/content trees:
the same information a JSON object carries, with the field names removed
because position already says what each slot is.

```
{tag,[k:v,],content}
```

This document defines the grammar completely enough that an encoder and a
decoder written from it, in different languages, cannot disagree — the same
standard the [North Star](../docs/triepack-northstar.md) §4.2 sets for every other
part of this project. Every rule below that resolves an ambiguity says so and
says what it costs.

## 1. Why positional

A tag/attribute/content node in JSON spends its bytes on the field names:

```json
{"t":"a","a":{"href":"/x"},"c":"click"}      39 bytes
```

Position carries the same information:

```
{a,[href:/x,],click}                          20 bytes
```

The saving is proportional to how much of a document is field names, which
varies enormously — 26.6% of a product catalog, 2.7% of Wikipedia XML.
`bench_corpus.py` measures both; [README.md](README.md) has the tables.
(`../docs/comparisons.md` is about TriePack, a different format in the same
repository, and says nothing about this one.)

## 2. Grammar

ABNF, with the terminals defined in §3.

```abnf
document    = *node

node        = element / text

element     = "{" tag [ "," attributes ] [ "," content ] "}"
            / "{" content "}"                 ; singleton: implicit tag

tag         = 1*tagchar
attributes  = "[" [ pair *( "," pair ) [ "," ] ] "]"
pair        = key ":" value
key         = 1*keychar
value       = *valuechar

content     = *( element / text / binary )
text        = 1*( textchar / escape )
escape      = "\" ( "{" / "}" / "[" / "]" / "," / ":" / "\" )

binary      = terminated / counted
terminated  = "\b" *( binchar / binescape ) %x00
counted     = "\B" varint <exactly the number of bytes varint gives>
binescape   = "\" ( "0" / "\" )        ; \0 is a literal NUL byte
```

## 3. Terminals

| Production | Bytes allowed |
|---|---|
| `tagchar` | any byte except `,` `{` `}` `\` |
| *(any reserved byte may appear anywhere if escaped)* | |
| `keychar` | any byte except `:` `,` `]` `\` |
| `valuechar` | any byte except `,` `]` `\` |
| `textchar` | any byte except `{` `}` `\` |
| `binchar` | any byte except NUL and `\` |
| `varint` | LEB128: seven payload bits per byte, least significant group first, high bit set on all but the last; at most 10 groups |

Bytes, not characters: a terseml document is a byte string and a decoder
must not assume UTF-8. This matters for the same reason it matters in
[format v2 §7](../docs/internals/format-spec-v2.md) — an encoding that only works for text is
an encoding that silently corrupts binary.

## 4. The three ambiguities, and how they are resolved

A grammar that leaves these open produces encoders and decoders that each
work alone and disagree with each other. Each resolution below is a choice,
with its cost measured.

### 4.1 Inside content, does `{` begin a child or literal text?

Both readings of `{p,use {{tmpl}} here}` are self-consistent: content is the
text `use {{tmpl}} here`, or content is a child element `tmpl`.

**Resolution: `{` always begins a child element. A literal brace in text is
always escaped**, whether or not it is balanced.

Balance-counting was considered and rejected. It makes correctness depend on
a property of the *data* — an unbalanced brace anywhere silently reshapes the
tree rather than failing — which is the exact failure mode this project has
been bitten by twice.

**Cost, measured on 16 MB of Wikipedia XML: 32,019 bytes, 0.19% of the
file.** Cheap enough that the ambiguity is not worth keeping.

### 4.2 Are attributes present, or does content start with `[`?

`{a,[x]}` is an element with attribute set `x`, or an element whose content
is the text `[x]`.

**Resolution: attributes are present iff the byte after the tag's comma is
`[`. A literal `[` at the start of content is escaped.**

Only a leading `[` needs escaping; `[` anywhere else in content is an
ordinary byte. Wikipedia's 350,104 `[[link]]` markers therefore cost nothing.

### 4.3 Is `{foo}` a singleton, or an element with an empty content slot?

**Resolution: an element with a tag always has a comma. `{foo}` is a
singleton — content `foo` with the implicit tag. An element tagged `foo` with
empty content is `{foo,}`.**

The implicit tag is a parameter of the document, not of the grammar; a
profile using terseml states it (`#text` is the obvious choice).

## 5. What must be escaped, and what must not

The escape production (§2) accepts every reserved byte —
`{` `}` `[` `]` `,` `:` `\` — because a byte reserved in *one* position must
be escapable there, and a decoder cannot know which position a given escape
came from without accepting them all. An earlier draft listed only four, and
a randomised round-trip found the contradiction within three thousand trees:
the encoder wrote `\]` inside an attribute value and the decoder rejected its
own output.

The reserved bytes are **positional**, not global. This table is normative;
an implementation that escapes more is wrong in a way that costs bytes, and
one that escapes less is wrong in a way that corrupts.

| In | Escape | Leave alone |
|---|---|---|
| tag | `,` `{` `}` `\` | everything else |
| attribute key | `:` `,` `]` `\` | `{` `}` `[` |
| attribute value | `,` `]` `\` | `:` `{` `}` `[` |
| text content | `{` `}` `\`, and `[` **only as the first byte** | `,` `:` `]`, and `[` elsewhere |

There is no row for the singleton `{foo}` because an encoder never writes one
(§7 rule 7). A decoder reading one treats a `,` in it as structural, which is
exactly why encoders do not produce it.

Worked examples — all of these round-trip:

| Source content | Encoded | Note |
|---|---|---|
| `hello` | `{p,hello}` | |
| `a,b,c` | `{p,a,b,c}` | only the *first* comma is structural |
| `see [[link]]` | `{p,see [[link]]}` | `[` is not first, so not escaped |
| `[start]` | `{p,\[start]}` | leading `[` would read as attributes |
| `use {t}` | `{p,use \{t\}}` | braces always escaped (§4.1) |
| `back\slash` | `{p,back\\slash}` | |
| *(empty)* | `{p,}` | distinct from `{p}` |

## 5.1 Binary runs

Text escaping costs one byte per reserved byte, which is fine for prose and
bad for binary. Two run forms exist so that a payload never has to be escaped
into a shape larger than itself.

### `\b` — NUL-terminated

```
\b <bytes> 00
```

Inside the run, only two bytes are special: a NUL ends it, and `\` introduces
an escape. `\0` is a literal NUL byte (backslash followed by ASCII `0`, not a
raw NUL — a raw one would end the run), and `\\` is a literal backslash.
Every other byte, **including `{` `}` `[` `]` `,` `:`**, is itself.

A `\` inside a run followed by anything other than `0` or `\` is malformed.

### `\B` — length-prefixed

```
\B <varint length> <exactly that many bytes>
```

**Nothing inside is escaped.** Any byte value may appear, including NUL,
braces and backslashes. This is the only construct a parser can traverse
without inspecting its contents, which makes it the only one whose cost is
independent of what it carries.

### Which form to emit

All three encodings of a payload are legal, so the grammar would admit three
different files for one document. Byte-identical output is the property this
rests on, so the choice is **not** left to the encoder's taste:

> An encoder **must** emit whichever of inline text, `\b` and `\B` is
> strictly shortest. Ties break `\B`, then `\b`, then inline.

The tie-break favours the skippable form: the two are usually within a couple
of bytes, and skip-ability is worth more than two bytes.

Sizes, so an implementer can check the rule cheaply:

```
inline  = n + count of { } \ in the payload
\b      = 2 + n + count of NUL and \ + 1
\B      = 2 + varint_len(n) + n
```

Measured on 64 KB payloads, each form owns a region:

| payload | inline | `\b` | `\B` | shortest |
|---|---:|---:|---:|---|
| plain ASCII | **65,536** | 65,539 | 65,541 | inline |
| wiki markup | 65,668 | **65,539** | 65,541 | `\b` |
| brace-dense | 131,072 | **65,539** | 65,541 | `\b` |
| random binary | 66,267 | 66,052 | **65,541** | `\B` |
| 90% NUL, 10% random | 66,219 | 72,578 | **65,541** | `\B` |

`\b` loses badly on NUL-heavy data because NUL is the one byte it must
escape -- and note that *pure* NUL is cheapest **inline**, since NUL is not
reserved there at all; `\B` loses by a constant two bytes where `\b` wins. Below roughly 450
bytes of arbitrary binary, inline beats both — the three-byte header costs
more than the escapes it avoids.

## 6. Decoder obligations

A decoder **must** reject, rather than guess:

| Condition | Why |
|---|---|
| Unterminated element at end of input | truncation must not look like a valid short document |
| `\` followed by any byte other than `{` `}` `[` `]` `,` `:` `\` | an unknown escape means a version mismatch |
| An attribute pair with no `:` | |
| A tag containing a reserved byte unescaped | only reachable from a broken encoder |
| Nesting deeper than the profile's limit | a depth bound must exist and be stated |
| A `\B` length greater than the bytes remaining | **check before allocating.** A length read from the file and trusted is how a decoder is made to abort on a 10-byte input; see the note below |
| An unterminated `\b` run | truncation must not read past the buffer |
| Inside `\b`, `\` followed by anything but `0` or `\` | unknown escape |
| A `varint` longer than 10 groups | malformed, not merely large |

> **On `\B` lengths.** This is the shape that produced a real denial of
> service in a sibling project: `read_bytes(n)` allocated `n` before checking
> the stream had `n`, so a corrupt length aborted the process in one language
> and raised the wrong error in two others. Validate the length against the
> remaining input **first**, then allocate.

Rejection is a returned error. Silently repairing a malformed document is
what makes two implementations disagree.

## 7. Canonical form

Encoders **must** produce the canonical form, so that the same tree yields
the same bytes everywhere:

1. Attributes are emitted in the order given; a profile requiring stable
   output across languages must sort them, since hash-map order is not
   portable.
2. The optional trailing comma inside `[...]` is **not** emitted. Decoders
   accept it; encoders never write it.
3. An element with no attributes omits the slot entirely — `{a,x}`, never
   `{a,[],x}`.
4. No byte is escaped that §5 does not require.
5. **Binary runs use the shortest of the three forms** (§5.1), with ties
   broken `\B`, `\b`, inline.
6. **Adjacent text children are merged.** Nothing separates one text node
   from the next on the wire, so `["a", "b"]` and `["ab"]` are necessarily the
   same bytes. An encoder given adjacent text nodes concatenates them; a
   decoder always produces at most one text node between any two elements.
   For the same reason an **empty** text child is dropped: `{p,}` is an
   element with no children, not one holding a zero-length string.
7. **The singleton form is accepted but never emitted.** `{foo}` means what
   §4.3 says it means; an encoder holding that tree writes `{#text,foo}`.

Rule 7 is the cost of rule 4. Inside `{foo}` there is no content slot yet, so
a comma in the content *is* structural there and nowhere else — `{a,b}` is
tag `a`, not the text `a,b`. Keeping the short form would mean a fifth column
in the §5 table and a fifth escaping context in every implementation, all to
save the six bytes of `#text,` on a construct that a real document reaches
only at its root. A differential test between two implementations found this:
one wrote `{a,b}` for the text `a,b` and the other read it back as an element.

Rule 5 is a consequence of the format rather than a preference — it was found
by a randomised round-trip, not by inspection. Stating it makes the decoder's
output well defined; leaving it implicit would let two implementations
disagree about `["a","b"]` versus `["ab"]` while both "round-trip".

Rule 4 is the one worth testing hardest. Over-escaping still round-trips, so
it passes every naive test while quietly costing bytes — on Wikipedia text,
escaping commas and brackets that never needed it cost **888,576 bytes**,
5.3% of the file, and inverted a comparison.

## 8. Relationship to JsonML

[JsonML](http://www.jsonml.org/) expresses the same idea in valid JSON:
`["tag", {attributes}, child, ...]`. It needs no custom parser, which is a
real advantage. terseml trades that for the bytes JSON spends on quotes:
measured on 16 MB of Wikipedia XML, **terseml 0.979 of the original against
JsonML's 1.004** — though once gzipped the two converge (0.368 vs 0.370).
Reproduce with `python3 bench_corpus.py path/to/enwik9`.

Choose JsonML when the document will be handled by generic JSON tooling.
Choose terseml when it is going into a format that will re-encode it anyway.

## 9. Markup normalisation (optional profile)

A profile may map source-specific constructs to typed nodes rather than
leaving them as text — Wikipedia's `[[link]]` and `{{template}}` being the
motivating case:

```
[[Foo]]      ->  \u[Foo]      ; wiki link
{{Foo}}      ->  \s[Foo]      ; template
```

**This is size-neutral as a substitution** — `[[Foo]]` and `\u[Foo]` are both
seven bytes. Its value is structural: a typed node makes the target a field
that a trie can share, where opaque text cannot. It also removes braces from
text entirely, which makes §4.1's escaping cost disappear.

Measured on 16 MB of Wikipedia text:

| Construct | Occurrences | Distinct | Duplication |
|---|---:|---:|---:|
| `{{templates}}` | 7,261 | 1,155 | **84%** |
| `[[links]]` | 174,265 | 99,723 | 33% |

Templates repeat heavily and would share well; links much less. Worth doing
for templates, marginal for links — and to be measured per corpus rather than
assumed, the same rule [format v2 §8.2](../docs/internals/format-spec-v2.md) applies to
Huffman.

## 10. Conformance

Three independent implementations live beside this document — `terseml.c`,
`terseml.py` and `terseml.js` — none of them a binding over another. Their
tests are written from this document rather than from any implementation, so
the grammar is the authority ([North Star
§4.2](../docs/triepack-northstar.md)). `make check` runs all of them.

They cover: every row of the §5 table, every rejection in §6, the canonical
form of §7, and a randomised round-trip over trees built from adversarial
byte strings — every reserved byte, in every position, including empty tags,
empty content and deep nesting.

`vectors.json` and `vectors.h` are the shared corpus, generated together by
`make_vectors.py` so that no implementation can pass by testing a different
file. Each encodes every tree to exactly the listed bytes and decodes those
bytes back to the tree.

**Three implementations is the point, not redundancy.** Two of this
document's contradictions were found by a randomised round-trip, and the
third only by running one implementation's bytes through another's parser —
it had survived 3,000 random trees in *each*, because each decoded its own
output the way it had encoded it. An ambiguity that every implementation
resolves the same way by accident is invisible until one of them does not.
