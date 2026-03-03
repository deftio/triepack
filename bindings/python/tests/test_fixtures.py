# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Cross-language fixture tests — mirrors JavaScript fixtures.test.js."""

import math
import os

import triepack

FIXTURE_DIR = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "tests", "fixtures"
)


def read_fixture(name):
    path = os.path.join(FIXTURE_DIR, name)
    with open(path, "rb") as f:
        return f.read()


# ── Decode C-generated fixtures ─────────────────────────────────────


def test_decode_empty():
    buf = read_fixture("empty.trp")
    result = triepack.decode(buf)
    assert result == {}


def test_decode_single_null():
    buf = read_fixture("single_null.trp")
    result = triepack.decode(buf)
    assert result == {"hello": None}


def test_decode_single_int():
    buf = read_fixture("single_int.trp")
    result = triepack.decode(buf)
    assert result == {"key": 42}


def test_decode_multi_mixed():
    buf = read_fixture("multi_mixed.trp")
    result = triepack.decode(buf)
    assert result["bool"] is True
    assert math.isclose(result["f64"], 3.14159, rel_tol=1e-10)
    assert result["int"] == -100
    assert result["str"] == "hello"
    assert result["uint"] == 200


def test_decode_shared_prefix():
    buf = read_fixture("shared_prefix.trp")
    result = triepack.decode(buf)
    assert result == {"abc": 10, "abd": 20, "xyz": 30}


def test_decode_large():
    buf = read_fixture("large.trp")
    result = triepack.decode(buf)
    assert len(result) == 100
    for i in range(100):
        key = f"key_{i:04d}"
        assert result[key] == i


def test_decode_keys_only():
    buf = read_fixture("keys_only.trp")
    result = triepack.decode(buf)
    assert result == {"apple": None, "banana": None, "cherry": None}


# ── Byte-for-byte binary reproducibility ────────────────────────────


def test_encode_empty_matches_fixture():
    py_buf = triepack.encode({})
    c_buf = read_fixture("empty.trp")
    assert py_buf == c_buf


def test_encode_single_null_matches_fixture():
    py_buf = triepack.encode({"hello": None})
    c_buf = read_fixture("single_null.trp")
    assert py_buf == c_buf


def test_encode_single_int_matches_fixture():
    py_buf = triepack.encode({"key": 42})
    c_buf = read_fixture("single_int.trp")
    assert py_buf == c_buf


def test_encode_multi_mixed_matches_fixture():
    py_buf = triepack.encode({
        "bool": True,
        "f64": 3.14159,
        "int": -100,
        "str": "hello",
        "uint": 200,
    })
    c_buf = read_fixture("multi_mixed.trp")
    assert py_buf == c_buf


def test_encode_shared_prefix_matches_fixture():
    py_buf = triepack.encode({"abc": 10, "abd": 20, "xyz": 30})
    c_buf = read_fixture("shared_prefix.trp")
    assert py_buf == c_buf


def test_encode_large_matches_fixture():
    data = {}
    for i in range(100):
        data[f"key_{i:04d}"] = i
    py_buf = triepack.encode(data)
    c_buf = read_fixture("large.trp")
    assert py_buf == c_buf


def test_encode_keys_only_matches_fixture():
    py_buf = triepack.encode({"apple": None, "banana": None, "cherry": None})
    c_buf = read_fixture("keys_only.trp")
    assert py_buf == c_buf
