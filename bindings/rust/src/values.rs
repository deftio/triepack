// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Value encoder/decoder for 8 type tags -- matches src/core/value.c.

use crate::bitstream::{BitReader, BitWriter};
use crate::varint::{read_var_int, read_var_uint, write_var_int, write_var_uint};
use crate::TriePackError;

// Type tags (4-bit, matches tp_value_type enum)
const TP_NULL: u64 = 0;
const TP_BOOL: u64 = 1;
const TP_INT: u64 = 2;
const TP_UINT: u64 = 3;
#[allow(dead_code)]
const TP_FLOAT32: u64 = 4;
const TP_FLOAT64: u64 = 5;
const TP_STRING: u64 = 6;
const TP_BLOB: u64 = 7;

/// Tagged union for values stored in a triepack dictionary.
#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    /// Null value (no data).
    Null,
    /// Boolean value.
    Bool(bool),
    /// Signed 64-bit integer (zigzag-encoded, for negative values).
    Int(i64),
    /// Unsigned 64-bit integer.
    UInt(u64),
    /// 64-bit IEEE 754 double.
    Float64(f64),
    /// UTF-8 string.
    String(String),
    /// Raw binary blob.
    Blob(Vec<u8>),
}

/// Encode a value into the bitstream.
pub fn encode_value(w: &mut BitWriter, val: &Value) {
    match val {
        Value::Null => {
            w.write_bits(TP_NULL, 4);
        }
        Value::Bool(b) => {
            w.write_bits(TP_BOOL, 4);
            w.write_bit(if *b { 1 } else { 0 });
        }
        Value::Int(v) => {
            w.write_bits(TP_INT, 4);
            write_var_int(w, *v);
        }
        Value::UInt(v) => {
            w.write_bits(TP_UINT, 4);
            write_var_uint(w, *v);
        }
        Value::Float64(v) => {
            w.write_bits(TP_FLOAT64, 4);
            let bits = v.to_bits();
            let hi = (bits >> 32) as u32;
            let lo = bits as u32;
            w.write_u64_parts(hi, lo);
        }
        Value::String(s) => {
            let bytes = s.as_bytes();
            w.write_bits(TP_STRING, 4);
            write_var_uint(w, bytes.len() as u64);
            w.align_to_byte();
            w.write_bytes(bytes);
        }
        Value::Blob(data) => {
            w.write_bits(TP_BLOB, 4);
            write_var_uint(w, data.len() as u64);
            w.align_to_byte();
            w.write_bytes(data);
        }
    }
}

/// Decode a value from the bitstream.
pub fn decode_value(r: &mut BitReader) -> Result<Value, TriePackError> {
    let tag = r.read_bits(4)?;

    match tag {
        TP_NULL => Ok(Value::Null),
        TP_BOOL => {
            let bit = r.read_bit()?;
            Ok(Value::Bool(bit != 0))
        }
        TP_INT => {
            let v = read_var_int(r)?;
            Ok(Value::Int(v))
        }
        TP_UINT => {
            let v = read_var_uint(r)?;
            Ok(Value::UInt(v))
        }
        TP_FLOAT32 => {
            let bits = r.read_u32()?;
            let f = f32::from_bits(bits);
            // Store as f64 since our Value enum only has Float64
            Ok(Value::Float64(f as f64))
        }
        TP_FLOAT64 => {
            let hi = r.read_u32()? as u64;
            let lo = r.read_u32()? as u64;
            let bits = (hi << 32) | lo;
            let f = f64::from_bits(bits);
            Ok(Value::Float64(f))
        }
        TP_STRING => {
            let slen = read_var_uint(r)? as usize;
            r.align_to_byte();
            let raw = r.read_bytes(slen)?;
            let s = std::string::String::from_utf8(raw)
                .map_err(|_| TriePackError::InvalidUtf8)?;
            Ok(Value::String(s))
        }
        TP_BLOB => {
            let blen = read_var_uint(r)? as usize;
            r.align_to_byte();
            let data = r.read_bytes(blen)?;
            Ok(Value::Blob(data))
        }
        _ => Err(TriePackError::InvalidParam("Unknown value type tag")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn roundtrip(val: &Value) -> Value {
        let mut w = BitWriter::new(64);
        encode_value(&mut w, val);
        let data = w.to_bytes();
        let mut r = BitReader::new(&data);
        decode_value(&mut r).unwrap()
    }

    #[test]
    fn test_null() {
        let v = roundtrip(&Value::Null);
        assert_eq!(v, Value::Null);
    }

    #[test]
    fn test_bool() {
        assert_eq!(roundtrip(&Value::Bool(true)), Value::Bool(true));
        assert_eq!(roundtrip(&Value::Bool(false)), Value::Bool(false));
    }

    #[test]
    fn test_int() {
        assert_eq!(roundtrip(&Value::Int(-100)), Value::Int(-100));
        assert_eq!(roundtrip(&Value::Int(0)), Value::Int(0));
        assert_eq!(roundtrip(&Value::Int(-1)), Value::Int(-1));
    }

    #[test]
    fn test_uint() {
        assert_eq!(roundtrip(&Value::UInt(42)), Value::UInt(42));
        assert_eq!(roundtrip(&Value::UInt(0)), Value::UInt(0));
        assert_eq!(roundtrip(&Value::UInt(200)), Value::UInt(200));
    }

    #[test]
    fn test_float64() {
        let val = Value::Float64(3.14159);
        let result = roundtrip(&val);
        if let Value::Float64(f) = result {
            assert!((f - 3.14159).abs() < 1e-10);
        } else {
            panic!("Expected Float64");
        }
    }

    #[test]
    fn test_string() {
        let val = Value::String("hello".to_string());
        assert_eq!(roundtrip(&val), val);
    }

    #[test]
    fn test_blob() {
        let val = Value::Blob(vec![0x01, 0x02, 0x03]);
        assert_eq!(roundtrip(&val), val);
    }

    #[test]
    fn test_empty_string() {
        let val = Value::String(String::new());
        assert_eq!(roundtrip(&val), val);
    }

    #[test]
    fn test_empty_blob() {
        let val = Value::Blob(Vec::new());
        assert_eq!(roundtrip(&val), val);
    }
}
