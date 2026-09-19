#!/usr/bin/env python3
"""xjarchive -- reference encoder and decoder.

Implements GRAMMAR.md exactly. This is the reference:
where the code and the grammar disagree, the grammar is right and the code is
a bug.

A node is one of:
    (tag, attrs, children)   an element; attrs is a list of (key, value)
    bytes                    text

Everything is bytes. A xjarchive document is a byte string and the decoder must
not assume UTF-8 -- an encoding that only works for text is one that silently
corrupts binary.

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

from dataclasses import dataclass, field
from typing import List, Tuple, Union

IMPLICIT_TAG = b"#text"
MAX_DEPTH = 256
VARINT_MAX_GROUPS = 10


class XjarchiveError(ValueError):
    """Malformed input. The decoder rejects rather than guesses (grammar §6)."""


@dataclass
class Element:
    tag: bytes
    attrs: List[Tuple[bytes, bytes]] = field(default_factory=list)
    children: List[Union["Element", bytes]] = field(default_factory=list)


Node = Union[Element, bytes]


# ---------------------------------------------------------------------------
# Encoding
# ---------------------------------------------------------------------------

def _escape(raw: bytes, reserved: bytes, escape_leading: bytes = b"") -> bytes:
    """Escape exactly the reserved bytes and no others (grammar §5, §7 rule 4).

    Over-escaping still round-trips, so it passes naive tests while costing
    bytes -- on Wikipedia text, escaping commas and brackets that never needed
    it cost 888,576 bytes and inverted a comparison.
    """
    out = bytearray()
    for i, b in enumerate(raw):
        ch = bytes([b])
        if ch == b"\\" or ch in _split(reserved) or (i == 0 and ch in _split(escape_leading)):
            out += b"\\" + ch
        else:
            out += ch
    return bytes(out)


def _split(bs: bytes) -> set:
    return {bytes([b]) for b in bs}


def _varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def _read_varint(buf: bytes, i: int):
    n = shift = groups = 0
    while True:
        if i >= len(buf):
            raise XjarchiveError("truncated varint")
        groups += 1
        if groups > VARINT_MAX_GROUPS:
            raise XjarchiveError("varint longer than 10 groups")
        b = buf[i]
        i += 1
        n |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return n, i


def _size_inline(b: bytes) -> int:
    return len(b) + sum(1 for x in b if x in b"{}\\")


def _size_nulterm(b: bytes) -> int:
    return 2 + len(b) + sum(1 for x in b if x in b"\x00\\") + 1


def _size_counted(b: bytes) -> int:
    return 2 + len(_varint(len(b))) + len(b)


def choose_form(payload: bytes, leading_bracket: bool = False) -> str:
    """Grammar §5.1: shortest wins; ties break counted, terminated, inline.

    Mandatory rather than advisory -- three legal encodings of one payload
    would otherwise mean three legal files for one document, and
    byte-identical output is the property everything here rests on.
    """
    c = _size_counted(payload)
    t = _size_nulterm(payload)
    i = _size_inline(payload) + (1 if leading_bracket and payload[:1] == b"[" else 0)
    best = min(c, t, i)
    if c == best:
        return "counted"
    if t == best:
        return "terminated"
    return "inline"


def _encode_binary(payload: bytes, form: str) -> bytes:
    if form == "counted":
        return b"\\B" + _varint(len(payload)) + payload
    if form == "terminated":
        body = payload.replace(b"\\", b"\\\\").replace(b"\x00", b"\\0")
        return b"\\b" + body + b"\x00"
    raise ValueError(form)


def _merge_text(children: List[Node]) -> List[Node]:
    """Canonical form §7 rule 5: nothing separates adjacent text on the wire,
    so ["a", "b"] and ["ab"] are the same document. Merge on the way out, and
    the decoder's output is then well defined."""
    out: List[Node] = []
    for c in children:
        if isinstance(c, (bytes, bytearray)) and out and isinstance(out[-1], (bytes, bytearray)):
            out[-1] = bytes(out[-1]) + bytes(c)
        else:
            out.append(c)
    return [c for c in out if not (isinstance(c, (bytes, bytearray)) and not c)]


def encode(node: Node, depth: int = 0) -> bytes:
    """Encode one node into canonical xjarchive (grammar §7)."""
    if depth > MAX_DEPTH:
        raise XjarchiveError(f"nesting deeper than {MAX_DEPTH}")

    if isinstance(node, (bytes, bytearray)):
        # Bare text at document level is a singleton element.
        return b"{" + _escape(bytes(node), b"{}", b"[") + b"}"

    tag = _escape(node.tag, b",{}")
    out = bytearray(b"{" + tag)

    if node.attrs:
        pairs = [
            _escape(k, b":,]") + b":" + _escape(v, b",]")
            for k, v in node.attrs
        ]
        # §7 rule 2: encoders never write the optional trailing comma.
        out += b",[" + b",".join(pairs) + b"]"

    children = _merge_text(node.children)
    if children:
        out += b","
        for i, child in enumerate(children):
            if isinstance(child, (bytes, bytearray)):
                # §4.2: only a leading '[' needs escaping, and only when it is
                # the first byte of the content slot.
                at_start = i == 0 and not node.attrs
                payload = bytes(child)
                form = choose_form(payload, at_start)
                if form == "inline":
                    out += _escape(payload, b"{}", b"[" if at_start else b"")
                else:
                    out += _encode_binary(payload, form)
            else:
                out += encode(child, depth + 1)
    elif node.tag != IMPLICIT_TAG:
        # §4.3: an element with a tag always has a comma, so that {foo} stays
        # available for the singleton.
        out += b","

    return bytes(out + b"}")


def encode_document(nodes: List[Node]) -> bytes:
    return b"".join(encode(n) for n in nodes)


# ---------------------------------------------------------------------------
# Decoding
# ---------------------------------------------------------------------------

class _Reader:
    def __init__(self, buf: bytes):
        self.buf = buf
        self.i = 0

    def eof(self) -> bool:
        return self.i >= len(self.buf)

    def peek(self) -> bytes:
        if self.eof():
            raise XjarchiveError("unexpected end of input")
        return self.buf[self.i : self.i + 1]

    def take(self) -> bytes:
        ch = self.peek()
        self.i += 1
        return ch

    def expect(self, ch: bytes) -> None:
        got = self.take()
        if got != ch:
            raise XjarchiveError(f"expected {ch!r} at offset {self.i - 1}, got {got!r}")


# Every reserved byte, per grammar §2: a byte reserved in one position must be
# escapable there, and the decoder cannot tell which position an escape came
# from without accepting them all.
_VALID_ESCAPES = _split(b"{}[],:\\")


def _read_escaped(r: _Reader, stop: bytes) -> bytes:
    """Read until an unescaped byte in `stop`. Does not consume the stop byte."""
    out = bytearray()
    while True:
        if r.eof():
            raise XjarchiveError("unterminated element")
        ch = r.peek()
        if ch == b"\\":
            r.take()
            nxt = r.take()
            if nxt not in _VALID_ESCAPES:
                # §6: an unknown escape means a version mismatch, not a literal.
                raise XjarchiveError(f"invalid escape \\{nxt.decode('latin-1')}")
            out += nxt
            continue
        if ch in _split(stop):
            return bytes(out)
        out += r.take()


def _read_binary(r: _Reader) -> bytes:
    r.expect(b"\\")
    kind = r.take()
    if kind == b"B":
        n, i = _read_varint(r.buf, r.i)
        # §6: validate the length against what is left BEFORE allocating. A
        # length read from the file and trusted is how a decoder is made to
        # abort on a ten-byte input.
        if n > len(r.buf) - i:
            raise XjarchiveError(f"binary run claims {n} bytes, {len(r.buf) - i} remain")
        r.i = i + n
        return r.buf[i : i + n]

    out = bytearray()
    while True:
        if r.eof():
            raise XjarchiveError("unterminated binary run")
        ch = r.take()
        if ch == b"\x00":
            return bytes(out)
        if ch == b"\\":
            nxt = r.take()
            if nxt == b"0":
                out += b"\x00"
            elif nxt == b"\\":
                out += b"\\"
            else:
                raise XjarchiveError(f"invalid escape in binary run: \\{nxt.decode('latin-1')}")
            continue
        out += ch


def _decode_element(r: _Reader, depth: int) -> Element:
    if depth > MAX_DEPTH:
        raise XjarchiveError(f"nesting deeper than {MAX_DEPTH}")
    r.expect(b"{")

    head = _read_escaped(r, b",{}")
    ch = r.peek()

    if ch == b"}":
        # §4.3: no comma -> singleton, content with the implicit tag.
        r.take()
        return Element(IMPLICIT_TAG, [], [head] if head else [])

    if ch == b"{":
        # A tag immediately followed by a child, with no comma, is not legal.
        raise XjarchiveError("element with children must have a comma after the tag")

    r.expect(b",")
    el = Element(head)

    # §4.2: attributes iff the byte after the comma is '['.
    if not r.eof() and r.peek() == b"[":
        r.take()
        if r.peek() != b"]":
            while True:
                key = _read_escaped(r, b":,]")
                if r.peek() != b":":
                    raise XjarchiveError("attribute pair without ':'")
                r.take()
                val = _read_escaped(r, b",]")
                el.attrs.append((key, val))
                if r.peek() == b",":
                    r.take()
                    if r.peek() == b"]":   # §7 rule 2: accepted, never emitted
                        break
                    continue
                break
        r.expect(b"]")
        if r.peek() == b",":
            r.take()
        elif r.peek() == b"}":
            r.take()
            return el

    # Content: text and child elements, in order.
    while True:
        if r.eof():
            raise XjarchiveError("unterminated element")
        ch = r.peek()
        if ch == b"}":
            r.take()
            return el
        if ch == b"{":
            el.children.append(_decode_element(r, depth + 1))
            continue
        if ch == b"\\" and r.buf[r.i + 1 : r.i + 2] in (b"b", b"B"):
            # §5.1. A letter after the escape byte introduces a command; the
            # reserved bytes are all punctuation, so the two cannot collide.
            el.children.append(_read_binary(r))
            continue
        text = _read_escaped(r, b"{}")
        if text:
            el.children.append(text)


def decode(buf: bytes) -> Element:
    r = _Reader(buf)
    el = _decode_element(r, 0)
    if not r.eof():
        raise XjarchiveError(f"trailing bytes at offset {r.i}")
    return el


def decode_document(buf: bytes) -> List[Element]:
    r = _Reader(buf)
    out = []
    while not r.eof():
        out.append(_decode_element(r, 0))
    return out
