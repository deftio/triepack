// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Arbitrary-width bit I/O -- MSB-first, matching src/bitstream/.

use crate::TriePackError;

// ---------------------------------------------------------------------------
// BitWriter
// ---------------------------------------------------------------------------

/// Growable buffer with bit-level write operations, MSB-first.
pub struct BitWriter {
    buf: Vec<u8>,
    pos: usize, // current bit position
}

impl BitWriter {
    /// Create a new writer with the given initial byte capacity.
    pub fn new(initial_cap: usize) -> Self {
        let cap = if initial_cap == 0 { 256 } else { initial_cap };
        Self {
            buf: vec![0u8; cap],
            pos: 0,
        }
    }

    /// Current write position in bits.
    pub fn position(&self) -> usize {
        self.pos
    }

    /// Ensure the buffer can hold at least `n_bits` more bits from the current
    /// position.
    fn ensure(&mut self, n_bits: usize) {
        let needed = (self.pos + n_bits + 7) >> 3;
        if needed > self.buf.len() {
            let mut new_cap = self.buf.len() * 2;
            while new_cap < needed {
                new_cap *= 2;
            }
            self.buf.resize(new_cap, 0);
        }
    }

    /// Write the low `n` bits (1..64) of `value`, MSB-first.
    pub fn write_bits(&mut self, value: u64, n: usize) {
        assert!(n >= 1 && n <= 64, "write_bits: n must be 1..64");
        self.ensure(n);
        for i in 0..n {
            let bit = ((value >> (n - 1 - i)) & 1) as u8;
            let byte_idx = self.pos >> 3;
            let bit_idx = 7 - (self.pos & 7);
            if bit != 0 {
                self.buf[byte_idx] |= 1 << bit_idx;
            } else {
                self.buf[byte_idx] &= !(1u8 << bit_idx);
            }
            self.pos += 1;
        }
    }

    /// Write a single bit.
    pub fn write_bit(&mut self, val: u8) {
        self.ensure(1);
        let byte_idx = self.pos >> 3;
        let bit_idx = 7 - (self.pos & 7);
        if val != 0 {
            self.buf[byte_idx] |= 1 << bit_idx;
        } else {
            self.buf[byte_idx] &= !(1u8 << bit_idx);
        }
        self.pos += 1;
    }

    /// Write an 8-bit unsigned integer.
    pub fn write_u8(&mut self, val: u8) {
        self.write_bits(val as u64, 8);
    }

    /// Write a 16-bit unsigned integer (big-endian).
    pub fn write_u16(&mut self, val: u16) {
        self.write_bits(val as u64, 16);
    }

    /// Write a 32-bit unsigned integer (big-endian).
    pub fn write_u32(&mut self, val: u32) {
        self.write_bits(val as u64, 32);
    }

    /// Write a 64-bit value as two 32-bit halves (hi, lo) big-endian.
    pub fn write_u64_parts(&mut self, hi: u32, lo: u32) {
        self.write_bits(hi as u64, 32);
        self.write_bits(lo as u64, 32);
    }

    /// Write raw bytes.
    pub fn write_bytes(&mut self, data: &[u8]) {
        for &b in data {
            self.write_u8(b);
        }
    }

    /// Pad to the next byte boundary with zero bits (no-op if aligned).
    pub fn align_to_byte(&mut self) {
        let rem = self.pos & 7;
        if rem != 0 {
            let pad = 8 - rem;
            self.ensure(pad);
            // The buffer is already zeroed on allocation / resize, but make
            // sure the padding bits are zero in case we overwrote earlier.
            for _ in 0..pad {
                let byte_idx = self.pos >> 3;
                let bit_idx = 7 - (self.pos & 7);
                self.buf[byte_idx] &= !(1u8 << bit_idx);
                self.pos += 1;
            }
        }
    }

    /// Return the written data truncated to the current bit position (rounded
    /// up to full bytes).
    pub fn to_bytes(&self) -> Vec<u8> {
        let length = (self.pos + 7) >> 3;
        self.buf[..length].to_vec()
    }

    /// Get a mutable reference to the underlying buffer. Used for patching
    /// header offsets after encoding.
    pub fn buffer_mut(&mut self) -> &mut [u8] {
        &mut self.buf
    }
}

// ---------------------------------------------------------------------------
// BitReader
// ---------------------------------------------------------------------------

/// Read from a byte buffer with bit-level operations, MSB-first.
pub struct BitReader<'a> {
    buf: &'a [u8],
    bit_len: usize,
    pos: usize,
}

impl<'a> BitReader<'a> {
    /// Create a reader over the given buffer.
    pub fn new(buf: &'a [u8]) -> Self {
        Self {
            buf,
            bit_len: buf.len() * 8,
            pos: 0,
        }
    }

    /// Current read position in bits.
    pub fn position(&self) -> usize {
        self.pos
    }

    /// Number of bits remaining.
    pub fn remaining(&self) -> usize {
        if self.pos >= self.bit_len {
            0
        } else {
            self.bit_len - self.pos
        }
    }

    /// Seek to an absolute bit position.
    pub fn seek(&mut self, bit_pos: usize) {
        self.pos = bit_pos;
    }

    /// Advance the cursor by `n` bits.
    pub fn advance(&mut self, n: usize) {
        self.pos += n;
    }

    /// Align to the next byte boundary (no-op if aligned).
    pub fn align_to_byte(&mut self) {
        let rem = self.pos & 7;
        if rem != 0 {
            self.pos += 8 - rem;
        }
    }

    /// Read `n` bits (1..64) as an unsigned value, MSB-first.
    pub fn read_bits(&mut self, n: usize) -> Result<u64, TriePackError> {
        if n == 0 || n > 64 {
            return Err(TriePackError::InvalidParam("read_bits: n must be 1..64"));
        }
        if self.pos + n > self.bit_len {
            return Err(TriePackError::Eof);
        }
        let mut val: u64 = 0;
        for _ in 0..n {
            let byte_idx = self.pos >> 3;
            let bit_idx = 7 - (self.pos & 7);
            val = (val << 1) | ((self.buf[byte_idx] >> bit_idx) & 1) as u64;
            self.pos += 1;
        }
        Ok(val)
    }

    /// Read a single bit.
    pub fn read_bit(&mut self) -> Result<u8, TriePackError> {
        if self.pos >= self.bit_len {
            return Err(TriePackError::Eof);
        }
        let byte_idx = self.pos >> 3;
        let bit_idx = 7 - (self.pos & 7);
        let bit = (self.buf[byte_idx] >> bit_idx) & 1;
        self.pos += 1;
        Ok(bit)
    }

    /// Read an 8-bit unsigned integer.
    pub fn read_u8(&mut self) -> Result<u8, TriePackError> {
        Ok(self.read_bits(8)? as u8)
    }

    /// Read a 16-bit unsigned integer (big-endian).
    pub fn read_u16(&mut self) -> Result<u16, TriePackError> {
        Ok(self.read_bits(16)? as u16)
    }

    /// Read a 32-bit unsigned integer (big-endian).
    pub fn read_u32(&mut self) -> Result<u32, TriePackError> {
        Ok(self.read_bits(32)? as u32)
    }

    /// Read a 64-bit unsigned integer as two 32-bit halves (big-endian).
    pub fn read_u64(&mut self) -> Result<u64, TriePackError> {
        let hi = self.read_u32()? as u64;
        let lo = self.read_u32()? as u64;
        Ok(hi * 0x1_0000_0000 + lo)
    }

    /// Read `n` raw bytes.
    pub fn read_bytes(&mut self, n: usize) -> Result<Vec<u8>, TriePackError> {
        let mut out = vec![0u8; n];
        for byte in out.iter_mut() {
            *byte = self.read_u8()?;
        }
        Ok(out)
    }

    /// Peek at the next `n` bits without advancing the cursor.
    pub fn peek_bits(&mut self, n: usize) -> Result<u64, TriePackError> {
        let saved = self.pos;
        let val = self.read_bits(n);
        self.pos = saved;
        val
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_write_read_bits() {
        let mut w = BitWriter::new(16);
        w.write_bits(0b110, 3);
        w.write_bits(0b01011, 5);
        let data = w.to_bytes();
        assert_eq!(data, vec![0b1100_1011]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_bits(3).unwrap(), 0b110);
        assert_eq!(r.read_bits(5).unwrap(), 0b01011);
    }

    #[test]
    fn test_write_read_u8() {
        let mut w = BitWriter::new(16);
        w.write_u8(0xAB);
        w.write_u8(0xCD);
        let data = w.to_bytes();
        assert_eq!(data, vec![0xAB, 0xCD]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_u8().unwrap(), 0xAB);
        assert_eq!(r.read_u8().unwrap(), 0xCD);
    }

    #[test]
    fn test_write_read_u16() {
        let mut w = BitWriter::new(16);
        w.write_u16(0x1234);
        let data = w.to_bytes();
        assert_eq!(data, vec![0x12, 0x34]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_u16().unwrap(), 0x1234);
    }

    #[test]
    fn test_write_read_u32() {
        let mut w = BitWriter::new(16);
        w.write_u32(0xDEADBEEF);
        let data = w.to_bytes();
        assert_eq!(data, vec![0xDE, 0xAD, 0xBE, 0xEF]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_u32().unwrap(), 0xDEADBEEF);
    }

    #[test]
    fn test_align_to_byte() {
        let mut w = BitWriter::new(16);
        w.write_bits(0b11, 2);
        w.align_to_byte();
        assert_eq!(w.position(), 8);
        w.write_u8(0xFF);
        let data = w.to_bytes();
        assert_eq!(data, vec![0b1100_0000, 0xFF]);
    }

    #[test]
    fn test_peek_bits() {
        let data = vec![0b1010_0110];
        let mut r = BitReader::new(&data);
        assert_eq!(r.peek_bits(4).unwrap(), 0b1010);
        assert_eq!(r.position(), 0);
        assert_eq!(r.read_bits(4).unwrap(), 0b1010);
        assert_eq!(r.position(), 4);
    }

    #[test]
    fn test_read_past_end() {
        let data = vec![0xFF];
        let mut r = BitReader::new(&data);
        assert_eq!(r.read_bits(8).unwrap(), 0xFF);
        assert!(r.read_bit().is_err());
    }

    #[test]
    fn test_write_read_bytes() {
        let mut w = BitWriter::new(16);
        w.write_bytes(&[0x01, 0x02, 0x03]);
        let data = w.to_bytes();
        assert_eq!(data, vec![0x01, 0x02, 0x03]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_bytes(3).unwrap(), vec![0x01, 0x02, 0x03]);
    }

    #[test]
    fn test_read_u64() {
        let mut w = BitWriter::new(16);
        w.write_u64_parts(0x12345678, 0x9ABCDEF0);
        let data = w.to_bytes();

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_u64().unwrap(), 0x123456789ABCDEF0);
    }

    #[test]
    fn test_remaining_and_seek() {
        let data = vec![0xFF, 0x00];
        let mut r = BitReader::new(&data);
        assert_eq!(r.remaining(), 16);
        r.seek(5);
        assert_eq!(r.remaining(), 11);
        assert_eq!(r.position(), 5);
    }

    #[test]
    fn test_reader_align_to_byte() {
        let data = vec![0xFF, 0xAA];
        let mut r = BitReader::new(&data);
        r.read_bits(3).unwrap();
        r.align_to_byte();
        assert_eq!(r.position(), 8);
        assert_eq!(r.read_u8().unwrap(), 0xAA);
    }

    #[test]
    fn test_writer_already_aligned() {
        let mut w = BitWriter::new(16);
        w.write_u8(0xFF);
        w.align_to_byte(); // already aligned, should be no-op
        assert_eq!(w.position(), 8);
    }

    #[test]
    fn test_single_bit_write_read() {
        let mut w = BitWriter::new(16);
        w.write_bit(1);
        w.write_bit(0);
        w.write_bit(1);
        w.write_bit(1);
        w.write_bit(0);
        w.write_bit(0);
        w.write_bit(1);
        w.write_bit(0);
        let data = w.to_bytes();
        assert_eq!(data, vec![0b1011_0010]);

        let mut r = BitReader::new(&data);
        assert_eq!(r.read_bit().unwrap(), 1);
        assert_eq!(r.read_bit().unwrap(), 0);
        assert_eq!(r.read_bit().unwrap(), 1);
        assert_eq!(r.read_bit().unwrap(), 1);
        assert_eq!(r.read_bit().unwrap(), 0);
        assert_eq!(r.read_bit().unwrap(), 0);
        assert_eq!(r.read_bit().unwrap(), 1);
        assert_eq!(r.read_bit().unwrap(), 0);
    }
}
