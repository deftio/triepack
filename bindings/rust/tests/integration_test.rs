// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Integration tests: roundtrip + fixture tests for the triepack crate.

use std::collections::HashMap;
use triepack::{decode, encode, Value};

// ---------------------------------------------------------------------------
// Helper: load fixture file
// ---------------------------------------------------------------------------

fn fixture_path(name: &str) -> std::path::PathBuf {
    let mut path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.push("..");
    path.push("..");
    path.push("tests");
    path.push("fixtures");
    path.push(name);
    path
}

fn load_fixture(name: &str) -> Vec<u8> {
    let path = fixture_path(name);
    std::fs::read(&path).unwrap_or_else(|e| panic!("Failed to read fixture {:?}: {}", path, e))
}

// ---------------------------------------------------------------------------
// Fixture decode tests
// ---------------------------------------------------------------------------

#[test]
fn test_fixture_decode_empty() {
    let buf = load_fixture("empty.trp");
    let result = decode(&buf).unwrap();
    assert!(result.is_empty());
}

#[test]
fn test_fixture_decode_single_null() {
    let buf = load_fixture("single_null.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 1);
    assert_eq!(result.get("hello"), Some(&Value::Null));
}

#[test]
fn test_fixture_decode_single_int() {
    let buf = load_fixture("single_int.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 1);
    assert_eq!(result.get("key"), Some(&Value::UInt(42)));
}

#[test]
fn test_fixture_decode_multi_mixed() {
    let buf = load_fixture("multi_mixed.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 5);
    assert_eq!(result.get("bool"), Some(&Value::Bool(true)));
    // Float64 comparison
    match result.get("f64") {
        Some(Value::Float64(f)) => assert!((*f - 3.14159).abs() < 1e-10),
        other => panic!("Expected Float64(3.14159), got {:?}", other),
    }
    assert_eq!(result.get("int"), Some(&Value::Int(-100)));
    assert_eq!(
        result.get("str"),
        Some(&Value::String("hello".to_string()))
    );
    assert_eq!(result.get("uint"), Some(&Value::UInt(200)));
}

#[test]
fn test_fixture_decode_shared_prefix() {
    let buf = load_fixture("shared_prefix.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result.get("abc"), Some(&Value::UInt(10)));
    assert_eq!(result.get("abd"), Some(&Value::UInt(20)));
    assert_eq!(result.get("xyz"), Some(&Value::UInt(30)));
}

#[test]
fn test_fixture_decode_large() {
    let buf = load_fixture("large.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 100);
    for i in 0..100u64 {
        let key = format!("key_{:04}", i);
        assert_eq!(
            result.get(&key),
            Some(&Value::UInt(i)),
            "Mismatch for key {}",
            key
        );
    }
}

#[test]
fn test_fixture_decode_keys_only() {
    let buf = load_fixture("keys_only.trp");
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result.get("apple"), Some(&Value::Null));
    assert_eq!(result.get("banana"), Some(&Value::Null));
    assert_eq!(result.get("cherry"), Some(&Value::Null));
}

// ---------------------------------------------------------------------------
// Fixture encode tests (byte-identical to C reference)
// ---------------------------------------------------------------------------

#[test]
fn test_fixture_encode_empty() {
    let data = HashMap::new();
    let encoded = encode(&data);
    let expected = load_fixture("empty.trp");
    assert_eq!(
        encoded, expected,
        "Encoded empty dict differs from fixture (got {} bytes, expected {} bytes)",
        encoded.len(),
        expected.len()
    );
}

#[test]
fn test_fixture_encode_single_null() {
    let mut data = HashMap::new();
    data.insert("hello".to_string(), Value::Null);
    let encoded = encode(&data);
    let expected = load_fixture("single_null.trp");
    assert_eq!(
        encoded, expected,
        "Encoded single_null differs from fixture"
    );
}

#[test]
fn test_fixture_encode_single_int() {
    let mut data = HashMap::new();
    data.insert("key".to_string(), Value::UInt(42));
    let encoded = encode(&data);
    let expected = load_fixture("single_int.trp");
    assert_eq!(
        encoded, expected,
        "Encoded single_int differs from fixture"
    );
}

#[test]
fn test_fixture_encode_multi_mixed() {
    let mut data = HashMap::new();
    data.insert("bool".to_string(), Value::Bool(true));
    data.insert("f64".to_string(), Value::Float64(3.14159));
    data.insert("int".to_string(), Value::Int(-100));
    data.insert("str".to_string(), Value::String("hello".to_string()));
    data.insert("uint".to_string(), Value::UInt(200));
    let encoded = encode(&data);
    let expected = load_fixture("multi_mixed.trp");
    assert_eq!(
        encoded, expected,
        "Encoded multi_mixed differs from fixture"
    );
}

#[test]
fn test_fixture_encode_shared_prefix() {
    let mut data = HashMap::new();
    data.insert("abc".to_string(), Value::UInt(10));
    data.insert("abd".to_string(), Value::UInt(20));
    data.insert("xyz".to_string(), Value::UInt(30));
    let encoded = encode(&data);
    let expected = load_fixture("shared_prefix.trp");
    assert_eq!(
        encoded, expected,
        "Encoded shared_prefix differs from fixture"
    );
}

#[test]
fn test_fixture_encode_large() {
    let mut data = HashMap::new();
    for i in 0..100u64 {
        let key = format!("key_{:04}", i);
        data.insert(key, Value::UInt(i));
    }
    let encoded = encode(&data);
    let expected = load_fixture("large.trp");
    assert_eq!(encoded, expected, "Encoded large differs from fixture");
}

#[test]
fn test_fixture_encode_keys_only() {
    let mut data = HashMap::new();
    data.insert("apple".to_string(), Value::Null);
    data.insert("banana".to_string(), Value::Null);
    data.insert("cherry".to_string(), Value::Null);
    let encoded = encode(&data);
    let expected = load_fixture("keys_only.trp");
    assert_eq!(
        encoded, expected,
        "Encoded keys_only differs from fixture"
    );
}

// ---------------------------------------------------------------------------
// Roundtrip tests
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_empty() {
    let data: HashMap<String, Value> = HashMap::new();
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result, data);
}

#[test]
fn test_roundtrip_single_key() {
    let mut data = HashMap::new();
    data.insert("test".to_string(), Value::UInt(123));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result, data);
}

#[test]
fn test_roundtrip_mixed_types() {
    let mut data = HashMap::new();
    data.insert("null_val".to_string(), Value::Null);
    data.insert("bool_val".to_string(), Value::Bool(false));
    data.insert("int_val".to_string(), Value::Int(-42));
    data.insert("uint_val".to_string(), Value::UInt(999));
    data.insert("float_val".to_string(), Value::Float64(2.71828));
    data.insert(
        "string_val".to_string(),
        Value::String("world".to_string()),
    );
    data.insert("blob_val".to_string(), Value::Blob(vec![0xDE, 0xAD]));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), data.len());

    assert_eq!(result.get("null_val"), Some(&Value::Null));
    assert_eq!(result.get("bool_val"), Some(&Value::Bool(false)));
    assert_eq!(result.get("int_val"), Some(&Value::Int(-42)));
    assert_eq!(result.get("uint_val"), Some(&Value::UInt(999)));
    match result.get("float_val") {
        Some(Value::Float64(f)) => assert!((*f - 2.71828).abs() < 1e-10),
        other => panic!("Expected Float64, got {:?}", other),
    }
    assert_eq!(
        result.get("string_val"),
        Some(&Value::String("world".to_string()))
    );
    assert_eq!(
        result.get("blob_val"),
        Some(&Value::Blob(vec![0xDE, 0xAD]))
    );
}

#[test]
fn test_roundtrip_large_dict() {
    let mut data = HashMap::new();
    for i in 0..500u64 {
        let key = format!("item_{:05}", i);
        data.insert(key, Value::UInt(i * 3));
    }
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 500);
    for i in 0..500u64 {
        let key = format!("item_{:05}", i);
        assert_eq!(result.get(&key), Some(&Value::UInt(i * 3)));
    }
}

#[test]
fn test_roundtrip_utf8_keys() {
    let mut data = HashMap::new();
    data.insert("cafe\u{0301}".to_string(), Value::UInt(1));
    data.insert("\u{1F600}".to_string(), Value::UInt(2));
    data.insert("\u{4F60}\u{597D}".to_string(), Value::UInt(3));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result.get("cafe\u{0301}"), Some(&Value::UInt(1)));
    assert_eq!(result.get("\u{1F600}"), Some(&Value::UInt(2)));
    assert_eq!(result.get("\u{4F60}\u{597D}"), Some(&Value::UInt(3)));
}

#[test]
fn test_roundtrip_shared_prefixes() {
    let mut data = HashMap::new();
    data.insert("prefix".to_string(), Value::UInt(0));
    data.insert("prefix_a".to_string(), Value::UInt(1));
    data.insert("prefix_ab".to_string(), Value::UInt(2));
    data.insert("prefix_abc".to_string(), Value::UInt(3));
    data.insert("prefix_b".to_string(), Value::UInt(4));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 5);
    assert_eq!(result.get("prefix"), Some(&Value::UInt(0)));
    assert_eq!(result.get("prefix_a"), Some(&Value::UInt(1)));
    assert_eq!(result.get("prefix_ab"), Some(&Value::UInt(2)));
    assert_eq!(result.get("prefix_abc"), Some(&Value::UInt(3)));
    assert_eq!(result.get("prefix_b"), Some(&Value::UInt(4)));
}

// ---------------------------------------------------------------------------
// Error handling tests
// ---------------------------------------------------------------------------

#[test]
fn test_crc_corruption_detected() {
    let mut data = HashMap::new();
    data.insert("test".to_string(), Value::UInt(42));
    let mut buf = encode(&data);
    // Flip a bit in the middle of the data
    buf[20] ^= 0x01;
    let result = decode(&buf);
    assert!(matches!(result, Err(triepack::TriePackError::Corrupt)));
}

#[test]
fn test_invalid_magic() {
    let mut buf = vec![0u8; 40];
    buf[0] = b'X';
    buf[1] = b'Y';
    buf[2] = b'Z';
    buf[3] = 0;
    let result = decode(&buf);
    assert!(matches!(result, Err(triepack::TriePackError::BadMagic)));
}

#[test]
fn test_truncated_buffer() {
    let buf = vec![0x54, 0x52, 0x50, 0x00, 1, 0]; // 6 bytes, too short
    let result = decode(&buf);
    assert!(matches!(result, Err(triepack::TriePackError::Truncated)));
}

#[test]
fn test_unsupported_version() {
    // Build a minimal valid-looking buffer with wrong version
    let mut buf = vec![0u8; 40];
    buf[0] = 0x54;
    buf[1] = 0x52;
    buf[2] = 0x50;
    buf[3] = 0x00;
    buf[4] = 2; // version_major = 2 (unsupported)
    buf[5] = 0;
    // The rest is zeros. CRC will fail before version check actually,
    // but let's compute a valid CRC for everything except last 4 bytes.
    let crc_data_len = buf.len() - 4;
    let crc = triepack::crc32::crc32(&buf[..crc_data_len]);
    buf[crc_data_len] = (crc >> 24) as u8;
    buf[crc_data_len + 1] = (crc >> 16) as u8;
    buf[crc_data_len + 2] = (crc >> 8) as u8;
    buf[crc_data_len + 3] = crc as u8;
    let result = decode(&buf);
    assert!(matches!(result, Err(triepack::TriePackError::Version)));
}

// ---------------------------------------------------------------------------
// Roundtrip determinism: encode twice, verify identical
// ---------------------------------------------------------------------------

#[test]
fn test_encode_deterministic() {
    let mut data = HashMap::new();
    data.insert("foo".to_string(), Value::UInt(1));
    data.insert("bar".to_string(), Value::UInt(2));
    data.insert("baz".to_string(), Value::UInt(3));

    let buf1 = encode(&data);
    let buf2 = encode(&data);
    assert_eq!(buf1, buf2, "Encoding should be deterministic");
}

#[test]
fn test_roundtrip_all_null_values() {
    let mut data = HashMap::new();
    data.insert("a".to_string(), Value::Null);
    data.insert("b".to_string(), Value::Null);
    data.insert("c".to_string(), Value::Null);
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result.get("a"), Some(&Value::Null));
    assert_eq!(result.get("b"), Some(&Value::Null));
    assert_eq!(result.get("c"), Some(&Value::Null));
}

#[test]
fn test_roundtrip_single_char_keys() {
    let mut data = HashMap::new();
    data.insert("a".to_string(), Value::UInt(1));
    data.insert("b".to_string(), Value::UInt(2));
    data.insert("z".to_string(), Value::UInt(26));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result.get("a"), Some(&Value::UInt(1)));
    assert_eq!(result.get("b"), Some(&Value::UInt(2)));
    assert_eq!(result.get("z"), Some(&Value::UInt(26)));
}

#[test]
fn test_roundtrip_long_string_value() {
    let mut data = HashMap::new();
    let long_str = "x".repeat(1000);
    data.insert("key".to_string(), Value::String(long_str.clone()));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.get("key"), Some(&Value::String(long_str)));
}

#[test]
fn test_roundtrip_empty_string_value() {
    let mut data = HashMap::new();
    data.insert("key".to_string(), Value::String(String::new()));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.get("key"), Some(&Value::String(String::new())));
}

#[test]
fn test_roundtrip_large_uint() {
    let mut data = HashMap::new();
    data.insert("max".to_string(), Value::UInt(u64::MAX));
    data.insert("large".to_string(), Value::UInt(0xFFFFFFFF));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.get("max"), Some(&Value::UInt(u64::MAX)));
    assert_eq!(result.get("large"), Some(&Value::UInt(0xFFFFFFFF)));
}

#[test]
fn test_roundtrip_negative_ints() {
    let mut data = HashMap::new();
    data.insert("min".to_string(), Value::Int(i64::MIN));
    data.insert("neg1".to_string(), Value::Int(-1));
    data.insert("neg100".to_string(), Value::Int(-100));
    let buf = encode(&data);
    let result = decode(&buf).unwrap();
    assert_eq!(result.get("min"), Some(&Value::Int(i64::MIN)));
    assert_eq!(result.get("neg1"), Some(&Value::Int(-1)));
    assert_eq!(result.get("neg100"), Some(&Value::Int(-100)));
}
