// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Public API surface: the error type, the fallible encoder, and the guards
//! that only fire on input a safe caller cannot construct.

use std::collections::HashMap;
use triepack::{decode, encode, try_encode, TriePackError, Value, MAX_ALPHABET_SIZE};

// ---------------------------------------------------------------------------
// TriePackError
// ---------------------------------------------------------------------------

/// Every variant has to render, including the one that interpolates.
#[test]
fn error_display_covers_every_variant() {
    let cases: Vec<(TriePackError, &str)> = vec![
        (TriePackError::Eof, "Read past end of stream"),
        (TriePackError::InvalidParam("bad thing"), "bad thing"),
        (TriePackError::Overflow, "VarInt overflow"),
        (TriePackError::BadMagic, "not a .trp file"),
        (TriePackError::Version, "Unsupported format version"),
        (TriePackError::Corrupt, "CRC-32"),
        (TriePackError::Truncated, "too short"),
        (TriePackError::InvalidUtf8, "Invalid UTF-8"),
        (TriePackError::Alphabet(300), "300"),
    ];
    for (err, expect) in cases {
        let rendered = format!("{}", err);
        assert!(
            rendered.contains(expect),
            "{:?} rendered as {:?}, expected it to contain {:?}",
            err,
            rendered,
            expect
        );
    }
    // The alphabet message names the limit as well as the offending size.
    let msg = format!("{}", TriePackError::Alphabet(300));
    assert!(msg.contains(&MAX_ALPHABET_SIZE.to_string()));
}

/// It is an ordinary error, so `?` into `Box<dyn Error>` has to work.
#[test]
fn error_implements_std_error() {
    fn fallible() -> Result<(), Box<dyn std::error::Error>> {
        Err(Box::new(TriePackError::BadMagic))
    }
    let err = fallible().unwrap_err();
    assert!(err.to_string().contains("not a .trp file"));
}

#[test]
fn errors_compare_and_clone() {
    let a = TriePackError::Alphabet(7);
    assert_eq!(a, a.clone());
    assert_ne!(a, TriePackError::Alphabet(8));
    assert_ne!(TriePackError::Eof, TriePackError::Corrupt);
}

// ---------------------------------------------------------------------------
// try_encode
// ---------------------------------------------------------------------------

#[test]
fn try_encode_matches_encode() {
    let mut data = HashMap::new();
    data.insert("alpha".to_string(), Value::Int(1));
    data.insert("beta".to_string(), Value::String("two".to_string()));

    let via_try = try_encode(&data).expect("ordinary data encodes");
    assert_eq!(via_try, encode(&data));
    assert_eq!(decode(&via_try).unwrap(), data);
}

/// Build keys spanning more distinct byte values than the format can address.
///
/// Safe Rust cannot do this: `String` is always UTF-8, which reaches at most
/// 242 distinct bytes — below the ceiling. The guard exists for parity with
/// the C and Python encoders, which take raw bytes and *can* reach it, so
/// exercising it here needs a deliberately invalid `String`. Nothing decodes
/// these; the encoder is expected to refuse before they are written.
fn keys_with_every_byte_value() -> HashMap<String, Value> {
    let mut data = HashMap::new();
    for b in 1u8..=255 {
        // SAFETY: the bytes are never read as UTF-8 — the encoder rejects
        // this input on alphabet width before it interprets any key.
        let key = unsafe { String::from_utf8_unchecked(vec![b]) };
        data.insert(key, Value::Null);
    }
    data
}

#[test]
fn try_encode_rejects_an_over_wide_alphabet() {
    match try_encode(&keys_with_every_byte_value()) {
        Err(TriePackError::Alphabet(n)) => assert_eq!(n, 255),
        other => panic!("expected an Alphabet error, got {:?}", other.map(|b| b.len())),
    }
}

#[test]
#[should_panic(expected = "encode failed")]
fn encode_panics_where_try_encode_reports() {
    // The infallible wrapper turns the same condition into a panic.
    let _ = encode(&keys_with_every_byte_value());
}

#[test]
fn an_alphabet_at_the_limit_still_encodes() {
    let mut data = HashMap::new();
    for b in 1u8..=(MAX_ALPHABET_SIZE as u8) {
        // SAFETY: as above — this exercises the boundary, not the decoder.
        let key = unsafe { String::from_utf8_unchecked(vec![b]) };
        data.insert(key, Value::Null);
    }
    assert!(try_encode(&data).is_ok(), "{} symbols must encode", MAX_ALPHABET_SIZE);
}
