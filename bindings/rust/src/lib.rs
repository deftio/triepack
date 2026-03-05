// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! triepack -- Native Rust implementation of the TriePack .trp binary format.
//!
//! Provides `encode` and `decode` functions for converting between
//! `HashMap<String, Value>` and the .trp binary format, producing
//! byte-identical output to the C reference implementation.

pub mod bitstream;
pub mod crc32;
pub mod decoder;
pub mod encoder;
pub mod values;
pub mod varint;

pub use values::Value;

use std::collections::HashMap;
use std::fmt;

/// Error type for TriePack operations.
#[derive(Debug, Clone, PartialEq)]
pub enum TriePackError {
    /// Read past end of stream.
    Eof,
    /// NULL pointer or invalid argument.
    InvalidParam(&'static str),
    /// VarInt exceeds max groups.
    Overflow,
    /// Not a .trp file (bad magic bytes).
    BadMagic,
    /// Unsupported format version.
    Version,
    /// Integrity check failed (CRC mismatch).
    Corrupt,
    /// Data shorter than header claims.
    Truncated,
    /// Malformed UTF-8 in key or string value.
    InvalidUtf8,
}

impl fmt::Display for TriePackError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TriePackError::Eof => write!(f, "Read past end of stream"),
            TriePackError::InvalidParam(msg) => write!(f, "Invalid parameter: {}", msg),
            TriePackError::Overflow => write!(f, "VarInt overflow"),
            TriePackError::BadMagic => write!(f, "Invalid magic bytes -- not a .trp file"),
            TriePackError::Version => write!(f, "Unsupported format version"),
            TriePackError::Corrupt => write!(f, "CRC-32 integrity check failed"),
            TriePackError::Truncated => write!(f, "Data too short for .trp format"),
            TriePackError::InvalidUtf8 => write!(f, "Invalid UTF-8"),
        }
    }
}

impl std::error::Error for TriePackError {}

/// Encode key-value data into the .trp binary format.
///
/// Keys are sorted by their UTF-8 byte representation. The output is
/// byte-identical to the C reference encoder.
pub fn encode(data: &HashMap<String, Value>) -> Vec<u8> {
    encoder::encode(data)
}

/// Decode a .trp binary buffer into key-value data.
///
/// Validates magic bytes, version, and CRC-32 checksum before decoding.
pub fn decode(buffer: &[u8]) -> Result<HashMap<String, Value>, TriePackError> {
    decoder::decode(buffer)
}
