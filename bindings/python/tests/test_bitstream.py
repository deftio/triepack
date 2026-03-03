# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import pytest

from triepack.bitstream import BitReader, BitWriter


def test_write_read_bits():
    w = BitWriter()
    w.write_bits(0x07, 3)
    w.write_bits(0xAB, 8)
    w.write_bits(0x05, 5)

    r = BitReader(w.to_bytes())
    assert r.read_bits(3) == 0x07
    assert r.read_bits(8) == 0xAB
    assert r.read_bits(5) == 0x05


def test_write_read_single_bit():
    w = BitWriter()
    w.write_bit(1)
    w.write_bit(0)
    w.write_bit(1)

    r = BitReader(w.to_bytes())
    assert r.read_bit() == 1
    assert r.read_bit() == 0
    assert r.read_bit() == 1


def test_u8_roundtrip():
    w = BitWriter()
    w.write_u8(0x00)
    w.write_u8(0xFF)
    w.write_u8(0x42)

    r = BitReader(w.to_bytes())
    assert r.read_u8() == 0x00
    assert r.read_u8() == 0xFF
    assert r.read_u8() == 0x42


def test_u16_roundtrip():
    w = BitWriter()
    w.write_u16(0xABCD)

    r = BitReader(w.to_bytes())
    assert r.read_u16() == 0xABCD


def test_u32_roundtrip():
    w = BitWriter()
    w.write_u32(0xDEADBEEF)

    r = BitReader(w.to_bytes())
    assert r.read_u32() == 0xDEADBEEF


def test_align_to_byte():
    w = BitWriter()
    w.write_bits(0x07, 3)
    w.align_to_byte()
    assert w.position == 8

    w.write_u8(0x42)
    r = BitReader(w.to_bytes())
    r.read_bits(3)
    r.align_to_byte()
    assert r.read_u8() == 0x42


def test_peek_bits():
    w = BitWriter()
    w.write_bits(0xAB, 8)

    r = BitReader(w.to_bytes())
    assert r.peek_bits(8) == 0xAB
    assert r.position == 0  # didn't advance
    assert r.read_bits(8) == 0xAB
    assert r.position == 8


def test_read_bytes():
    w = BitWriter()
    w.write_bytes(b"\xDE\xAD\xBE\xEF")

    r = BitReader(w.to_bytes())
    assert r.read_bytes(4) == b"\xDE\xAD\xBE\xEF"


def test_eof_raises():
    r = BitReader(b"\x00")
    r.read_bits(8)
    with pytest.raises(EOFError):
        r.read_bits(1)


def test_remaining():
    r = BitReader(b"\x00\x00")
    assert r.remaining == 16
    r.read_bits(4)
    assert r.remaining == 12


def test_seek():
    w = BitWriter()
    w.write_u8(0xAA)
    w.write_u8(0xBB)

    r = BitReader(w.to_bytes())
    r.seek(8)
    assert r.read_u8() == 0xBB


def test_is_aligned():
    r = BitReader(b"\xFF")
    assert r.is_aligned()
    r.read_bit()
    assert not r.is_aligned()
