#!/usr/bin/env python3
"""Generate vectors.json -- the shared conformance corpus.

Every implementation encodes each tree and must produce the listed bytes, then
decodes those bytes and must recover the tree. Two implementations that agree
with a third-party file cannot quietly agree with each other on something the
grammar does not say.

Trees and bytes are hex-encoded so the file stays plain ASCII and diffs
readably.

Run:  python3 make_vectors.py > vectors.json

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xjarchive import Element, encode  # noqa: E402


def T(tag, attrs=(), kids=()):
    return Element(tag, list(attrs), list(kids))


CASES = [
    # name, tree
    ("text-plain", T(b"p", [], [b"hello"])),
    ("text-commas", T(b"p", [], [b"a,b,c"])),
    ("text-brackets", T(b"p", [], [b"see [[link]]"])),
    ("text-leading-bracket", T(b"p", [], [b"[start]"])),
    ("text-braces", T(b"p", [], [b"use {{t}}"])),
    ("text-backslash", T(b"p", [], [b"back\\slash"])),
    ("empty-content", T(b"p", [], [])),
    ("singleton", T(b"#text", [], [b"bare"])),
    ("attrs-one", T(b"a", [(b"href", b"/x")], [b"click"])),
    ("attrs-many", T(b"a", [(b"k", b"v"), (b"j", b"w")], [b"t"])),
    ("attrs-reserved", T(b"a", [(b"k:1", b"v,2")], [b"t"])),
    ("attrs-no-content", T(b"img", [(b"src", b"/i.png")], [])),
    ("nested", T(b"div", [], [T(b"b", [], [b"bold"])])),
    ("mixed-content", T(b"p", [], [b"Hello ", T(b"b", [], [b"world"]), b"!"])),
    ("repeated-siblings", T(b"l", [], [T(b"i", [], [b"a"]), T(b"i", [], [b"b"])])),
    ("deep", T(b"a", [], [T(b"b", [], [T(b"c", [], [T(b"d", [], [b"x"])])])])),
    ("tag-reserved", T(b"t,g", [], [b"x"])),
    ("tag-brace", T(b"t{g", [], [b"x"])),
    ("bytes-all-256", T(b"bin", [], [bytes(range(256))])),
    ("bytes-nul", T(b"bin", [], [b"a\x00b"])),
    ("bytes-high", T(b"bin", [], [b"\xff\xfe\xfd"])),
    # binary run selection -- one per form, per GRAMMAR §5.1
    ("form-inline-ascii", T(b"d", [], [b"a" * 600])),
    ("form-counted-random", T(b"d", [], [bytes((i * 37 + 11) % 256 for i in range(600))])),
    ("form-terminated-braces", T(b"d", [], [b"{}" * 300])),
    ("form-counted-nuls", T(b"d", [], [bytes(600)])),
    ("form-boundary-short", T(b"d", [], [bytes((i * 37) % 256 for i in range(16))])),
]


def node_json(n):
    if isinstance(n, Element):
        return {
            "tag": n.tag.hex(),
            "attrs": [[k.hex(), v.hex()] for k, v in n.attrs],
            "children": [node_json(c) for c in n.children],
        }
    return {"text": bytes(n).hex()}


def main():
    out = {
        "note": "Shared conformance vectors for xjarchive. See GRAMMAR.md.",
        "cases": [
            {"name": name, "tree": node_json(tree), "wire": encode(tree).hex()}
            for name, tree in CASES
        ],
    }
    json.dump(out, sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
