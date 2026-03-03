# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Trie encoder — port of src/core/encoder.c (via JavaScript encoder.js)."""

from .bitstream import BitWriter
from .crc32 import crc32
from .values import encode_value
from .varint import var_uint_bits, write_var_uint

# Format constants (match core_internal.h)
TP_MAGIC = bytes([0x54, 0x52, 0x50, 0x00])  # "TRP\0"
TP_VERSION_MAJOR = 1
TP_VERSION_MINOR = 0
TP_HEADER_SIZE = 32
TP_FLAG_HAS_VALUES = 1

# Control code indices
CTRL_END = 0
CTRL_END_VAL = 1
CTRL_SKIP = 2
# CTRL_SUFFIX = 3
# CTRL_ESCAPE = 4
CTRL_BRANCH = 5
NUM_CONTROL_CODES = 6


def encode(data):
    """Encode a dict into the .trp binary format. Returns bytes."""
    if not isinstance(data, dict):
        raise TypeError("encode expects a dict")

    # Collect entries: (key_bytes, val)
    entries = []
    for k, v in data.items():
        key_bytes = k.encode("utf-8") if isinstance(k, str) else bytes(k)
        entries.append((key_bytes, v))

    # Sort lexicographically by key bytes
    entries.sort(key=lambda e: e[0])

    # Dedup (keep last)
    deduped = []
    for i, entry in enumerate(entries):
        if i + 1 < len(entries) and entries[i][0] == entries[i + 1][0]:
            continue
        deduped.append(entry)

    # Determine if any entries have non-null values
    has_values = any(e[1] is not None for e in deduped)

    # Symbol analysis
    used = [False] * 256
    for kb, _ in deduped:
        for b in kb:
            used[b] = True

    alphabet_size = sum(used)
    total_symbols = alphabet_size + NUM_CONTROL_CODES
    bps = 1
    while (1 << bps) < total_symbols:
        bps += 1

    # Control codes: 0..5
    ctrl_codes = list(range(NUM_CONTROL_CODES))

    # Symbol map: byte value -> N-bit code
    symbol_map = [0] * 256
    reverse_map = [0] * 256
    code = NUM_CONTROL_CODES
    for i in range(256):
        if used[i]:
            symbol_map[i] = code
            if code < 256:
                reverse_map[code] = i
            code += 1

    # Build the bitstream
    w = BitWriter(256)

    # Write 32-byte header placeholder
    for b in TP_MAGIC:
        w.write_u8(b)
    w.write_u8(TP_VERSION_MAJOR)
    w.write_u8(TP_VERSION_MINOR)
    flags = TP_FLAG_HAS_VALUES if has_values else 0
    w.write_u16(flags)
    w.write_u32(len(deduped))  # num_keys
    w.write_u32(0)  # trie_data_offset placeholder
    w.write_u32(0)  # value_store_offset placeholder
    w.write_u32(0)  # suffix_table_offset placeholder
    w.write_u32(0)  # total_data_bits placeholder
    w.write_u32(0)  # reserved

    data_start = w.position  # should be 256 (32 bytes * 8)

    # Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
    w.write_bits(bps, 4)
    w.write_bits(total_symbols, 8)

    # Control code mappings
    for c in range(NUM_CONTROL_CODES):
        w.write_bits(ctrl_codes[c], bps)

    # Symbol table
    for cd in range(NUM_CONTROL_CODES, total_symbols):
        byte_val = reverse_map[cd] if cd < 256 else 0
        write_var_uint(w, byte_val)

    trie_data_offset = w.position - data_start

    # Write prefix trie
    ctx = {
        "deduped": deduped,
        "bps": bps,
        "symbol_map": symbol_map,
        "ctrl_codes": ctrl_codes,
        "has_values": has_values,
    }
    value_idx = [0]
    if deduped:
        _trie_write(ctx, w, 0, len(deduped), 0, value_idx)

    value_store_offset = w.position - data_start

    # Write value store
    if has_values:
        for _, val in deduped:
            encode_value(w, val)

    total_data_bits = w.position - data_start

    w.align_to_byte()

    # Get pre-CRC buffer
    pre_crc_buf = bytearray(w.to_bytes())
    crc_val = crc32(pre_crc_buf)

    # Append CRC
    w.write_u32(crc_val)

    out_buf = bytearray(w.to_bytes())

    # Patch header offsets
    _patch_u32(out_buf, 12, trie_data_offset)
    _patch_u32(out_buf, 16, value_store_offset)
    _patch_u32(out_buf, 20, 0)  # suffix_table_offset
    _patch_u32(out_buf, 24, total_data_bits)

    # Recompute CRC over patched data
    crc_data_len = len(out_buf) - 4
    crc_val = crc32(out_buf[:crc_data_len])
    _patch_u32(out_buf, crc_data_len, crc_val)

    return bytes(out_buf)


def _patch_u32(buf, offset, value):
    buf[offset] = (value >> 24) & 0xFF
    buf[offset + 1] = (value >> 16) & 0xFF
    buf[offset + 2] = (value >> 8) & 0xFF
    buf[offset + 3] = value & 0xFF


def _trie_subtree_size(ctx, start, end, prefix_len, value_idx):
    """Compute subtree size in bits (dry run)."""
    if start >= end:
        return 0

    deduped = ctx["deduped"]
    bps = ctx["bps"]
    has_values = ctx["has_values"]
    bits = 0

    # Find common prefix beyond prefix_len
    common = prefix_len
    while True:
        all_have = True
        ch = 0
        for i in range(start, end):
            if len(deduped[i][0]) <= common:
                all_have = False
                break
            if i == start:
                ch = deduped[i][0][common]
            elif deduped[i][0][common] != ch:
                all_have = False
                break
        if not all_have:
            break
        common += 1

    bits += (common - prefix_len) * bps

    has_terminal = len(deduped[start][0]) == common
    children_start = start + 1 if has_terminal else start

    # Count children
    child_count = 0
    cs = children_start
    while cs < end:
        child_count += 1
        ch = deduped[cs][0][common]
        ce = cs + 1
        while ce < end and deduped[ce][0][common] == ch:
            ce += 1
        cs = ce

    if has_terminal and child_count == 0:
        if has_values and deduped[start][1] is not None:
            bits += bps
            bits += var_uint_bits(value_idx[0])
        else:
            bits += bps
        value_idx[0] += 1
    elif has_terminal and child_count > 0:
        if has_values and deduped[start][1] is not None:
            bits += bps
            bits += var_uint_bits(value_idx[0])
        else:
            bits += bps
        value_idx[0] += 1
        bits += bps
        bits += var_uint_bits(child_count)

        cs = children_start
        child_i = 0
        while cs < end:
            ch = deduped[cs][0][common]
            ce = cs + 1
            while ce < end and deduped[ce][0][common] == ch:
                ce += 1
            if child_i < child_count - 1:
                child_sz = _trie_subtree_size(ctx, cs, ce, common, value_idx)
                bits += bps
                bits += var_uint_bits(child_sz)
                bits += child_sz
            else:
                bits += _trie_subtree_size(ctx, cs, ce, common, value_idx)
            child_i += 1
            cs = ce
    elif not has_terminal and child_count == 1:
        bits += _trie_subtree_size(ctx, children_start, end, common + 1, value_idx)
    elif not has_terminal and child_count > 1:
        bits += bps
        bits += var_uint_bits(child_count)

        cs = children_start
        child_i = 0
        while cs < end:
            ch = deduped[cs][0][common]
            ce = cs + 1
            while ce < end and deduped[ce][0][common] == ch:
                ce += 1
            if child_i < child_count - 1:
                child_sz = _trie_subtree_size(ctx, cs, ce, common, value_idx)
                bits += bps
                bits += var_uint_bits(child_sz)
                bits += child_sz
            else:
                bits += _trie_subtree_size(ctx, cs, ce, common, value_idx)
            child_i += 1
            cs = ce

    return bits


def _trie_write(ctx, w, start, end, prefix_len, value_idx):
    """Write the trie subtree."""
    if start >= end:
        return

    deduped = ctx["deduped"]
    bps = ctx["bps"]
    symbol_map = ctx["symbol_map"]
    ctrl_codes = ctx["ctrl_codes"]
    has_values = ctx["has_values"]

    # Find common prefix beyond prefix_len
    common = prefix_len
    while True:
        all_have = True
        ch = 0
        for i in range(start, end):
            if len(deduped[i][0]) <= common:
                all_have = False
                break
            if i == start:
                ch = deduped[i][0][common]
            elif deduped[i][0][common] != ch:
                all_have = False
                break
        if not all_have:
            break
        common += 1

    # Write common prefix symbols
    for i in range(prefix_len, common):
        ch = deduped[start][0][i]
        w.write_bits(symbol_map[ch], bps)

    has_terminal = len(deduped[start][0]) == common
    children_start = start + 1 if has_terminal else start

    # Count children
    child_count = 0
    cs = children_start
    while cs < end:
        child_count += 1
        ch = deduped[cs][0][common]
        ce = cs + 1
        while ce < end and deduped[ce][0][common] == ch:
            ce += 1
        cs = ce

    # Write terminal
    if has_terminal:
        if has_values and deduped[start][1] is not None:
            w.write_bits(ctrl_codes[CTRL_END_VAL], bps)
            write_var_uint(w, value_idx[0])
        else:
            w.write_bits(ctrl_codes[CTRL_END], bps)
        value_idx[0] += 1

    if child_count == 0:
        return
    elif child_count == 1 and not has_terminal:
        _trie_write(ctx, w, children_start, end, common + 1, value_idx)
        return

    # BRANCH
    w.write_bits(ctrl_codes[CTRL_BRANCH], bps)
    write_var_uint(w, child_count)

    cs = children_start
    child_i = 0
    while cs < end:
        ch = deduped[cs][0][common]
        ce = cs + 1
        while ce < end and deduped[ce][0][common] == ch:
            ce += 1

        if child_i < child_count - 1:
            vi_copy = [value_idx[0]]
            child_sz = _trie_subtree_size(ctx, cs, ce, common, vi_copy)
            w.write_bits(ctrl_codes[CTRL_SKIP], bps)
            write_var_uint(w, child_sz)

        _trie_write(ctx, w, cs, ce, common, value_idx)

        child_i += 1
        cs = ce
