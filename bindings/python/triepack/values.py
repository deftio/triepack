# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Value encoder/decoder for 8 type tags — matches src/core/value.c."""

import struct

from .varint import read_var_int, read_var_uint, write_var_int, write_var_uint

# Type tags (4-bit, matches tp_value_type enum)
TP_NULL = 0
TP_BOOL = 1
TP_INT = 2
TP_UINT = 3
TP_FLOAT32 = 4
TP_FLOAT64 = 5
TP_STRING = 6
TP_BLOB = 7


def encode_value(writer, val):
    """Encode a Python value into the bitstream."""
    if val is None:
        writer.write_bits(TP_NULL, 4)
        return

    if isinstance(val, bool):
        writer.write_bits(TP_BOOL, 4)
        writer.write_bit(1 if val else 0)
        return

    if isinstance(val, int):
        if val < 0:
            writer.write_bits(TP_INT, 4)
            write_var_int(writer, val)
        else:
            writer.write_bits(TP_UINT, 4)
            write_var_uint(writer, val)
        return

    if isinstance(val, float):
        writer.write_bits(TP_FLOAT64, 4)
        packed = struct.pack(">d", val)
        hi = struct.unpack(">I", packed[:4])[0]
        lo = struct.unpack(">I", packed[4:])[0]
        writer.write_u64(hi, lo)
        return

    if isinstance(val, str):
        encoded = val.encode("utf-8")
        writer.write_bits(TP_STRING, 4)
        write_var_uint(writer, len(encoded))
        writer.align_to_byte()
        writer.write_bytes(encoded)
        return

    if isinstance(val, (bytes, bytearray)):
        writer.write_bits(TP_BLOB, 4)
        write_var_uint(writer, len(val))
        writer.align_to_byte()
        writer.write_bytes(val)
        return

    raise TypeError(f"Unsupported value type: {type(val)}")


def decode_value(reader):
    """Decode a value from the bitstream."""
    tag = reader.read_bits(4)

    if tag == TP_NULL:
        return None
    if tag == TP_BOOL:
        return reader.read_bit() != 0
    if tag == TP_INT:
        return read_var_int(reader)
    if tag == TP_UINT:
        return read_var_uint(reader)
    if tag == TP_FLOAT32:
        bits = reader.read_u32()
        packed = struct.pack(">I", bits)
        return struct.unpack(">f", packed)[0]
    if tag == TP_FLOAT64:
        hi = reader.read_u32()
        lo = reader.read_u32()
        packed = struct.pack(">II", hi, lo)
        return struct.unpack(">d", packed)[0]
    if tag == TP_STRING:
        slen = read_var_uint(reader)
        reader.align_to_byte()
        raw = reader.read_bytes(slen)
        return raw.decode("utf-8")
    if tag == TP_BLOB:
        blen = read_var_uint(reader)
        reader.align_to_byte()
        return reader.read_bytes(blen)

    raise ValueError(f"Unknown value type tag: {tag}")
