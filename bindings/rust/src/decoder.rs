// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Trie decoder -- port of src/core/decoder.c.

use std::collections::HashMap;

use crate::bitstream::BitReader;
use crate::crc32::crc32;
use crate::values::{decode_value, Value};
use crate::varint::read_var_uint;
use crate::TriePackError;

// Format constants
const TP_MAGIC: [u8; 4] = [0x54, 0x52, 0x50, 0x00];
const TP_HEADER_SIZE: usize = 32;
const TP_FLAG_HAS_VALUES: u16 = 1;
const NUM_CONTROL_CODES: usize = 6;

// Control code indices
const CTRL_END: usize = 0;
const CTRL_END_VAL: usize = 1;
#[allow(dead_code)]
const CTRL_SKIP: usize = 2;
const CTRL_BRANCH: usize = 5;

/// Decode a .trp binary buffer into a HashMap<String, Value>.
pub fn decode(buffer: &[u8]) -> Result<HashMap<String, Value>, TriePackError> {
    if buffer.len() < TP_HEADER_SIZE + 4 {
        return Err(TriePackError::Truncated);
    }

    // Validate magic
    if buffer[0..4] != TP_MAGIC {
        return Err(TriePackError::BadMagic);
    }

    // Version
    let version_major = buffer[4];
    if version_major != 1 {
        return Err(TriePackError::Version);
    }

    // Flags
    let flags = ((buffer[6] as u16) << 8) | buffer[7] as u16;
    let has_values = (flags & TP_FLAG_HAS_VALUES) != 0;

    // num_keys
    let num_keys = ((buffer[8] as u32) << 24)
        | ((buffer[9] as u32) << 16)
        | ((buffer[10] as u32) << 8)
        | buffer[11] as u32;

    // Offsets
    let trie_data_offset = ((buffer[12] as u32) << 24)
        | ((buffer[13] as u32) << 16)
        | ((buffer[14] as u32) << 8)
        | buffer[15] as u32;
    let value_store_offset = ((buffer[16] as u32) << 24)
        | ((buffer[17] as u32) << 16)
        | ((buffer[18] as u32) << 8)
        | buffer[19] as u32;

    // CRC verification
    let crc_data_len = buffer.len() - 4;
    let expected_crc = ((buffer[crc_data_len] as u32) << 24)
        | ((buffer[crc_data_len + 1] as u32) << 16)
        | ((buffer[crc_data_len + 2] as u32) << 8)
        | buffer[crc_data_len + 3] as u32;
    let actual_crc = crc32(&buffer[..crc_data_len]);
    if actual_crc != expected_crc {
        return Err(TriePackError::Corrupt);
    }

    if num_keys == 0 {
        return Ok(HashMap::new());
    }

    // Parse trie config
    let mut reader = BitReader::new(buffer);
    let data_start = TP_HEADER_SIZE * 8;

    reader.seek(data_start);
    let bps = reader.read_bits(4)? as usize;
    let symbol_count = reader.read_bits(8)? as usize;

    // Read control codes
    let mut ctrl_codes = [0u32; NUM_CONTROL_CODES];
    let mut code_is_ctrl = [false; 256];
    for c in 0..NUM_CONTROL_CODES {
        ctrl_codes[c] = reader.read_bits(bps)? as u32;
        if (ctrl_codes[c] as usize) < 256 {
            code_is_ctrl[ctrl_codes[c] as usize] = true;
        }
    }

    // Read symbol table
    let mut reverse_map = [0u8; 256];
    for cd in NUM_CONTROL_CODES..symbol_count {
        let cp = read_var_uint(&mut reader)? as usize;
        if cd < 256 && cp < 256 {
            reverse_map[cd] = cp as u8;
        }
    }

    let trie_start = data_start + trie_data_offset as usize;
    let value_start = data_start + value_store_offset as usize;

    // DFS iteration to collect all keys
    let mut result: HashMap<String, Value> = HashMap::new();
    let mut key_stack: Vec<u8> = Vec::new();

    reader.seek(trie_start);
    dfs_walk(
        &mut reader,
        bps,
        &ctrl_codes,
        &reverse_map,
        &mut key_stack,
        &mut result,
    )?;

    // Decode values
    if has_values {
        reader.seek(value_start);
        let mut sorted_keys: Vec<String> = result.keys().cloned().collect();
        sorted_keys.sort_by(|a, b| a.as_bytes().cmp(b.as_bytes()));
        for key in sorted_keys {
            let val = decode_value(&mut reader)?;
            result.insert(key, val);
        }
    }

    Ok(result)
}

/// DFS walk through the trie collecting keys.
fn dfs_walk(
    r: &mut BitReader,
    bps: usize,
    ctrl_codes: &[u32; NUM_CONTROL_CODES],
    reverse_map: &[u8; 256],
    key_stack: &mut Vec<u8>,
    result: &mut HashMap<String, Value>,
) -> Result<(), TriePackError> {
    loop {
        if r.remaining() < bps {
            return Ok(());
        }
        let sym = r.read_bits(bps)? as u32;

        if sym == ctrl_codes[CTRL_END] {
            // Terminal: key ends here with null value
            let key_str = String::from_utf8(key_stack.clone())
                .map_err(|_| TriePackError::InvalidUtf8)?;
            result.insert(key_str, Value::Null);

            // Check if a BRANCH follows (terminal with children)
            if r.remaining() >= bps {
                let next_sym = r.peek_bits(bps)? as u32;
                if next_sym == ctrl_codes[CTRL_BRANCH] {
                    r.read_bits(bps)?; // consume BRANCH
                    let child_count = read_var_uint(r)? as usize;
                    walk_branch(r, bps, ctrl_codes, reverse_map, key_stack, result, child_count)?;
                }
            }
            return Ok(());
        }

        if sym == ctrl_codes[CTRL_END_VAL] {
            // Terminal with value index
            read_var_uint(r)?; // value index (decoded later)
            let key_str = String::from_utf8(key_stack.clone())
                .map_err(|_| TriePackError::InvalidUtf8)?;
            result.insert(key_str, Value::Null); // placeholder, replaced during value decode

            // Check if a BRANCH follows
            if r.remaining() >= bps {
                let next_sym = r.peek_bits(bps)? as u32;
                if next_sym == ctrl_codes[CTRL_BRANCH] {
                    r.read_bits(bps)?; // consume BRANCH
                    let child_count = read_var_uint(r)? as usize;
                    walk_branch(r, bps, ctrl_codes, reverse_map, key_stack, result, child_count)?;
                }
            }
            return Ok(());
        }

        if sym == ctrl_codes[CTRL_BRANCH] {
            let child_count = read_var_uint(r)? as usize;
            walk_branch(r, bps, ctrl_codes, reverse_map, key_stack, result, child_count)?;
            return Ok(());
        }

        // Regular symbol
        let byte_val = if (sym as usize) < 256 {
            reverse_map[sym as usize]
        } else {
            0
        };
        key_stack.push(byte_val);
    }
}

/// Walk through a branch node's children.
fn walk_branch(
    r: &mut BitReader,
    bps: usize,
    ctrl_codes: &[u32; NUM_CONTROL_CODES],
    reverse_map: &[u8; 256],
    key_stack: &mut Vec<u8>,
    result: &mut HashMap<String, Value>,
    child_count: usize,
) -> Result<(), TriePackError> {
    let saved_key_len = key_stack.len();

    for ci in 0..child_count {
        let has_skip = ci < child_count - 1;
        let mut skip_dist: u64 = 0;

        if has_skip {
            r.read_bits(bps)?; // SKIP control code
            skip_dist = read_var_uint(r)?;
        }

        let child_start_pos = r.position();
        key_stack.truncate(saved_key_len);
        dfs_walk(r, bps, ctrl_codes, reverse_map, key_stack, result)?;

        if has_skip {
            r.seek(child_start_pos + skip_dist as usize);
        }
    }

    key_stack.truncate(saved_key_len);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::encoder;

    #[test]
    fn test_decode_empty() {
        let data = HashMap::new();
        let buf = encoder::encode(&data);
        let result = decode(&buf).unwrap();
        assert!(result.is_empty());
    }

    #[test]
    fn test_decode_single_null() {
        let mut data = HashMap::new();
        data.insert("hello".to_string(), Value::Null);
        let buf = encoder::encode(&data);
        let result = decode(&buf).unwrap();
        assert_eq!(result.len(), 1);
        assert_eq!(result.get("hello"), Some(&Value::Null));
    }

    #[test]
    fn test_decode_single_uint() {
        let mut data = HashMap::new();
        data.insert("key".to_string(), Value::UInt(42));
        let buf = encoder::encode(&data);
        let result = decode(&buf).unwrap();
        assert_eq!(result.len(), 1);
        assert_eq!(result.get("key"), Some(&Value::UInt(42)));
    }

    #[test]
    fn test_roundtrip_multi() {
        let mut data = HashMap::new();
        data.insert("alpha".to_string(), Value::String("hello".to_string()));
        data.insert("beta".to_string(), Value::UInt(42));
        data.insert("gamma".to_string(), Value::Bool(true));
        let buf = encoder::encode(&data);
        let result = decode(&buf).unwrap();
        assert_eq!(result.len(), 3);
        assert_eq!(
            result.get("alpha"),
            Some(&Value::String("hello".to_string()))
        );
        assert_eq!(result.get("beta"), Some(&Value::UInt(42)));
        assert_eq!(result.get("gamma"), Some(&Value::Bool(true)));
    }

    #[test]
    fn test_bad_magic() {
        let mut buf = vec![0u8; 40];
        buf[0] = 0xFF;
        let result = decode(&buf);
        assert!(matches!(result, Err(TriePackError::BadMagic)));
    }

    #[test]
    fn test_truncated() {
        let buf = vec![0x54, 0x52, 0x50, 0x00]; // too short
        let result = decode(&buf);
        assert!(matches!(result, Err(TriePackError::Truncated)));
    }

    #[test]
    fn test_corrupt_crc() {
        let mut data = HashMap::new();
        data.insert("test".to_string(), Value::Null);
        let mut buf = encoder::encode(&data);
        // Corrupt the CRC
        let last = buf.len() - 1;
        buf[last] ^= 0xFF;
        let result = decode(&buf);
        assert!(matches!(result, Err(TriePackError::Corrupt)));
    }
}
