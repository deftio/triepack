#!/usr/bin/env python3
"""XML <-> terseml, and a command line for it.

Comments and processing instructions map to the reserved tags `#comment` and
`#pi` (see GRAMMAR.md §9). They are a convention on top of the one node type
the grammar defines, not an extension to it -- a profile that does not use
them never emits them.

    python3 xml_bridge.py encode doc.xml  > doc.tsml
    python3 xml_bridge.py decode doc.tsml > doc.xml
    cat doc.xml | python3 xml_bridge.py encode > doc.tsml

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

import sys
import xml.etree.ElementTree as ET

from terseml import COMMENT_TAG, PI_TAG, Element, decode, encode

_XML_ESCAPES = ((b"&", b"&amp;"), (b"<", b"&lt;"), (b">", b"&gt;"))


def from_xml(data: bytes) -> Element:
    """Parse XML into a terseml tree, keeping comments and PIs."""
    parser = ET.XMLParser(
        target=ET.TreeBuilder(insert_comments=True, insert_pis=True)
    )
    parser.feed(data)
    return _convert(parser.close())


def _convert(el) -> Element:
    if el.tag is ET.Comment:
        return Element(COMMENT_TAG, [], [(el.text or "").encode()])
    if el.tag is ET.ProcessingInstruction:
        return Element(PI_TAG, [], [(el.text or "").encode()])

    kids = []
    if el.text:
        kids.append(el.text.encode())
    for child in el:
        kids.append(_convert(child))
        if child.tail:
            kids.append(child.tail.encode())
    attrs = [(k.encode(), v.encode()) for k, v in el.attrib.items()]
    return Element(el.tag.encode(), attrs, kids)


def to_xml(node) -> bytes:
    """Serialise a terseml tree back to XML."""
    if not isinstance(node, Element):
        return _xml_escape(bytes(node))

    if node.tag == COMMENT_TAG:
        return b"<!--" + b"".join(bytes(c) for c in node.children) + b"-->"
    if node.tag == PI_TAG:
        return b"<?" + b"".join(bytes(c) for c in node.children) + b"?>"

    attrs = b"".join(
        b' ' + k + b'="' + _xml_escape(v).replace(b'"', b"&quot;") + b'"'
        for k, v in node.attrs
    )
    if not node.children:
        return b"<" + node.tag + attrs + b"/>"
    inner = b"".join(to_xml(c) for c in node.children)
    return b"<" + node.tag + attrs + b">" + inner + b"</" + node.tag + b">"


def _xml_escape(raw: bytes) -> bytes:
    for needle, repl in _XML_ESCAPES:
        if needle in raw:
            raw = raw.replace(needle, repl)
    return raw


def main(argv):
    if len(argv) < 2 or argv[1] not in ("encode", "decode"):
        sys.stderr.write(__doc__)
        return 2
    mode = argv[1]
    data = open(argv[2], "rb").read() if len(argv) > 2 else sys.stdin.buffer.read()
    out = encode(from_xml(data)) if mode == "encode" else to_xml(decode(data))
    sys.stdout.buffer.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
