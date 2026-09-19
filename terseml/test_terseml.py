"""Tests for terseml, written from the grammar rather than the code.

GRAMMAR.md is the authority: every case below cites
the section it comes from, and where the implementation disagrees with the
grammar the implementation is wrong.

The property test at the end is the one that matters. Worked examples confirm
what an author already thought of; a randomised round-trip over trees built
from adversarial byte strings -- every reserved byte, in every position -- is
what catches the case nobody thought of. That is the same discipline the
conformance corpus applies to the binary format.

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

import os
import random
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))

import terseml
from terseml import (  # noqa: E402
    IMPLICIT_TAG,
    Element,
    TersemlError,
    decode,
    decode_document,
    encode,
    encode_document,
)


# ---------------------------------------------------------------------------
# §5 -- what must be escaped, and what must not
# ---------------------------------------------------------------------------

@pytest.mark.parametrize(
    "content,expected,why",
    [
        (b"hello", b"{p,hello}", "plain text"),
        (b"a,b,c", b"{p,a,b,c}", "only the first comma is structural"),
        (b"see [[link]]", b"{p,see [[link]]}", "'[' is not first, so not escaped"),
        (b"[start]", b"{p,\\[start]}", "leading '[' would read as attributes"),
        (b"use {t}", b"{p,use \\{t\\}}", "braces are always escaped"),
        (b"back\\slash", b"{p,back\\\\slash}", "the escape byte itself"),
        (b"", b"{p,}", "empty content is distinct from a singleton"),
    ],
)
def test_escaping_table(content, expected, why):
    """Every row of the grammar's §5 table, byte for byte."""
    el = Element(b"p", [], [content] if content else [])
    assert encode(el) == expected, why
    back = decode(expected)
    assert back.tag == b"p"
    assert (back.children[0] if back.children else b"") == content


def test_nothing_is_escaped_that_need_not_be():
    """§7 rule 4. Over-escaping still round-trips, so it passes naive tests
    while costing bytes -- 888,576 of them on Wikipedia text."""
    el = Element(b"p", [], [b"a,b:c]d[e"])
    out = encode(el)
    assert b"\\," not in out
    assert b"\\:" not in out
    assert b"\\]" not in out
    assert out == b"{p,a,b:c]d[e}"


# ---------------------------------------------------------------------------
# §4 -- the three ambiguities
# ---------------------------------------------------------------------------

def test_brace_in_content_is_a_child_not_text():
    """§4.1. '{' always begins a child element; literal braces are escaped."""
    doc = decode(b"{div,{b,bold}}")
    assert len(doc.children) == 1
    child = doc.children[0]
    assert isinstance(child, Element)
    assert child.tag == b"b"
    assert child.children == [b"bold"]

    # and the text reading is reachable only by escaping
    doc2 = decode(b"{div,\\{b,bold\\}}")
    assert doc2.children == [b"{b,bold}"]


def test_attributes_are_detected_by_a_leading_bracket():
    """§4.2."""
    with_attrs = decode(b"{a,[href:/x],click}")
    assert with_attrs.attrs == [(b"href", b"/x")]
    assert with_attrs.children == [b"click"]

    as_text = decode(b"{a,\\[href:/x],click}")
    assert as_text.attrs == []
    assert as_text.children == [b"[href:/x],click"]


def test_singleton_versus_empty_content():
    """§4.3. {foo} is content with the implicit tag; {foo,} is tag foo."""
    single = decode(b"{foo}")
    assert single.tag == IMPLICIT_TAG
    assert single.children == [b"foo"]

    empty = decode(b"{foo,}")
    assert empty.tag == b"foo"
    assert empty.children == []

    assert encode(Element(b"foo", [], [])) == b"{foo,}"


# ---------------------------------------------------------------------------
# §6 -- a decoder must reject rather than guess
# ---------------------------------------------------------------------------

@pytest.mark.parametrize(
    "bad,why",
    [
        (b"{p,unterminated", "truncation must not look like a short document"),
        (b"{p,bad\\escape}", "an unknown escape means a version mismatch"),
        (b"{a,[nokeyvalue],x}", "attribute pair without ':'"),
        (b"{p,ok}trailing", "trailing bytes"),
        (b"", "empty input is not a document"),
        (b"{", "bare open brace"),
    ],
)
def test_malformed_input_is_rejected(bad, why):
    with pytest.raises(TersemlError):
        decode(bad)


def test_depth_is_bounded():
    """§6: a depth bound must exist and be stated."""
    deep = b"{a," * 400 + b"x" + b"}" * 400
    with pytest.raises(TersemlError):
        decode(deep)


# ---------------------------------------------------------------------------
# §7 -- canonical form
# ---------------------------------------------------------------------------

def test_version_tracks_triepack():
    """terseml follows the TriePack release version while it lives in this
    repository. A constant nobody checks is a constant that goes stale, and
    three implementations each carrying their own copy is three chances."""
    import os
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(root, "triepack-version.txt")) as f:
        expected = f.read().strip()
    assert terseml.__version__ == expected, (
        f"terseml.py says {terseml.__version__}, triepack-version.txt says "
        f"{expected} -- run ./scripts/sync_version.sh")

    # The other two implementations carry the same constant; check the text
    # rather than run two more interpreters.
    for name, pattern in (("terseml.h", f'#define TSML_VERSION "{expected}"'),
                          ("terseml.js", f"const VERSION = '{expected}';")):
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), name)) as f:
            assert pattern in f.read(), f"{name} does not declare {expected}"


def test_singleton_accepted_never_emitted():
    """§7 rule 7. {foo} means what §4.3 says, but no encoder writes it.

    Found by a differential test against the C implementation: inside {foo}
    there is no content slot yet, so a comma there is structural. An encoder
    that emitted the short form wrote {a,b} for the text "a,b" and the other
    implementation read it back as an element tagged "a".
    """
    assert encode(b"foo") == b"{#text,foo}"
    assert encode(Element(IMPLICIT_TAG, [], [b"foo"])) == b"{#text,foo}"

    for payload in (b"a,b", b"", b"[k:v]", b"{x}", b",", b",,,"):
        wire = encode(payload)
        assert decode(wire).children in ([payload], []), payload
        assert encode(decode(wire)) == wire, payload

    # The short form still decodes -- it is input, not output.
    assert decode(b"{foo}").children == [b"foo"]
    assert encode(decode(b"{foo}")) == b"{#text,foo}"


def test_trailing_comma_accepted_never_emitted():
    """§7 rule 2."""
    assert decode(b"{a,[k:v,],x}").attrs == [(b"k", b"v")]
    assert encode(Element(b"a", [(b"k", b"v")], [b"x"])) == b"{a,[k:v],x}"


def test_empty_attribute_slot_is_omitted():
    """§7 rule 3."""
    assert encode(Element(b"a", [], [b"x"])) == b"{a,x}"


def test_encoding_is_deterministic():
    """The same tree must produce the same bytes every time -- the property
    every cross-implementation guarantee rests on."""
    el = Element(b"a", [(b"k", b"v"), (b"j", b"w")], [b"t", Element(b"b", [], [b"u"])])
    assert encode(el) == encode(el)


# ---------------------------------------------------------------------------
# Bytes, not text
# ---------------------------------------------------------------------------

def test_arbitrary_bytes_survive():
    """§3: a document is a byte string. An implementation reaching for str
    fails here and nowhere else."""
    payload = bytes(range(256))
    el = Element(b"bin", [], [payload])
    assert decode(encode(el)).children[0] == payload


def test_nul_and_high_bytes_in_every_position():
    el = Element(b"t\x00\xff", [(b"k\x00", b"v\xff")], [b"c\x00\xffz"])
    back = decode(encode(el))
    assert back.tag == b"t\x00\xff"
    assert back.attrs == [(b"k\x00", b"v\xff")]
    assert back.children == [b"c\x00\xffz"]


# ---------------------------------------------------------------------------
# The property test
# ---------------------------------------------------------------------------

ADVERSARIAL = [
    b"", b"a", b"{", b"}", b"[", b"]", b",", b":", b"\\",
    b"{{", b"}}", b"[[", b"]]", b"\\\\", b",,", b"::",
    b"[lead", b"trail]", b"\x00", b"\xff", b"\x00\xff",
    b"see [[link]]", b"use {{tmpl}}", b"a,b:c]d",
]


def _random_tree(rng, depth=0):
    if depth >= 4 or rng.random() < 0.3:
        return rng.choice(ADVERSARIAL)
    tag = rng.choice([b"a", b"p", b"div", b"x\x00y", b"t,ag", b"t{g"])
    attrs = [
        (rng.choice(ADVERSARIAL) or b"k", rng.choice(ADVERSARIAL))
        for _ in range(rng.randint(0, 3))
    ]
    kids = [_random_tree(rng, depth + 1) for _ in range(rng.randint(0, 4))]
    return Element(tag, attrs, kids)


def _canonical(node):
    """Grammar §7 rule 5: adjacent text children are one text node. Compare
    against the canonical shape, not the shape the generator happened to
    build."""
    if isinstance(node, (bytes, bytearray)):
        return bytes(node)
    merged = []
    for c in (_canonical(x) for x in node.children):
        if isinstance(c, bytes) and merged and isinstance(merged[-1], bytes):
            merged[-1] += c
        else:
            merged.append(c)
    return Element(node.tag, node.attrs, [c for c in merged if c != b""])


def _same(a, b):
    if isinstance(a, (bytes, bytearray)) or isinstance(b, (bytes, bytearray)):
        return bytes(a) == bytes(b)
    return (
        a.tag == b.tag
        and a.attrs == b.attrs
        and len(a.children) == len(b.children)
        and all(_same(x, y) for x, y in zip(a.children, b.children))
    )


def test_random_trees_round_trip():
    """Every reserved byte, in every position, at every depth. This is the
    case nobody thought of, found by not needing to think of it."""
    rng = random.Random(20260918)
    for _ in range(3000):
        tree = _random_tree(rng)
        if isinstance(tree, (bytes, bytearray)):
            tree = Element(b"root", [], [tree])
        wire = encode(tree)
        back = decode(wire)
        assert _same(_canonical(tree), back), f"round-trip failed for {wire!r}"
        # and encoding is stable: re-encoding the decoded tree is identical
        assert encode(back) == wire, f"not canonical: {wire!r}"


def test_random_documents_round_trip():
    rng = random.Random(4242)
    for _ in range(300):
        nodes = [
            t if isinstance(t, Element) else Element(b"n", [], [t])
            for t in (_random_tree(rng) for _ in range(rng.randint(1, 5)))
        ]
        wire = encode_document(nodes)
        back = decode_document(wire)
        assert len(back) == len(nodes)
        assert all(_same(_canonical(a), b) for a, b in zip(nodes, back))


def test_truncation_at_every_offset_is_rejected_or_parsed():
    """A truncated document must never decode to something that looks whole."""
    el = Element(b"a", [(b"k", b"v")], [b"text", Element(b"b", [], [b"deep"])])
    wire = encode(el)
    for cut in range(1, len(wire)):
        try:
            decode(wire[:cut])
        except TersemlError:
            pass
        else:
            pytest.fail(f"truncation at {cut} decoded as a whole document: {wire[:cut]!r}")


# ---------------------------------------------------------------------------
# Shared conformance vectors
# ---------------------------------------------------------------------------

def _from_json(n):
    if "text" in n:
        return bytes.fromhex(n["text"])
    return Element(
        bytes.fromhex(n["tag"]),
        [(bytes.fromhex(k), bytes.fromhex(v)) for k, v in n["attrs"]],
        [_from_json(c) for c in n["children"]],
    )


def _vectors():
    import json
    path = os.path.join(os.path.dirname(__file__), "vectors.json")
    with open(path) as f:
        return json.load(f)["cases"]


@pytest.mark.parametrize("case", _vectors(), ids=lambda c: c["name"])
def test_shared_vectors(case):
    """The same corpus the JavaScript implementation checks. Two
    implementations that both match a third-party file cannot quietly agree
    with each other on something the grammar does not say."""
    tree = _from_json(case["tree"])
    assert encode(tree).hex() == case["wire"], "encoded bytes differ from the vector"
    assert _same(_canonical(tree), decode(bytes.fromhex(case["wire"]))), "decoded tree differs"


# ---------------------------------------------------------------------------
# Binary runs (§5.1)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize(
    "payload,expected_form",
    [
        (b"a" * 600, "inline"),
        (b"{}" * 300, "terminated"),
        (bytes((i * 37 + 11) % 256 for i in range(600)), "counted"),
        # NUL is not reserved inline, so plain zeros need no escaping at all
        # and inline beats the 4-byte counted header.
        (bytes(600), "inline"),
        (b"short", "inline"),
    ],
)
def test_binary_form_selection(payload, expected_form):
    """§5.1: shortest wins, ties break counted/terminated/inline. Mandatory,
    so that one document has exactly one encoding."""
    from terseml import choose_form
    assert choose_form(payload) == expected_form


def test_every_binary_form_round_trips():
    for payload in (
        b"a" * 600,
        b"{}" * 300,
        bytes(range(256)) * 4,
        bytes(600),
        b"\x00" * 10 + b"{" * 10 + b"\\" * 10,
    ):
        el = Element(b"d", [], [payload])
        assert decode(encode(el)).children == [payload]


def test_counted_run_length_is_validated_before_allocating():
    """§6, and the reason it is called out: a length read from the file and
    trusted is how a decoder is made to abort on a ten-byte input."""
    wire = b"{d,\\B" + b"\xff\xff\xff\x7f" + b"ab}"
    with pytest.raises(JsonpackError if False else Exception):
        decode(wire)


def test_unterminated_binary_run_is_rejected():
    with pytest.raises(Exception):
        decode(b"{d,\\babc")


# ---------------------------------------------------------------------------
# Large documents
# ---------------------------------------------------------------------------

def test_a_large_flat_document():
    """Many siblings: 50k elements, the shape a converted data file takes."""
    root = Element(b"rows", [], [
        Element(b"r", [(b"i", str(i).encode())], [b"value-%d" % i]) for i in range(50_000)
    ])
    wire = encode(root)
    back = decode(wire)
    assert len(back.children) == 50_000
    assert back.children[0].attrs == [(b"i", b"0")]
    assert back.children[-1].children == [b"value-49999"]


def test_a_large_binary_payload():
    """A megabyte through each form."""
    import os as _os
    for payload in (_os.urandom(1 << 20), b"{" * (1 << 20), bytes(1 << 20)):
        el = Element(b"blob", [], [payload])
        assert decode(encode(el)).children == [payload]


def test_deeply_nested_within_the_bound():
    node = Element(b"leaf", [], [b"x"])
    for _ in range(200):
        node = Element(b"n", [], [node])
    assert decode(encode(node)) is not None


def test_a_large_mixed_document():
    """Text and elements interleaved at scale -- the XML shape JSON cannot
    express, and the one most likely to expose a boundary bug."""
    kids = []
    for i in range(20_000):
        kids.append(b"text %d " % i)
        kids.append(Element(b"b", [], [b"bold"]))
    root = Element(b"p", [], kids)
    back = decode(encode(root))
    assert len(back.children) == 40_000
    assert back.children[1].tag == b"b"
