# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

from triepack.crc32 import crc32


def test_known_answer_123456789():
    """Standard CRC-32 known-answer test."""
    data = b"123456789"
    assert crc32(data) == 0xCBF43926


def test_empty():
    assert crc32(b"") == 0x00000000


def test_single_byte():
    result = crc32(b"\x00")
    assert isinstance(result, int)
    assert 0 <= result <= 0xFFFFFFFF


def test_all_zeros():
    result = crc32(b"\x00" * 256)
    assert isinstance(result, int)


def test_all_ff():
    result = crc32(b"\xff" * 4)
    assert isinstance(result, int)
