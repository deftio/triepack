// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! The bitstream, varint and value primitives, and the decoder's behaviour on
//! a trie that has been corrupted underneath it.

use std::collections::HashMap;
use triepack::bitstream::{BitReader, BitWriter};
use triepack::crc32::crc32;
use triepack::values::{decode_value, encode_value};
use triepack::varint::{read_var_uint, write_var_uint};
use triepack::{decode, encode, TriePackError, Value};

// ---------------------------------------------------------------------------
// BitWriter
// ---------------------------------------------------------------------------

/// The buffer doubles until it fits. A write far past the current capacity
/// takes the loop more than once, which a single append never does.
#[test]
fn writer_grows_past_repeated_doubling() {
    // write_bytes goes a byte at a time, so each ensure() needs only one
    // doubling. A single 64-bit write against a 1-byte buffer needs three,
    // which is the only thing that takes the loop more than once.
    let mut w = BitWriter::new(1);
    w.write_bits(0xDEAD_BEEF_CAFE_F00D, 64);
    assert_eq!(w.position(), 64);
    assert_eq!(
        w.to_bytes()[..8],
        [0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xF0, 0x0D]
    );

    // And the ordinary incremental path still grows correctly.
    let mut w = BitWriter::new(1);
    let payload: Vec<u8> = (0..500u32).map(|i| (i % 251) as u8).collect();
    w.write_bytes(&payload);
    assert_eq!(&w.to_bytes()[..payload.len()], &payload[..]);
    assert_eq!(w.position(), payload.len() * 8);
}

/// The encoder patches header offsets in place after the fact.
#[test]
fn writer_exposes_its_buffer_for_patching() {
    let mut w = BitWriter::new(8);
    w.write_u32(0);
    w.buffer_mut()[0] = 0xAB;
    assert_eq!(w.to_bytes()[0], 0xAB);
}

// ---------------------------------------------------------------------------
// BitReader
// ---------------------------------------------------------------------------

#[test]
fn reader_reports_nothing_remaining_past_the_end() {
    let buf = [0xFFu8, 0x00];
    let mut r = BitReader::new(&buf);
    assert_eq!(r.remaining(), 16);

    r.seek(16);
    assert_eq!(r.remaining(), 0);

    // Seeking beyond the end saturates rather than underflowing.
    r.seek(64);
    assert_eq!(r.remaining(), 0);
}

#[test]
fn reader_advance_moves_the_cursor() {
    let buf = [0b1010_1010u8, 0b1100_1100];
    let mut r = BitReader::new(&buf);
    r.advance(8);
    assert_eq!(r.position(), 8);
    assert_eq!(r.read_bits(4).unwrap(), 0b1100);
}

/// A width outside 1..64 is a caller error, not an EOF.
#[test]
fn reader_rejects_an_impossible_width() {
    let buf = [0u8; 8];
    let mut r = BitReader::new(&buf);
    assert!(matches!(
        r.read_bits(0),
        Err(TriePackError::InvalidParam(_))
    ));
    assert!(matches!(
        r.read_bits(65),
        Err(TriePackError::InvalidParam(_))
    ));
    // The boundary itself still works.
    assert!(r.read_bits(64).is_ok());
}

// ---------------------------------------------------------------------------
// varint
// ---------------------------------------------------------------------------

/// A varint whose continuation bit never clears has to stop, not spin.
#[test]
fn var_uint_refuses_an_unterminated_run() {
    let mut w = BitWriter::new(32);
    for _ in 0..12 {
        w.write_u8(0xFF); // continuation set, forever
    }
    let bytes = w.to_bytes();
    let mut r = BitReader::new(&bytes);
    assert!(matches!(read_var_uint(&mut r), Err(TriePackError::Overflow)));
}

#[test]
fn var_uint_round_trips_at_the_boundaries() {
    for v in [0u64, 1, 127, 128, u64::MAX] {
        let mut w = BitWriter::new(16);
        write_var_uint(&mut w, v);
        let bytes = w.to_bytes();
        let mut r = BitReader::new(&bytes);
        assert_eq!(read_var_uint(&mut r).unwrap(), v, "round-trip of {}", v);
    }
}

// ---------------------------------------------------------------------------
// values
// ---------------------------------------------------------------------------

/// A tag no writer produces has to be rejected rather than guessed at.
#[test]
fn unknown_value_tag_is_rejected() {
    let mut w = BitWriter::new(8);
    // Tags are 4 bits and the format defines 0..7; 15 is not one of them.
    w.write_bits(0xF, 4);
    w.write_bits(0, 4);
    let bytes = w.to_bytes();
    let mut r = BitReader::new(&bytes);
    assert!(matches!(
        decode_value(&mut r),
        Err(TriePackError::InvalidParam(_))
    ));
}

#[test]
fn every_value_type_round_trips_through_the_primitives() {
    let cases = vec![
        Value::Null,
        Value::Bool(true),
        Value::Bool(false),
        Value::Int(-1),
        Value::UInt(u64::MAX),
        Value::Float64(-0.0),
        Value::String("héllo".to_string()),
        Value::Blob(vec![0, 1, 255]),
    ];
    for val in cases {
        let mut w = BitWriter::new(32);
        encode_value(&mut w, &val);
        let bytes = w.to_bytes();
        let mut r = BitReader::new(&bytes);
        assert_eq!(decode_value(&mut r).unwrap(), val);
    }
}

// ---------------------------------------------------------------------------
// Trie shapes and corruption
// ---------------------------------------------------------------------------

/// A key that is both a stored value and the road to several others: the
/// size pass has to account for the SKIP before every child but the last.
#[test]
fn a_stored_key_with_several_children_round_trips() {
    let mut data = HashMap::new();
    // "hel" is a stored key with two children, and it sits under "he", which
    // is itself a stored key with children. The size pass therefore has to
    // handle a terminal-with-children that is not the root.
    for (k, v) in [
        ("he", 1i64),
        ("hel", 2),
        ("hello", 3),
        ("help", 4),
        ("hen", 5),
        ("here", 6),
        ("zebra", 7),
    ] {
        data.insert(k.to_string(), Value::Int(v));
    }
    assert_eq!(decode(&encode(&data)).unwrap(), data);
}

fn repair_crc(buf: &mut [u8]) {
    let n = buf.len();
    let crc = crc32(&buf[..n - 4]);
    buf[n - 4..].copy_from_slice(&crc.to_be_bytes());
}

/// Every single-bit corruption of the body must end in a value or an error —
/// never a panic, a hang, or a read past the buffer. The CRC is repaired each
/// time so the trie walk is what has to catch the damage.
#[test]
fn every_single_bit_corruption_terminates() {
    let mut data = HashMap::new();
    for (k, v) in [("he", 1i64), ("hello", 2), ("help", 3), ("zebra", 4)] {
        data.insert(k.to_string(), Value::Int(v));
    }
    let clean = encode(&data);

    let mut decoded_ok = 0usize;
    let mut saw_branch_error = false;

    for byte in 4..clean.len() - 4 {
        for bit in 0..8 {
            let mut trial = clean.clone();
            trial[byte] ^= 1 << bit;
            repair_crc(&mut trial);

            match decode(&trial) {
                Ok(_) => decoded_ok += 1,
                Err(TriePackError::InvalidParam(msg)) => {
                    if msg.contains("expected BRANCH after terminal") {
                        saw_branch_error = true;
                    }
                }
                Err(_) => {}
            }
        }
    }

    assert!(decoded_ok > 0, "the sweep was rejected at the door every time");
    assert!(
        saw_branch_error,
        "no corruption reached the BRANCH-after-terminal check"
    );
}

/// Truncating the body leaves the walk reading off the end of the trie.
#[test]
fn truncation_is_caught_at_every_length() {
    let mut data = HashMap::new();
    for (k, v) in [("alpha", 1i64), ("alphabet", 2), ("beta", 3)] {
        data.insert(k.to_string(), Value::Int(v));
    }
    let clean = encode(&data);

    for cut in 40..clean.len() {
        let mut trial = clean[..cut].to_vec();
        repair_crc(&mut trial);
        // Either it decodes something finite or it errors; neither may panic.
        let _ = decode(&trial);
    }
}

// ---------------------------------------------------------------------------
// Reads that run out of input
// ---------------------------------------------------------------------------

/// Zero means "pick a default", not "allocate nothing".
#[test]
fn writer_treats_zero_capacity_as_a_default() {
    let mut w = BitWriter::new(0);
    w.write_u8(0xAB);
    assert_eq!(w.to_bytes()[0], 0xAB);
}

/// Each width has its own read path, and each has to report EOF rather than
/// return a value assembled from bits that are not there.
#[test]
fn wide_reads_stop_at_the_end_of_the_buffer() {
    let one = [0xFFu8];
    assert!(matches!(
        BitReader::new(&one).read_u16(),
        Err(TriePackError::Eof)
    ));

    let three = [0xFFu8; 3];
    assert!(matches!(
        BitReader::new(&three).read_u32(),
        Err(TriePackError::Eof)
    ));

    // read_u64 reads two u32s; failing on the second is its own path.
    let seven = [0xFFu8; 7];
    assert!(matches!(
        BitReader::new(&seven).read_u64(),
        Err(TriePackError::Eof)
    ));
    let three_again = [0xFFu8; 3];
    assert!(matches!(
        BitReader::new(&three_again).read_u64(),
        Err(TriePackError::Eof)
    ));
}

/// A value whose tag promises a payload the buffer does not contain has to
/// fail on that payload, not return a half-built value.
///
/// A reader is byte-granular, so a 4-bit tag written on its own leaves four
/// spare bits that a 1-bit payload would happily consume. Writing four bits
/// of padding first puts the tag at the very end of the buffer, so the
/// payload read starts exactly at EOF whatever its width.
#[test]
fn a_value_truncated_after_its_tag_is_rejected() {
    for tag in [1u64 /* Bool */, 2, 3, 4, 5, 6, 7] {
        let mut w = BitWriter::new(2);
        w.write_bits(0, 4); // padding, so the tag lands in the low nibble
        w.write_bits(tag, 4);
        let bytes = w.to_bytes();

        let mut r = BitReader::new(&bytes[..1]);
        r.seek(4);
        assert!(
            decode_value(&mut r).is_err(),
            "tag {} with no payload should not decode",
            tag
        );
    }
}

/// Float64 reads two 32-bit halves. Supplying the first and not the second
/// is a different path from supplying neither.
#[test]
fn a_float64_missing_its_second_half_is_rejected() {
    let mut w = BitWriter::new(8);
    w.write_bits(0, 4);
    w.write_bits(5, 4); // Float64
    w.write_u32(0xDEAD_BEEF); // high word only
    let bytes = w.to_bytes();

    let mut r = BitReader::new(&bytes[..5]);
    r.seek(4);
    assert!(matches!(decode_value(&mut r), Err(TriePackError::Eof)));
}

/// A string or blob whose declared length runs past the buffer.
#[test]
fn a_value_claiming_more_bytes_than_exist_is_rejected() {
    for tag in [6u64, 7] {
        let mut w = BitWriter::new(8);
        w.write_bits(tag, 4);
        write_var_uint(&mut w, 64); // claims 64 bytes
        w.align_to_byte();
        w.write_bytes(&[1, 2, 3]); // supplies 3
        let bytes = w.to_bytes();
        let mut r = BitReader::new(&bytes);
        assert!(matches!(decode_value(&mut r), Err(TriePackError::Eof)));
    }
}

/// Truncation at every length, starting from the shortest buffer decode will
/// even look at. The early cuts leave the trie config itself unreadable.
#[test]
fn truncation_is_caught_from_the_header_onward() {
    let mut data = HashMap::new();
    for (k, v) in [("alpha", 1i64), ("alphabet", 2), ("beta", 3), ("b", 4)] {
        data.insert(k.to_string(), Value::Int(v));
    }
    let clean = encode(&data);

    for cut in 36..clean.len() {
        let mut trial = clean[..cut].to_vec();
        repair_crc(&mut trial);
        // Either it decodes something finite or it errors; neither may panic.
        let _ = decode(&trial);
    }
}

/// Keys and string values are decoded as UTF-8. A corruption that rewires the
/// symbol table can turn a valid key into bytes that are not, and that has to
/// be reported rather than papered over with replacement characters.
#[test]
fn corruption_that_breaks_utf8_is_reported() {
    let mut data = HashMap::new();
    data.insert("héllo".to_string(), Value::String("wörld".to_string()));
    data.insert("naïve".to_string(), Value::String("çedilla".to_string()));
    data.insert("ascii".to_string(), Value::Int(1));
    let clean = encode(&data);

    let mut saw_invalid_utf8 = false;
    for byte in 4..clean.len() - 4 {
        for bit in 0..8 {
            let mut trial = clean.clone();
            trial[byte] ^= 1 << bit;
            repair_crc(&mut trial);
            if let Err(TriePackError::InvalidUtf8) = decode(&trial) {
                saw_invalid_utf8 = true;
            }
        }
    }

    assert!(
        saw_invalid_utf8,
        "no corruption produced a key or value that failed UTF-8 validation"
    );
}

/// The trie's end comes from the header, not from the buffer, so a header
/// claiming the value store starts past the end leaves the walk reading off
/// the edge. It has to stop at EOF rather than index out of bounds.
#[test]
fn a_header_pointing_past_the_buffer_is_caught() {
    let mut data = HashMap::new();
    data.insert("alpha".to_string(), Value::Int(1));
    data.insert("alphabet".to_string(), Value::Int(2));
    let clean = encode(&data);

    // value_store_offset is the u32 at bytes 16..20, big-endian.
    for inflate in [0x0000_1000u32, 0x0100_0000, 0xFFFF_FFFF] {
        let mut trial = clean.clone();
        trial[16..20].copy_from_slice(&inflate.to_be_bytes());
        repair_crc(&mut trial);
        // Must not panic; an error or a truncated result are both acceptable.
        let _ = decode(&trial);
    }

    // Same for trie_data_offset at bytes 12..16.
    for inflate in [0x0000_1000u32, 0xFFFF_FFFF] {
        let mut trial = clean.clone();
        trial[12..16].copy_from_slice(&inflate.to_be_bytes());
        repair_crc(&mut trial);
        let _ = decode(&trial);
    }
}
