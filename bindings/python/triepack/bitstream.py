# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Arbitrary-width bit I/O — MSB-first, matching src/bitstream/."""


class BitWriter:
    """Write arbitrary-width bit fields into a growable buffer."""

    def __init__(self, initial_cap=256):
        self._buf = bytearray(initial_cap or 256)
        self._pos = 0  # bit position

    @property
    def position(self):
        return self._pos

    def _ensure(self, n_bits):
        needed = (self._pos + n_bits + 7) >> 3
        if needed <= len(self._buf):
            return
        new_cap = len(self._buf) * 2
        while new_cap < needed:
            new_cap *= 2
        self._buf.extend(b"\x00" * (new_cap - len(self._buf)))

    def write_bits(self, value, n):
        if n == 0 or n > 64:
            raise ValueError("write_bits: n must be 1..64")
        self._ensure(n)
        for i in range(n):
            bit = (value >> (n - 1 - i)) & 1
            byte_idx = self._pos >> 3
            bit_idx = 7 - (self._pos & 7)
            if bit:
                self._buf[byte_idx] |= 1 << bit_idx
            else:
                self._buf[byte_idx] &= ~(1 << bit_idx)
            self._pos += 1

    def write_bit(self, val):
        self._ensure(1)
        byte_idx = self._pos >> 3
        bit_idx = 7 - (self._pos & 7)
        if val:
            self._buf[byte_idx] |= 1 << bit_idx
        else:
            self._buf[byte_idx] &= ~(1 << bit_idx)
        self._pos += 1

    def write_u8(self, val):
        self.write_bits(val & 0xFF, 8)

    def write_u16(self, val):
        self.write_bits(val & 0xFFFF, 16)

    def write_u32(self, val):
        self.write_bits(val & 0xFFFFFFFF, 32)

    def write_u64(self, hi32, lo32):
        self.write_bits((hi32 >> 0) & 0xFFFFFFFF, 32)
        self.write_bits((lo32 >> 0) & 0xFFFFFFFF, 32)

    def write_bytes(self, data):
        for b in data:
            self.write_u8(b)

    def align_to_byte(self):
        rem = self._pos & 7
        if rem != 0:
            pad = 8 - rem
            self._ensure(pad)
            self._pos += pad

    def to_bytes(self):
        length = (self._pos + 7) >> 3
        return bytes(self._buf[:length])

    def get_buffer(self):
        return self._buf


class BitReader:
    """Read arbitrary-width bit fields from a buffer."""

    def __init__(self, buf):
        if isinstance(buf, (bytes, bytearray)):
            self._buf = buf
        else:
            self._buf = bytes(buf)
        self._bit_len = len(self._buf) * 8
        self._pos = 0

    @property
    def position(self):
        return self._pos

    @property
    def remaining(self):
        return self._bit_len - self._pos

    def seek(self, bit_pos):
        self._pos = bit_pos

    def advance(self, n_bits):
        self._pos += n_bits

    def read_bits(self, n):
        if n == 0 or n > 64:
            raise ValueError("read_bits: n must be 1..64")
        if self._pos + n > self._bit_len:
            raise EOFError("EOF")
        val = 0
        for i in range(n):
            byte_idx = self._pos >> 3
            bit_idx = 7 - (self._pos & 7)
            val = (val << 1) | ((self._buf[byte_idx] >> bit_idx) & 1)
            self._pos += 1
        return val

    def read_bit(self):
        if self._pos >= self._bit_len:
            raise EOFError("EOF")
        byte_idx = self._pos >> 3
        bit_idx = 7 - (self._pos & 7)
        bit = (self._buf[byte_idx] >> bit_idx) & 1
        self._pos += 1
        return bit

    def read_u8(self):
        return self.read_bits(8)

    def read_u16(self):
        return self.read_bits(16)

    def read_u32(self):
        return self.read_bits(32)

    def read_u64(self):
        hi = self.read_u32()
        lo = self.read_u32()
        return hi * 0x100000000 + lo

    def read_bytes(self, n):
        out = bytearray(n)
        for i in range(n):
            out[i] = self.read_u8()
        return bytes(out)

    def peek_bits(self, n):
        saved = self._pos
        val = self.read_bits(n)
        self._pos = saved
        return val

    def align_to_byte(self):
        rem = self._pos & 7
        if rem != 0:
            self._pos += 8 - rem

    def is_aligned(self):
        return (self._pos & 7) == 0
