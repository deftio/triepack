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

pub use encoder::MAX_ALPHABET_SIZE;
pub use values::Value;

/// Library version, kept in step with triepack-version.txt by
/// scripts/sync_version.sh.
pub const VERSION: &str = "1.3.1";

/// Version of the on-disk .trp format this implementation writes. Distinct
/// from the library version: it changes only when the bytes change.
pub const FORMAT_VERSION_MAJOR: u8 = 1;
/// See [`FORMAT_VERSION_MAJOR`].
pub const FORMAT_VERSION_MINOR: u8 = 0;

/// Metadata about a triepack build. Every implementation reports the same
/// fields, so a polyglot system can ask each one what it is.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VersionInfo {
    pub name: &'static str,
    pub implementation: &'static str,
    pub version: &'static str,
    pub version_major: u8,
    pub version_minor: u8,
    pub version_patch: u8,
    pub format_version_major: u8,
    pub format_version_minor: u8,
    pub max_alphabet_size: usize,
}

/// Return metadata about this build.
pub fn version() -> VersionInfo {
    let mut parts = VERSION.split('.').map(|p| p.parse().unwrap_or(0));
    VersionInfo {
        name: "triepack",
        implementation: "rust",
        version: VERSION,
        version_major: parts.next().unwrap_or(0),
        version_minor: parts.next().unwrap_or(0),
        version_patch: parts.next().unwrap_or(0),
        format_version_major: FORMAT_VERSION_MAJOR,
        format_version_minor: FORMAT_VERSION_MINOR,
        max_alphabet_size: MAX_ALPHABET_SIZE,
    }
}

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
    /// Keys use more distinct byte values than the format can address.
    /// Carries the alphabet size that was rejected.
    Alphabet(usize),
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
            TriePackError::Alphabet(n) => write!(
                f,
                "Keys use too many distinct byte values ({}); the format allows at most {}",
                n,
                encoder::MAX_ALPHABET_SIZE
            ),
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

/// Encode key-value data, reporting the alphabet limit as an error instead of
/// panicking. See [`encode`] and [`MAX_ALPHABET_SIZE`].
pub fn try_encode(data: &HashMap<String, Value>) -> Result<Vec<u8>, TriePackError> {
    encoder::try_encode(data)
}

/// Decode a .trp binary buffer into key-value data.
///
/// Validates magic bytes, version, and CRC-32 checksum before decoding.
pub fn decode(buffer: &[u8]) -> Result<HashMap<String, Value>, TriePackError> {
    decoder::decode(buffer)
}
