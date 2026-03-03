# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""LEB128 unsigned VarInt + zigzag signed VarInt — matches bitstream_varint.c."""

VARINT_MAX_GROUPS = 10


def write_var_uint(writer, value):
    """Write an unsigned LEB128 VarInt."""
    if value < 0:
        raise ValueError("write_var_uint: negative value")
    while True:
        byte = value & 0x7F
        value >>= 7
        if value > 0:
            byte |= 0x80
        writer.write_u8(byte)
        if value == 0:
            break


def read_var_uint(reader):
    """Read an unsigned LEB128 VarInt."""
    val = 0
    shift = 0
    for _ in range(VARINT_MAX_GROUPS):
        byte = reader.read_u8()
        val |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:
            return val
        shift += 7
    raise OverflowError("VarInt overflow")


def write_var_int(writer, value):
    """Write a signed zigzag VarInt."""
    if value >= 0:
        raw = value * 2
    else:
        raw = (-value) * 2 - 1
    write_var_uint(writer, raw)


def read_var_int(reader):
    """Read a signed zigzag VarInt."""
    raw = read_var_uint(reader)
    if raw & 1:
        return -(raw >> 1) - 1
    return raw >> 1


def var_uint_bits(val):
    """Return the number of bits needed to encode val as a VarInt."""
    bits = 0
    while True:
        bits += 8
        val >>= 7
        if val == 0:
            break
    return bits
