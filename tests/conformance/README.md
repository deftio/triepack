# Cross-language conformance corpus

One list of cases that every Triepack implementation runs, so the C library and
the eight bindings cannot drift apart without a test noticing.

```
tests/conformance/
  cases.txt      the corpus: what each case contains  (generated, checked in)
  fixtures/      one .trp per case, from the C encoder (generated, checked in)
  malformed/     buffers every reader must reject      (generated, checked in)
```

Each implementation has a harness that reads `cases.txt` and, for every case:

1. **decodes** `fixtures/<name>.trp` and checks the values against the corpus;
2. **encodes** the corpus values and checks the bytes against the same file.

Byte-for-byte encoding is the strong claim: any implementation can produce a
`.trp` another can read, and two implementations given the same input produce
the same file.

## Regenerating

`cases.txt` comes from a script rather than being edited by hand, so adding a
case means editing `tools/gen_conformance_cases.py` and rerunning both steps:

```sh
python3 tools/gen_conformance_cases.py            # rewrite cases.txt
cmake --build build --target fixtures             # rewrite fixtures/
python3 tools/gen_malformed_fixtures.py           # rewrite malformed/
```

The `fixtures` target also refreshes the older `tests/fixtures/` set; the seven
cases the two have in common are byte-identical, and a test would fail if they
stopped being so.

`tools/generate_conformance.c` is the only thing that writes `fixtures/`: the C
library is the reference encoder, and every other implementation is measured
against it.

## Corpus format

Line-oriented, so a parser is thirty lines in any language. Blank lines and
lines starting with `#` are ignored.

```
case <name> [encode=no] [requires=int64]
key <key> <type> [<arg>]
```

`case` starts a case; the `key` lines that follow belong to it, in corpus
order. Flags:

| Flag             | Meaning                                                                |
|------------------|------------------------------------------------------------------------|
| `encode=no`      | Decode-only. The values cannot be re-encoded exactly by every language. |
| `requires=int64` | Values need more than 53 bits; implementations that keep integers in a double skip the case. |

### Types

| Type   | Argument                                                     |
|--------|--------------------------------------------------------------|
| `null` | none                                                         |
| `bool` | `0` or `1`                                                   |
| `int`  | decimal, always negative (see below)                         |
| `uint` | decimal, non-negative                                        |
| `f32`  | 8 hex digits: the IEEE-754 binary32 bit pattern              |
| `f64`  | 16 hex digits: the IEEE-754 binary64 bit pattern             |
| `str`  | escaped token                                                |
| `blob` | hex digit pairs, or `~` for empty                            |

Floats are written as bit patterns so no case depends on how a language parses
or prints a decimal literal, and so `-0.0` and NaN survive the round trip.

### Escaped tokens

Keys and `str` values are UTF-8 bytes where anything outside
`[A-Za-z0-9_.-]` is written `%XX` (uppercase hex). A single `~` means the empty
string — a `~` in the data itself escapes to `%7E`, so there is no ambiguity.

### Why `int` is always negative

JavaScript has one number type, so the encoder picks the `int` or `uint` tag
from the sign. A non-negative value tagged `int` could therefore never be
reproduced there. The corpus sidesteps it: `uint` covers zero and up, `int`
covers negatives, and every language maps the two unambiguously.

For the same reason an *integral* double (`3.0`, or `f64::MAX`) only appears in
decode-only cases — JavaScript cannot tell one from an integer.

## What the cases cover

- The original `tests/fixtures/` set, so the two stay in step
- Trie shapes: empty keys, terminals with children, long single chains, wide
  branches, keys that differ only at the end
- The trailing-BRANCH key sets from issue #1
- `bits_per_symbol` at each power-of-two boundary (3 through 8) and the widest
  alphabet reachable from UTF-8 keys
- Keys: Unicode, every UTF-8 length class, 1000 characters long
- Values: varint group boundaries, the full 64-bit signed and unsigned range,
  float specials, empty and 256-byte blobs, empty and 500-character strings
- Scale: 100, 200 and 500 keys, shared prefixes, sparse keys

## Malformed inputs

`malformed/` holds valid fixtures with one field damaged. Where the damage is
inside the data the CRC is recomputed, so a reader has to catch the problem
itself rather than being handed a checksum failure. Every implementation
asserts that all of them are rejected.

Covered: bad magic, unsupported version, wrong CRC, truncation in the header
and in the body, and a trie config that is out of range (`bits_per_symbol` of
0, 9 or 15; `symbol_count` of 0, below the six control codes, or wider than
`bits_per_symbol` can address).
