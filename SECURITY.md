# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 2.0.x   | Yes       |
| 1.3.x   | Yes       |
| < 1.3   | No        |

2.0.0 reads and writes the same on-disk format as 1.3.x, so a fix to the
decoder applies to both.

Fixes land on `main` and ship in the next release.

## Reporting a vulnerability

Please report privately rather than opening a public issue.

- Preferred: [GitHub private vulnerability reporting](https://github.com/deftio/triepack/security/advisories/new)
- Or email: deftio@deftio.com

Please include:

- The affected component — the C library, the C++ wrapper, or a named binding
- Version, OS and toolchain
- A minimal reproduction. For a parsing issue, the `.trp` buffer (hex dump or
  base64) or the key set that produces it
- What an attacker could do with it

You should get an acknowledgement within a few days, an assessment within two
weeks, and credit in the release notes unless you would rather not be named.

## Scope

TriePack parses untrusted binary input, so the decoder is the part that
matters most. Of particular interest:

- Out-of-bounds reads or writes while walking a trie or a value store
- A crafted header that makes a reader over-read, loop forever, or allocate
  without bound
- A buffer that passes CRC validation but decodes to something other than
  what it encodes
- Integer overflow in offset, length or bit-position arithmetic
- Memory-ownership bugs in the C API — a double free or a use-after-free from
  an ordinary call sequence

Note that a valid CRC means the buffer is intact, not that it is trustworthy:
an attacker who can supply a whole buffer can supply a matching checksum. Treat
CRC-32 as corruption detection, never as authentication. Any input from an
untrusted source should be opened with the full validating path
(`tp_dict_open`, not `tp_dict_open_unchecked`).

## terseml

[`terseml/`](terseml/) is a subproject in this repository, not part of the
TriePack library, and is not published to any registry. It parses untrusted
bytes too, so the same interests apply — particularly a declared `\B` run
length that is not validated against the remaining input before allocating,
which is the shape that produced a real denial of service in this codebase.

Report terseml issues through the same private path.

## Out of scope

- Denial of service from input you chose to hand the library, such as a
  legitimately enormous dictionary
- Results from `tp_dict_open_unchecked`, which skips CRC validation by design
- Anything requiring an attacker who already has arbitrary code execution in
  your process
