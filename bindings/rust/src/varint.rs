// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! LEB128 unsigned VarInt + zigzag signed VarInt -- matches bitstream_varint.c.

use crate::bitstream::{BitReader, BitWriter};
use crate::TriePackError;

const VARINT_MAX_GROUPS: usize = 10;

/// Write an unsigned LEB128 VarInt.
pub fn write_var_uint(w: &mut BitWriter, value: u64) {
    let mut val = value;
    loop {
        let mut byte = (val & 0x7F) as u8;
        val >>= 7;
        if val > 0 {
            byte |= 0x80;
        }
        w.write_u8(byte);
        if val == 0 {
            break;
        }
    }
}

/// Read an unsigned LEB128 VarInt.
pub fn read_var_uint(r: &mut BitReader) -> Result<u64, TriePackError> {
    let mut val: u64 = 0;
    let mut shift: u32 = 0;
    for _ in 0..VARINT_MAX_GROUPS {
        let byte = r.read_u8()?;
        val |= ((byte & 0x7F) as u64) << shift;
        if byte & 0x80 == 0 {
            return Ok(val);
        }
        shift += 7;
    }
    Err(TriePackError::Overflow)
}

/// Write a signed zigzag VarInt.
pub fn write_var_int(w: &mut BitWriter, value: i64) {
    let raw = if value >= 0 {
        (value as u64) * 2
    } else {
        // Use wrapping arithmetic to handle i64::MIN correctly.
        // For negative v: zigzag = (-v) * 2 - 1
        // Rewritten as: (-(v + 1)) * 2 + 1 to avoid overflow on i64::MIN.
        let abs_minus_one = !value as u64; // == (-value - 1) as u64
        abs_minus_one * 2 + 1
    };
    write_var_uint(w, raw);
}

/// Read a signed zigzag VarInt.
pub fn read_var_int(r: &mut BitReader) -> Result<i64, TriePackError> {
    let raw = read_var_uint(r)?;
    if raw & 1 != 0 {
        Ok(-((raw >> 1) as i64) - 1)
    } else {
        Ok((raw >> 1) as i64)
    }
}

/// Return the number of bits needed to encode `val` as an unsigned VarInt.
pub fn var_uint_bits(val: u64) -> u64 {
    let mut bits: u64 = 0;
    let mut v = val;
    loop {
        bits += 8;
        v >>= 7;
        if v == 0 {
            break;
        }
    }
    bits
}

#[cfg(test)]
mod tests {
    use super::*;

    fn roundtrip_uint(value: u64) {
        let mut w = BitWriter::new(16);
        write_var_uint(&mut w, value);
        let data = w.to_bytes();
        let mut r = BitReader::new(&data);
        assert_eq!(read_var_uint(&mut r).unwrap(), value);
    }

    fn roundtrip_int(value: i64) {
        let mut w = BitWriter::new(16);
        write_var_int(&mut w, value);
        let data = w.to_bytes();
        let mut r = BitReader::new(&data);
        assert_eq!(read_var_int(&mut r).unwrap(), value);
    }

    #[test]
    fn test_var_uint_zero() {
        roundtrip_uint(0);
    }

    #[test]
    fn test_var_uint_small() {
        roundtrip_uint(42);
        roundtrip_uint(127);
    }

    #[test]
    fn test_var_uint_medium() {
        roundtrip_uint(128);
        roundtrip_uint(300);
        roundtrip_uint(16384);
    }

    #[test]
    fn test_var_uint_large() {
        roundtrip_uint(u64::MAX);
        roundtrip_uint(0xFFFFFFFF);
    }

    #[test]
    fn test_var_int_positive() {
        roundtrip_int(0);
        roundtrip_int(1);
        roundtrip_int(42);
        roundtrip_int(i64::MAX);
    }

    #[test]
    fn test_var_int_negative() {
        roundtrip_int(-1);
        roundtrip_int(-100);
        roundtrip_int(i64::MIN);
    }

    #[test]
    fn test_var_uint_bits() {
        assert_eq!(var_uint_bits(0), 8);
        assert_eq!(var_uint_bits(127), 8);
        assert_eq!(var_uint_bits(128), 16);
        assert_eq!(var_uint_bits(16383), 16);
        assert_eq!(var_uint_bits(16384), 24);
    }

    #[test]
    fn test_var_uint_encoding_bytes() {
        // Value 0 encodes as single byte 0x00
        let mut w = BitWriter::new(16);
        write_var_uint(&mut w, 0);
        assert_eq!(w.to_bytes(), vec![0x00]);

        // Value 300 encodes as 0xAC 0x02
        let mut w2 = BitWriter::new(16);
        write_var_uint(&mut w2, 300);
        assert_eq!(w2.to_bytes(), vec![0xAC, 0x02]);
    }
}
