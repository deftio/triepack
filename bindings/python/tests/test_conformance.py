# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""Cross-language conformance suite.

Reads tests/conformance/cases.txt -- the corpus every implementation shares --
and checks this binding against the C-generated fixtures: decoding a fixture
must produce the corpus values, and encoding the corpus values must produce the
fixture byte for byte.

See tests/conformance/README.md.
"""

import math
import os
import struct

import pytest

import triepack

CONFORMANCE_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "tests", "conformance")
)
CASES_FILE = os.path.join(CONFORMANCE_DIR, "cases.txt")
FIXTURE_DIR = os.path.join(CONFORMANCE_DIR, "fixtures")
MALFORMED_DIR = os.path.join(CONFORMANCE_DIR, "malformed")


# -- Corpus parsing --------------------------------------------------------


def token_bytes(tok):
    """Decode a corpus token ("~" for empty, %XX escapes) into bytes."""
    if tok == "~":
        return b""
    out = bytearray()
    i = 0
    while i < len(tok):
        if tok[i] == "%":
            out.append(int(tok[i + 1 : i + 3], 16))
            i += 3
        else:
            out.append(ord(tok[i]))
            i += 1
    return bytes(out)


def token_str(tok):
    return token_bytes(tok).decode("utf-8")


def hex_bytes(tok):
    return b"" if tok == "~" else bytes.fromhex(tok)


class Case:
    def __init__(self, name, encode_expected, requires):
        self.name = name
        self.encode_expected = encode_expected
        self.requires = requires
        self.keys = []

    @property
    def fixture_path(self):
        return os.path.join(FIXTURE_DIR, self.name + ".trp")

    def fixture(self):
        with open(self.fixture_path, "rb") as f:
            return f.read()

    def data(self):
        return {k: case_value(t, arg) for k, t, arg in self.keys}


def case_value(type_name, arg):
    """Build the Python value a corpus entry describes."""
    if type_name == "null":
        return None
    if type_name == "bool":
        return arg == "1"
    if type_name in ("int", "uint"):
        return int(arg)
    if type_name == "f64":
        return struct.unpack(">d", bytes.fromhex(arg))[0]
    if type_name == "f32":
        return struct.unpack(">f", bytes.fromhex(arg))[0]
    if type_name == "str":
        return token_str(arg)
    if type_name == "blob":
        return hex_bytes(arg)
    raise AssertionError("unknown corpus type: " + type_name)


def parse_cases():
    cases = []
    cur = None
    with open(CASES_FILE, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if parts[0] == "case":
                cur = Case(
                    parts[1],
                    "encode=no" not in parts,
                    "int64" if "requires=int64" in parts else None,
                )
                cases.append(cur)
            elif parts[0] == "key":
                cur.keys.append(
                    (token_str(parts[1]), parts[2], parts[3] if len(parts) > 3 else None)
                )
            else:
                raise AssertionError("unknown corpus directive: " + parts[0])
    return cases


CASES = parse_cases()
IDS = [c.name for c in CASES]


def values_match(got, want):
    """Doubles compare by bit pattern, so -0.0 and NaN are handled."""
    if want is None:
        return got is None
    if isinstance(want, bool):
        return isinstance(got, bool) and got == want
    if isinstance(want, float):
        if not isinstance(got, float):
            return False
        if math.isnan(want) and math.isnan(got):
            return True
        return struct.pack(">d", got) == struct.pack(">d", want)
    return type(got) is type(want) and got == want


# -- The suite -------------------------------------------------------------


def test_corpus_and_fixtures_present():
    assert CASES
    for c in CASES:
        assert os.path.exists(c.fixture_path), c.name


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_decodes_c_fixture(case):
    result = triepack.decode(case.fixture())
    want = case.data()
    assert sorted(result.keys()) == sorted(want.keys())
    for k, v in want.items():
        assert values_match(result[k], v), "key %r: %r != %r" % (k, result[k], v)


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_encodes_like_c_reference(case):
    # Cases marked encode=no describe values this binding cannot reproduce
    # exactly (float32, or doubles some languages read back as integers).
    if not case.encode_expected:
        pytest.skip("decode-only case")
    assert triepack.encode(case.data()) == case.fixture()


# -- Malformed inputs ------------------------------------------------------

MALFORMED = sorted(f for f in os.listdir(MALFORMED_DIR) if f.endswith(".trp"))


def test_malformed_corpus_present():
    assert MALFORMED


@pytest.mark.parametrize("name", MALFORMED)
def test_malformed_input_rejected(name):
    """Each file is a valid fixture with one field damaged.

    Damage inside the data is re-sealed with a correct CRC, so the reader has
    to catch it rather than being handed a checksum failure.
    """
    with open(os.path.join(MALFORMED_DIR, name), "rb") as f:
        buf = f.read()
    with pytest.raises(Exception):
        triepack.decode(buf)
