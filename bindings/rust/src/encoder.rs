// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Trie encoder -- port of src/core/encoder.c.

use std::collections::HashMap;

use crate::bitstream::BitWriter;
use crate::crc32::crc32;
use crate::values::{encode_value, Value};
use crate::varint::{var_uint_bits, write_var_uint};

// Format constants (match core_internal.h)
const TP_MAGIC: [u8; 4] = [0x54, 0x52, 0x50, 0x00]; // "TRP\0"
const TP_VERSION_MAJOR: u8 = 1;
const TP_VERSION_MINOR: u8 = 0;
const TP_FLAG_HAS_VALUES: u16 = 1;

// Control code indices
const CTRL_END: usize = 0;
const CTRL_END_VAL: usize = 1;
const CTRL_SKIP: usize = 2;
const CTRL_BRANCH: usize = 5;
const NUM_CONTROL_CODES: usize = 6;

/// Internal entry: sorted key bytes + value reference index.
struct Entry {
    key: Vec<u8>,
    val_idx: usize, // index into the original values vec
}

/// Encoder context passed through recursive trie writing.
struct EncCtx {
    entries: Vec<Entry>,
    values: Vec<Value>,
    bps: usize,
    symbol_map: [u32; 256],
    ctrl_codes: [u32; NUM_CONTROL_CODES],
    has_values: bool,
}

/// Encode a HashMap<String, Value> into the .trp binary format.
///
/// Keys are sorted by their UTF-8 byte representation. Duplicate keys are
/// deduplicated (last value wins, but since HashMap has unique keys this is
/// a no-op). The output is byte-identical to the C reference encoder.
pub fn encode(data: &HashMap<String, Value>) -> Vec<u8> {
    // Collect entries as (key_bytes, value), sort by key_bytes
    let mut pairs: Vec<(Vec<u8>, &Value)> = data
        .iter()
        .map(|(k, v)| (k.as_bytes().to_vec(), v))
        .collect();
    pairs.sort_by(|a, b| a.0.cmp(&b.0));

    // Build owned values list and entry list
    let mut values: Vec<Value> = Vec::with_capacity(pairs.len());
    let mut entries: Vec<Entry> = Vec::with_capacity(pairs.len());
    for (i, (key, val)) in pairs.iter().enumerate() {
        values.push((*val).clone());
        entries.push(Entry {
            key: key.clone(),
            val_idx: i,
        });
    }

    // Determine if any entries have non-null values
    let has_values = values.iter().any(|v| !matches!(v, Value::Null));

    // Symbol analysis: find unique bytes across all keys
    let mut used = [false; 256];
    for e in &entries {
        for &b in &e.key {
            used[b as usize] = true;
        }
    }

    let alphabet_size: usize = used.iter().filter(|&&u| u).count();
    let total_symbols = alphabet_size + NUM_CONTROL_CODES;

    // Determine bits_per_symbol
    let mut bps: usize = 1;
    while (1usize << bps) < total_symbols {
        bps += 1;
    }

    // Control codes: 0..5
    let mut ctrl_codes = [0u32; NUM_CONTROL_CODES];
    for c in 0..NUM_CONTROL_CODES {
        ctrl_codes[c] = c as u32;
    }

    // Symbol map: byte value -> N-bit code
    let mut symbol_map = [0u32; 256];
    let mut reverse_map = [0u8; 256];
    let mut code = NUM_CONTROL_CODES as u32;
    for i in 0..256 {
        if used[i] {
            symbol_map[i] = code;
            if (code as usize) < 256 {
                reverse_map[code as usize] = i as u8;
            }
            code += 1;
        }
    }

    let ctx = EncCtx {
        entries,
        values,
        bps,
        symbol_map,
        ctrl_codes,
        has_values,
    };

    // Build the bitstream
    let mut w = BitWriter::new(256);

    // Write 32-byte header placeholder
    for &b in &TP_MAGIC {
        w.write_u8(b);
    }
    w.write_u8(TP_VERSION_MAJOR);
    w.write_u8(TP_VERSION_MINOR);
    let flags: u16 = if has_values { TP_FLAG_HAS_VALUES } else { 0 };
    w.write_u16(flags);
    w.write_u32(ctx.entries.len() as u32); // num_keys
    w.write_u32(0); // trie_data_offset placeholder
    w.write_u32(0); // value_store_offset placeholder
    w.write_u32(0); // suffix_table_offset placeholder
    w.write_u32(0); // total_data_bits placeholder
    w.write_u32(0); // reserved

    let data_start = w.position(); // should be 256 (32 bytes * 8)

    // Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
    w.write_bits(bps as u64, 4);
    w.write_bits(total_symbols as u64, 8);

    // Control code mappings
    for c in 0..NUM_CONTROL_CODES {
        w.write_bits(ctrl_codes[c] as u64, bps);
    }

    // Symbol table: for each code from NUM_CONTROL_CODES..total_symbols,
    // write the byte value it maps to as a VarInt.
    for cd in NUM_CONTROL_CODES..total_symbols {
        let byte_val = if cd < 256 { reverse_map[cd] } else { 0 };
        write_var_uint(&mut w, byte_val as u64);
    }

    let trie_data_offset = (w.position() - data_start) as u32;

    // Write prefix trie
    let mut value_idx: u32 = 0;
    let num_entries = ctx.entries.len() as u32;
    if num_entries > 0 {
        trie_write(&ctx, &mut w, 0, num_entries, 0, &mut value_idx);
    }

    let value_store_offset = (w.position() - data_start) as u32;

    // Write value store
    if has_values {
        for val in &ctx.values {
            encode_value(&mut w, val);
        }
    }

    let total_data_bits = (w.position() - data_start) as u32;

    w.align_to_byte();

    // Compute CRC-32 over everything written so far
    let pre_crc_buf = w.to_bytes();
    let crc_val = crc32(&pre_crc_buf);

    // Append CRC
    w.write_u32(crc_val);

    let mut out_buf = w.to_bytes();

    // Patch header offsets (big-endian u32)
    patch_u32(&mut out_buf, 12, trie_data_offset);
    patch_u32(&mut out_buf, 16, value_store_offset);
    patch_u32(&mut out_buf, 20, 0); // suffix_table_offset
    patch_u32(&mut out_buf, 24, total_data_bits);

    // Recompute CRC over patched data (everything except last 4 bytes)
    let crc_data_len = out_buf.len() - 4;
    let crc_val = crc32(&out_buf[..crc_data_len]);
    patch_u32(&mut out_buf, crc_data_len, crc_val);

    out_buf
}

/// Patch a big-endian u32 at the given byte offset.
fn patch_u32(buf: &mut [u8], offset: usize, value: u32) {
    buf[offset] = (value >> 24) as u8;
    buf[offset + 1] = (value >> 16) as u8;
    buf[offset + 2] = (value >> 8) as u8;
    buf[offset + 3] = value as u8;
}

/// Compute the number of bits needed to encode the trie subtree for
/// entries[start..end) given that `prefix_len` characters have already been
/// consumed. This is the "dry run" pass used to compute SKIP distances.
fn trie_subtree_size(
    ctx: &EncCtx,
    start: u32,
    end: u32,
    prefix_len: usize,
    value_idx: &mut u32,
) -> u64 {
    let bps = ctx.bps;
    let mut bits: u64 = 0;

    // Find common prefix beyond prefix_len
    let common = find_common_prefix(ctx, start, end, prefix_len);

    // Emit symbols for the common prefix part
    bits += ((common - prefix_len) as u64) * (bps as u64);

    // Check if any entry terminates exactly at 'common'
    let has_terminal = ctx.entries[start as usize].key.len() == common;
    let children_start = if has_terminal { start + 1 } else { start };

    // Count children
    let child_ranges = get_child_ranges(ctx, children_start, end, common);
    let child_count = child_ranges.len();

    if has_terminal && child_count == 0 {
        // Leaf node
        if ctx.has_values && !matches!(ctx.values[ctx.entries[start as usize].val_idx], Value::Null)
        {
            bits += bps as u64; // END_VAL
            bits += var_uint_bits(*value_idx as u64);
        } else {
            bits += bps as u64; // END
        }
        *value_idx += 1;
    } else if has_terminal {
        // Terminal + children
        if ctx.has_values && !matches!(ctx.values[ctx.entries[start as usize].val_idx], Value::Null)
        {
            bits += bps as u64; // END_VAL
            bits += var_uint_bits(*value_idx as u64);
        } else {
            bits += bps as u64; // END
        }
        *value_idx += 1;

        bits += bps as u64; // BRANCH
        bits += var_uint_bits(child_count as u64);

        for (ci, &(cs, ce)) in child_ranges.iter().enumerate() {
            if ci < child_count - 1 {
                let child_sz = trie_subtree_size(ctx, cs, ce, common, value_idx);
                bits += bps as u64; // SKIP
                bits += var_uint_bits(child_sz);
                bits += child_sz;
            } else {
                bits += trie_subtree_size(ctx, cs, ce, common, value_idx);
            }
        }
    } else {
        // No terminal, just branch
        bits += bps as u64; // BRANCH
        bits += var_uint_bits(child_count as u64);

        for (ci, &(cs, ce)) in child_ranges.iter().enumerate() {
            if ci < child_count - 1 {
                let child_sz = trie_subtree_size(ctx, cs, ce, common, value_idx);
                bits += bps as u64; // SKIP
                bits += var_uint_bits(child_sz);
                bits += child_sz;
            } else {
                bits += trie_subtree_size(ctx, cs, ce, common, value_idx);
            }
        }
    }

    bits
}

/// Write the trie subtree for entries[start..end).
fn trie_write(
    ctx: &EncCtx,
    w: &mut BitWriter,
    start: u32,
    end: u32,
    prefix_len: usize,
    value_idx: &mut u32,
) {
    let bps = ctx.bps;

    // Find common prefix beyond prefix_len
    let common = find_common_prefix(ctx, start, end, prefix_len);

    // Write common prefix symbols
    for i in prefix_len..common {
        let ch = ctx.entries[start as usize].key[i];
        let code = ctx.symbol_map[ch as usize];
        w.write_bits(code as u64, bps);
    }

    // Check if any entry terminates exactly at 'common'
    let has_terminal = ctx.entries[start as usize].key.len() == common;
    let children_start = if has_terminal { start + 1 } else { start };

    // Count and locate children
    let child_ranges = get_child_ranges(ctx, children_start, end, common);
    let child_count = child_ranges.len();

    // Write terminal if present
    if has_terminal {
        if ctx.has_values && !matches!(ctx.values[ctx.entries[start as usize].val_idx], Value::Null)
        {
            w.write_bits(ctx.ctrl_codes[CTRL_END_VAL] as u64, bps);
            write_var_uint(w, *value_idx as u64);
        } else {
            w.write_bits(ctx.ctrl_codes[CTRL_END] as u64, bps);
        }
        *value_idx += 1;
    }

    if child_count == 0 {
        return;
    }

    // BRANCH
    w.write_bits(ctx.ctrl_codes[CTRL_BRANCH] as u64, bps);
    write_var_uint(w, child_count as u64);

    // For each child: optionally write SKIP, then recurse
    for (ci, &(cs, ce)) in child_ranges.iter().enumerate() {
        if ci < child_count - 1 {
            let mut vi_copy = *value_idx;
            let child_sz = trie_subtree_size(ctx, cs, ce, common, &mut vi_copy);
            w.write_bits(ctx.ctrl_codes[CTRL_SKIP] as u64, bps);
            write_var_uint(w, child_sz);
        }

        trie_write(ctx, w, cs, ce, common, value_idx);
    }
}

/// Find the common prefix length beyond `prefix_len` for entries[start..end).
fn find_common_prefix(ctx: &EncCtx, start: u32, end: u32, prefix_len: usize) -> usize {
    let mut common = prefix_len;
    loop {
        let mut all_have = true;
        let mut ch: u8 = 0;
        for i in start..end {
            let key = &ctx.entries[i as usize].key;
            if key.len() <= common {
                all_have = false;
                break;
            }
            if i == start {
                ch = key[common];
            } else if key[common] != ch {
                all_have = false;
                break;
            }
        }
        if !all_have {
            break;
        }
        common += 1;
    }
    common
}

/// Get the child ranges for entries[children_start..end) grouped by the byte
/// at position `common`. Returns a Vec of (start, end) pairs.
fn get_child_ranges(ctx: &EncCtx, children_start: u32, end: u32, common: usize) -> Vec<(u32, u32)> {
    let mut ranges = Vec::new();
    let mut cs = children_start;
    while cs < end {
        let ch = ctx.entries[cs as usize].key[common];
        let mut ce = cs + 1;
        while ce < end && ctx.entries[ce as usize].key[common] == ch {
            ce += 1;
        }
        ranges.push((cs, ce));
        cs = ce;
    }
    ranges
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_empty() {
        let data = HashMap::new();
        let buf = encode(&data);
        // Should be 40 bytes: 32-byte header + 4 bytes trie config + 4 bytes CRC
        // Actually the empty dict has a specific trie config section
        assert!(buf.len() >= 36);
        // Check magic
        assert_eq!(&buf[0..4], &TP_MAGIC);
        // Check version
        assert_eq!(buf[4], TP_VERSION_MAJOR);
        assert_eq!(buf[5], TP_VERSION_MINOR);
        // Check num_keys = 0
        assert_eq!(&buf[8..12], &[0, 0, 0, 0]);
    }

    #[test]
    fn test_encode_single_null() {
        let mut data = HashMap::new();
        data.insert("hello".to_string(), Value::Null);
        let buf = encode(&data);
        assert_eq!(&buf[0..4], &TP_MAGIC);
        // num_keys = 1
        assert_eq!(&buf[8..12], &[0, 0, 0, 1]);
        // flags: no values (all null)
        assert_eq!(&buf[6..8], &[0, 0]);
    }

    #[test]
    fn test_encode_single_int() {
        let mut data = HashMap::new();
        data.insert("key".to_string(), Value::UInt(42));
        let buf = encode(&data);
        assert_eq!(&buf[0..4], &TP_MAGIC);
        // num_keys = 1
        assert_eq!(&buf[8..12], &[0, 0, 0, 1]);
        // flags: has_values
        let flags = ((buf[6] as u16) << 8) | buf[7] as u16;
        assert_ne!(flags & TP_FLAG_HAS_VALUES, 0);
    }
}
