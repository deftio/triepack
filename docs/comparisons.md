---
layout: default
title: Comparisons
---

# How TriePack Compares

<!-- Copyright (c) 2026 M. A. Chatterjee -->

TriePack sits between two families of format, and the honest answer is
different for each. Every number on this page was measured on this machine
against `tests/data/benchmark_100k.json` (202,408 bytes, 5,043 keys once
flattened) and `tests/data/common_words_10k.txt`; the commands are at the
bottom so you can check them.

Where TriePack loses, this page says so.

## 1. Against JSON document formats

BSON, MessagePack and CBOR are **document formats**: they preserve an
object graph and let you walk it. TriePack is a **dictionary format**: it
maps byte-string keys to typed values, and its JSON layer flattens a document
into dotted keys (`items[3].price`) to fit that shape.

That difference decides everything below.

### Measured on `benchmark_100k.json`

| Format | Bytes | vs JSON |
|---|---:|---:|
| Raw JSON | 202,408 | 1.00× |
| BSON | 144,545 | 1.40× |
| MessagePack | 120,143 | 1.68× |
| TriePack v1 (`.trp`) | 134,200 | 1.51× |
| gzip(JSON) −9 | 15,412 | **13.13×** |

TriePack v1 lands between BSON and MessagePack, and **gzip beats all of them
by an order of magnitude.** If your only goal is bytes on disk for a document
you will decompress in one go, use gzip. That is not a close call and no
amount of format work changes it.

### Where TriePack actually wins: repeated keys

BSON stores every field name **in full, once per occurrence**, as a
NUL-terminated string. There is no sharing. On this corpus:

| | Bytes | Share of file |
|---|---:|---:|
| BSON total | 144,545 | |
| — of which field names | **45,476** | **31%** |
| — everything else | 99,069 | 69% |

Nearly a third of a BSON document can be the same field names written over
and over. That is exactly what a trie removes. The v2 prototype stores the
whole flattened key set — all 5,043 keys, 129,460 bytes of raw key text — in
**13,116 bytes**:

| Key storage | Bytes |
|---|---:|
| Raw key text | 129,460 |
| BSON field names | 45,476 |
| **TriePack v2 key structure** | **13,116** |

**9.87× against the raw key text, 3.47× against BSON's field names.** The
reason is visible in the build: 5,974 tail occurrences collapse to **41
distinct tails**, because a JSON schema repeats the same field names across
every element of every array.

### Stacking a general compressor on top

Since a `.trp` is just bytes, you can compress it again. Two naive attempts,
measured on the real v2 artifact:

| Corpus | v2 | v2 + gzip | v2 + byte Huffman | gzip on the raw file |
|---|---:|---:|---:|---:|
| `common_words_10k.txt` | 26,836 | **15,863** | 25,730 | 27,973 |
| `benchmark_100k.json` | 91,780 | 18,206 | 72,431 | **15,412** |
| 20k path-like keys | 276,426 | 190,794 | 243,655 | **150,525** |
| 20k random binary keys | 237,923 | 228,479 | 237,089 | **172,738** |
| enwik9, first 16 MB | 15,108,237 | 6,909,850 | 10,584,524 | **6,191,347** |

**gzip on the `.trp` beats gzip on the original only for the word list** —
15,863 against 27,973, a further 1.76×. Everywhere else gzipping the original
wins outright.

The reason is that the two are competing for the same redundancy. A trie
removes shared prefixes and suffixes, which is most of what LZ77 would have
found; feeding the result to gzip leaves it less to work with, while the
trie's own indexes and bit-packed arrays are close to incompressible. Only
where the structural win is large and the residual text still repeats — a
sorted word list — does stacking pay.

**Byte-level Huffman is not worth it.** It never beats gzip and barely beats
the uncompressed `.trp`: 4% on the word list, 30% on the JSON keys. gzip is
LZ77 *plus* Huffman, so plain Huffman only ever collects the smaller half of
what gzip already does.

Both of these destroy random access, which is the reason to use TriePack at
all. A gzipped `.trp` must be fully decompressed before a single key can be
read — at which point you are back to the gzip-the-JSON row, which is
smaller. **Stacking is worth it only when the file is an archive you will
expand before use.**

### So which should you use?

| If you need | Use |
|---|---|
| Smallest bytes for a document read whole | **gzip / zstd over JSON** |
| A document graph you walk and mutate | **BSON, MessagePack, CBOR** |
| Random key lookup without decompressing | **TriePack** |
| Read directly from flash, no RAM copy | **TriePack** |
| The same file read identically by ten languages | **TriePack** |

The distinction that matters is **random access**. gzip gets 13× but you must
decompress the whole document to read one field. BSON lets you scan to a
field but stores every name in full. TriePack looks up one key in ~2 µs,
touching only the bytes on that key's path, with the file still in flash and
never copied into RAM.

If you are going to decompress everything anyway, TriePack is the wrong tool
and this page would rather tell you than sell you.

## 2. Against trie and dictionary structures

Here TriePack is competing on its own ground, and still does not win on
compression or speed.

| Library | Wins on | TriePack's position |
|---|---|---|
| `marisa-trie` | compression of large dictionaries | loses; recursive nested tries are far more compact |
| `xcdat` | compressed double-array tries | loses on size |
| `darts-clone` | raw lookup throughput | loses; double-array is O(1) per character |
| `fst` (Rust) | fuzzy search, memory mapping | loses outright — TriePack has no fuzzy search at all |
| `sdsl-lite` | succinct structure toolkit | different thing; a library, not a format |

These have not been benchmarked head to head here, so the table states
architectural expectations rather than measurements. Taking any of those
crowns is explicitly a non-goal
([North Star §3](triepack-northstar.md)).

**What TriePack has that they do not** is a single artifact that ten
independent native implementations produce and consume byte-for-byte
identically, enforced on every build, in about 4,600 lines of C99 with no
dependencies and a read path that runs from ROM. `fst` is Rust. `marisa` is
C++ with FFI wrappers, which are bindings rather than independent
implementations — there is nothing for them to disagree about. TriePack has
ten things that *could* disagree and a conformance corpus that fails when
they do.

That is the trade: portability and in-place readability, bought with
compression ratio and lookup speed.

## 3. Where TriePack is the wrong answer

Stated plainly, because a comparison page that only lists strengths is
marketing:

- **Compressing a document you read whole** — gzip wins by 13×.
- **Keys with no shared structure.** Compression tracks prefix and suffix
  sharing and nothing else. Random binary keys measured at **148% of their
  input** — larger than what went in. Synthetic keys with hash-derived middles
  came out at 86–89%.
- **Anything mutable.** A `.trp` is built once. There is no insert.
- **Fuzzy or approximate matching.** `tp_dict_find_fuzzy` returns
  `TP_ERR_UNSUPPORTED` and will until someone implements a Levenshtein
  automaton ten times.
- **Very large dictionaries where every byte counts** — `marisa-trie` is the
  better tool.

### Would converting to JSON first help?

No — it makes things worse, and the reason is instructive. 16 MB of enwik9
XML, converted to JSON and encoded:

| | Bytes | Of the XML |
|---|---:|---:|
| Raw XML | 16,776,986 | 1.000 |
| The same content as JSON | 16,326,852 | 0.973 |
| **TriePack v2 on the XML lines** | 15,108,237 | **0.901** |
| **TriePack v2 on the JSON** | 15,825,734 | **0.943** |
| — key structure | 25,335 | 0.002 |
| — value store | 15,800,399 | 0.942 |
| gzip(XML) | 6,191,347 | 0.369 |
| TriePack v2 on the JSON, then gzip | 6,070,806 | 0.362 |

The key structure is the best result on this page — **11,335 keys in 25 KB,
two tenths of one percent** — because a JSON schema repeats
`pages[N].revision.text` for every article. And it does not matter, because
99.8% of the output is the value store, where article text sits with no
sharing at all.

As XML lines, that same text is split across many lines that share prefixes
and suffixes and get trie-compressed. As JSON, each article is one opaque
value. **Structuring the data moves content out of the part TriePack
compresses and into the part it does not.**

The practical reading: TriePack compresses *keys*. If your bytes are mostly
values, converting to JSON to get nicer keys optimises the 0.2% and leaves
the 99.8% alone.

## 4. The whole table

One command produces every number on this page:

```bash
cmake --build build --target v2_prototype
python3 tools/compare_formats.py           # the small corpora
python3 tools/compare_formats.py --full    # adds the whole 1 GB enwik9
```

It reports bytes and the ratio against each raw file, for every format, with
a refusals section naming anything a format declined and why.

## 5. Reproducing these numbers

```bash
# JSON formats
python3 -c "
import json, gzip, bson, msgpack
raw = open('tests/data/benchmark_100k.json','rb').read()
doc = json.loads(raw)
print('JSON       ', len(raw))
print('BSON       ', len(bson.encode(doc)))
print('MessagePack', len(msgpack.packb(doc)))
print('gzip -9    ', len(gzip.compress(raw, 9)))
"

# TriePack v1
cmake --build build --target run_benchmarks && ./build/tools/run_benchmarks

# TriePack v2 prototype, on any newline-separated key list
./build/tools/v2_prototype tests/data/common_words_10k.txt
```

v2 figures come from `tools/v2_prototype.c`, which verifies every key and
value through its own serialised bytes before reporting; see
[the v2 plan](internals/v2-implementation-plan.md).
