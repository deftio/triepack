# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import struct

import pytest

from triepack.bitstream import BitReader, BitWriter
from triepack.values import decode_value, encode_value


def test_encode_null():
    w = BitWriter()
    encode_value(w, None)
    r = BitReader(w.to_bytes())
    assert r.read_bits(4) == 0  # TP_NULL


def test_encode_bool():
    w = BitWriter()
    encode_value(w, True)
    encode_value(w, False)
    r = BitReader(w.to_bytes())
    assert r.read_bits(4) == 1  # TP_BOOL
    assert r.read_bit() == 1
    assert r.read_bits(4) == 1  # TP_BOOL
    assert r.read_bit() == 0


def test_decode_null():
    w = BitWriter()
    w.write_bits(0, 4)  # TP_NULL
    r = BitReader(w.to_bytes())
    assert decode_value(r) is None


def test_decode_float32():
    w = BitWriter()
    w.write_bits(4, 4)  # TP_FLOAT32
    packed = struct.pack(">f", 3.140000104904175)
    bits = struct.unpack(">I", packed)[0]
    w.write_u32(bits)
    r = BitReader(w.to_bytes())
    val = decode_value(r)
    assert abs(val - 3.14) < 0.01


def test_encode_unsupported_type():
    w = BitWriter()
    with pytest.raises(TypeError):
        encode_value(w, [1, 2, 3])


def test_decode_unknown_tag():
    w = BitWriter()
    w.write_bits(15, 4)  # invalid tag
    r = BitReader(w.to_bytes())
    with pytest.raises(ValueError, match="Unknown"):
        decode_value(r)
