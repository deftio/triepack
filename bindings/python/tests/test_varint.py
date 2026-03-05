# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import pytest

from triepack.bitstream import BitReader, BitWriter
from triepack.varint import (
    read_var_int,
    read_var_uint,
    var_uint_bits,
    write_var_int,
    write_var_uint,
)


def _roundtrip_u(value):
    w = BitWriter()
    write_var_uint(w, value)
    r = BitReader(w.to_bytes())
    result = read_var_uint(r)
    assert result == value


def _roundtrip_s(value):
    w = BitWriter()
    write_var_int(w, value)
    r = BitReader(w.to_bytes())
    result = read_var_int(r)
    assert result == value


def test_unsigned_zero():
    _roundtrip_u(0)


def test_unsigned_one():
    _roundtrip_u(1)


def test_unsigned_127():
    _roundtrip_u(127)


def test_unsigned_128():
    _roundtrip_u(128)


def test_unsigned_16383():
    _roundtrip_u(16383)


def test_unsigned_16384():
    _roundtrip_u(16384)


def test_unsigned_large():
    _roundtrip_u(0x7FFFFFFFFFFFFFFF)


def test_signed_zero():
    _roundtrip_s(0)


def test_signed_positive():
    _roundtrip_s(1)
    _roundtrip_s(63)
    _roundtrip_s(64)


def test_signed_negative():
    _roundtrip_s(-1)
    _roundtrip_s(-64)
    _roundtrip_s(-65)


def test_signed_large():
    _roundtrip_s(2**62)
    _roundtrip_s(-(2**62))


def test_zigzag_mapping():
    w = BitWriter()
    write_var_int(w, 0)
    write_var_int(w, -1)
    write_var_int(w, 1)
    write_var_int(w, -2)

    r = BitReader(w.to_bytes())
    assert read_var_uint(r) == 0
    assert read_var_uint(r) == 1
    assert read_var_uint(r) == 2
    assert read_var_uint(r) == 3


def test_encoding_size():
    w = BitWriter()
    write_var_uint(w, 0)
    assert w.position == 8

    w = BitWriter()
    write_var_uint(w, 127)
    assert w.position == 8

    w = BitWriter()
    write_var_uint(w, 128)
    assert w.position == 16


def test_var_uint_bits_fn():
    assert var_uint_bits(0) == 8
    assert var_uint_bits(127) == 8
    assert var_uint_bits(128) == 16
    assert var_uint_bits(16383) == 16
    assert var_uint_bits(16384) == 24


def test_negative_uint_raises():
    w = BitWriter()
    with pytest.raises(ValueError):
        write_var_uint(w, -1)


def test_varint_read_overflow():
    w = BitWriter()
    for _ in range(11):
        w.write_u8(0x80)
    r = BitReader(w.to_bytes())
    with pytest.raises(OverflowError):
        read_var_uint(r)
