# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import math

import pytest

import triepack


def test_version():
    assert triepack.__version__ == "0.1.0"


def test_empty_object():
    buf = triepack.encode({})
    assert isinstance(buf, bytes)
    assert len(buf) >= 36  # 32-byte header + 4-byte CRC
    result = triepack.decode(buf)
    assert result == {}


def test_single_key_null():
    data = {"hello": None}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_integer():
    data = {"key": 42}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_negative():
    data = {"neg": -100}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_bool_true():
    data = {"flag": True}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_bool_false():
    data = {"flag": False}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_float64():
    data = {"pi": 3.14159}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert math.isclose(result["pi"], 3.14159, rel_tol=1e-10)


def test_single_key_string():
    data = {"greeting": "hello world"}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_blob():
    blob = b"\xDE\xAD\xBE\xEF"
    data = {"binary": blob}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result["binary"] == blob


def test_multiple_keys_mixed():
    data = {
        "bool": True,
        "f64": 3.14159,
        "int": -100,
        "str": "hello",
        "uint": 200,
    }
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result["bool"] is True
    assert math.isclose(result["f64"], 3.14159, rel_tol=1e-10)
    assert result["int"] == -100
    assert result["str"] == "hello"
    assert result["uint"] == 200


def test_shared_prefix():
    data = {"abc": 10, "abd": 20, "xyz": 30}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_keys_only_null():
    data = {"apple": None, "banana": None, "cherry": None}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_large_dictionary():
    data = {}
    for i in range(100):
        data[f"key_{i:04d}"] = i
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert len(result) == 100
    for i in range(100):
        assert result[f"key_{i:04d}"] == i


def test_magic_bytes():
    buf = triepack.encode({"test": 1})
    assert buf[0] == 0x54  # T
    assert buf[1] == 0x52  # R
    assert buf[2] == 0x50  # P
    assert buf[3] == 0x00


def test_version_header():
    buf = triepack.encode({"test": 1})
    assert buf[4] == 1  # major
    assert buf[5] == 0  # minor


def test_crc_corruption():
    buf = bytearray(triepack.encode({"test": 1}))
    buf[10] ^= 0x01
    with pytest.raises(ValueError, match="CRC"):
        triepack.decode(bytes(buf))


def test_invalid_magic():
    buf = bytearray(40)
    buf[0] = 0xFF
    with pytest.raises(ValueError, match="magic"):
        triepack.decode(bytes(buf))


def test_encode_rejects_non_dict():
    with pytest.raises(TypeError):
        triepack.encode("string")
    with pytest.raises(TypeError):
        triepack.encode(42)
    with pytest.raises(TypeError):
        triepack.encode(None)


def test_decode_rejects_short():
    with pytest.raises(ValueError):
        triepack.decode(b"\x00" * 10)


def test_utf8_keys():
    data = {"café": 1, "naïve": 2, "über": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_char_keys():
    data = {"a": 1, "b": 2, "c": 3, "d": 4, "e": 5}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_empty_string_key():
    data = {"": 99}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result[""] == 99


def test_long_shared_prefix():
    data = {"aaaaaa1": 1, "aaaaaa2": 2, "aaaaaa3": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data
