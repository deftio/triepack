"""XML <-> terseml bridge tests.

The claim being checked is infoset fidelity: an XML document converted to
terseml and back must be the same document. Comments and processing
instructions are included because the README claims the infoset, and before
the reserved tags existed they were silently dropped.

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

import os
import subprocess
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))

from terseml import COMMENT_TAG, PI_TAG, decode, encode  # noqa: E402
from xml_bridge import from_xml, to_xml  # noqa: E402


@pytest.mark.parametrize(
    "xml",
    [
        b"<r>text</r>",
        b'<r a="1" b="2">text</r>',
        b"<p>Hello <b>world</b>!</p>",          # mixed content
        b"<l><i>a</i><i>b</i></l>",             # repeated siblings
        b"<e/>",                                 # empty element
        b"<r><!-- note --></r>",                # comment
        b"<r><?pi data?></r>",                  # processing instruction
        b"<r>  <a/>  </r>",                     # significant whitespace
        b'<r a="&lt;&amp;&gt;">&lt;text&gt;</r>',  # entities
        b"<r><a><b><c>deep</c></b></a></r>",
    ],
)
def test_xml_round_trips(xml):
    assert to_xml(from_xml(xml)) == xml


def test_comments_and_pis_survive():
    """They are dropped by a default ElementTree parse, so this is the test
    that the reserved tags actually do something."""
    tree = from_xml(b"<r><!-- c --><?p d?></r>")
    tags = [c.tag for c in tree.children]
    assert COMMENT_TAG in tags and PI_TAG in tags


def test_the_wire_form_round_trips_too():
    """XML -> terseml -> bytes -> terseml -> XML."""
    xml = b'<r a="1"><!-- n --><p>Hi <b>there</b></p><e/></r>'
    assert to_xml(decode(encode(from_xml(xml)))) == xml


def test_attribute_quotes_are_escaped():
    xml = b'<r a="say &quot;hi&quot;">x</r>'
    assert to_xml(from_xml(xml)) == xml


@pytest.mark.parametrize("mode", ["encode", "decode"])
def test_cli(mode, tmp_path):
    xml = b'<r a="1"><p>Hello <b>world</b>!</p></r>'
    src = tmp_path / "doc.xml"
    src.write_bytes(xml)
    here = os.path.dirname(os.path.abspath(__file__))
    packed = subprocess.run(
        [sys.executable, os.path.join(here, "xml_bridge.py"), "encode", str(src)],
        capture_output=True, check=True,
    ).stdout
    if mode == "encode":
        assert packed.startswith(b"{r,[a:1]")
        return
    tsml = tmp_path / "doc.tsml"
    tsml.write_bytes(packed)
    back = subprocess.run(
        [sys.executable, os.path.join(here, "xml_bridge.py"), "decode", str(tsml)],
        capture_output=True, check=True,
    ).stdout
    assert back == xml


def test_cli_reads_stdin():
    here = os.path.dirname(os.path.abspath(__file__))
    out = subprocess.run(
        [sys.executable, os.path.join(here, "xml_bridge.py"), "encode"],
        input=b"<r>x</r>", capture_output=True, check=True,
    ).stdout
    assert out == b"{r,x}"


def test_cli_rejects_a_bad_mode():
    here = os.path.dirname(os.path.abspath(__file__))
    r = subprocess.run([sys.executable, os.path.join(here, "xml_bridge.py"), "wat"],
                       capture_output=True)
    assert r.returncode == 2
