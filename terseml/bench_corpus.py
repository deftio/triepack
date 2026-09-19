#!/usr/bin/env python3
"""Produce the size table quoted in README.md, GRAMMAR.md and the docs.

Every number in those documents about the Wikipedia corpus comes from here,
so a claim can be rechecked rather than believed.

    python3 bench_corpus.py path/to/enwik9        # the XML corpus
    python3 bench_corpus.py --json some.json      # a JSON corpus

enwik9 is the first 10^9 bytes of a Wikipedia XML dump
(https://mattmahoney.net/dc/textdata.html). Any prefix works; the script
takes 16 MB of it and trims to whole <page> elements so the input is
well-formed.

Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
BSD-2-Clause -- see LICENSE.txt
"""

import gzip
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import terseml
import xml_bridge

PREFIX = 16 * 1000 * 1000


def to_jsonml(node):
    """JsonML: ["tag", {attrs}, child, ...] -- the same tree in valid JSON."""
    if isinstance(node, (bytes, bytearray)):
        return bytes(node).decode("utf-8", "replace")
    out = [node.tag.decode("utf-8", "replace")]
    if node.attrs:
        out.append({k.decode("utf-8", "replace"): v.decode("utf-8", "replace")
                    for k, v in node.attrs})
    out.extend(to_jsonml(c) for c in node.children)
    return out


def json_to_tree(value, tag=b"v"):
    """JSON -> a terseml tree. Object keys become tags, which is the point:
    a key appears once as a tag instead of once quoted with a colon."""
    if isinstance(value, dict):
        return terseml.Element(
            tag, [], [json_to_tree(v, k.encode()) for k, v in value.items()])
    if isinstance(value, list):
        return terseml.Element(tag, [], [json_to_tree(v, b"i") for v in value])
    if value is None:
        return terseml.Element(tag, [], [])
    return terseml.Element(tag, [], [str(value).encode()])


def field_name_bytes(value):
    """What the document spends on naming its fields: each key plus its two
    quotes and colon."""
    if isinstance(value, dict):
        return (sum(len(k) + 3 for k in value)
                + sum(field_name_bytes(v) for v in value.values()))
    if isinstance(value, list):
        return sum(field_name_bytes(v) for v in value)
    return 0


def run_json(path):
    raw = open(path, "rb").read()
    doc = json.loads(raw)
    wire = terseml.encode(json_to_tree(doc, b"root"))
    if terseml.encode(terseml.decode(wire)) != wire:
        sys.exit("round trip is not canonical -- refusing to report a number")
    names = field_name_bytes(doc)
    n = len(raw)
    print(f"corpus: {path}, {n:,} bytes\n")
    print("| | bytes | of raw |")
    print("|---|---:|---:|")
    print(f"| JSON | {n:,} | 1.000 |")
    print(f"| field names in it | {names:,} | {names / n:.3f} |")
    print(f"| terseml | {len(wire):,} | {len(wire) / n:.3f} |")
    print(f"| gzip(JSON) | {len(gzip.compress(raw, 6)):,} | "
          f"{len(gzip.compress(raw, 6)) / n:.3f} |")
    print(f"| gzip(terseml) | {len(gzip.compress(wire, 6)):,} | "
          f"{len(gzip.compress(wire, 6)) / n:.3f} |")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "--json":
        return run_json(sys.argv[2])
    with open(sys.argv[1], "rb") as f:
        raw = f.read(PREFIX)

    end = raw.rfind(b"</page>")
    start = raw.index(b"<page>")
    if end < 0 or start < 0:
        sys.exit("no complete <page> element in the prefix")
    doc = b"<pages>" + raw[start:end + 7] + b"</pages>"

    tree = xml_bridge.from_xml(doc)
    t0 = time.time()
    tsml = terseml.encode(tree)
    enc_s = time.time() - t0
    t0 = time.time()
    terseml.decode(tsml)
    dec_s = time.time() - t0

    jm = json.dumps(to_jsonml(tree), separators=(",", ":"),
                    ensure_ascii=False).encode("utf-8")

    n = len(doc)
    mb = n / (1024 * 1024)
    print(f"corpus: {n:,} bytes, {doc.count(b'<page>'):,} pages "
          f"(whole <page> elements from a {PREFIX // 1000000} MB prefix)\n")
    print(f"| format | bytes | of XML |")
    print(f"|---|---:|---:|")
    for name, blob in (("XML", doc), ("JsonML", jm), ("terseml", tsml)):
        print(f"| {name} | {len(blob):,} | {len(blob) / n:.3f} |")
    print()
    for name, blob in (("gzip(XML)", doc), ("gzip(JsonML)", jm),
                       ("gzip(terseml)", tsml)):
        g = gzip.compress(blob, 6)
        print(f"| {name} | {len(g):,} | {len(g) / n:.3f} |")
    print(f"\npure-Python encode {mb / enc_s:.1f} MB/s, decode {mb / dec_s:.1f} MB/s")


if __name__ == "__main__":
    main()
