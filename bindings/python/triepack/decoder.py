# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Trie decoder — port of src/core/decoder.c (via JavaScript decoder.js)."""

from .bitstream import BitReader
from .crc32 import crc32
from .values import decode_value
from .varint import read_var_uint

# Format constants
TP_MAGIC = bytes([0x54, 0x52, 0x50, 0x00])
TP_HEADER_SIZE = 32
TP_FLAG_HAS_VALUES = 1
NUM_CONTROL_CODES = 6

# Control code indices
CTRL_END = 0
CTRL_END_VAL = 1
CTRL_SKIP = 2
CTRL_BRANCH = 5


def decode(buffer):
    """Decode a .trp binary buffer into a dict. Returns dict."""
    if not isinstance(buffer, (bytes, bytearray)):
        raise TypeError("decode expects bytes or bytearray")
    if len(buffer) < TP_HEADER_SIZE + 4:
        raise ValueError("Data too short for .trp format")

    buf = bytes(buffer)

    # Validate magic
    for i in range(4):
        if buf[i] != TP_MAGIC[i]:
            raise ValueError("Invalid magic bytes — not a .trp file")

    # Version
    version_major = buf[4]
    if version_major != 1:
        raise ValueError(f"Unsupported format version: {version_major}")

    # Flags
    flags = (buf[6] << 8) | buf[7]
    has_values = (flags & TP_FLAG_HAS_VALUES) != 0

    # num_keys
    num_keys = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11]

    # Offsets
    trie_data_offset = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15]
    value_store_offset = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19]

    # CRC verification
    crc_data_len = len(buf) - 4
    expected_crc = (
        (buf[crc_data_len] << 24)
        | (buf[crc_data_len + 1] << 16)
        | (buf[crc_data_len + 2] << 8)
        | buf[crc_data_len + 3]
    )
    actual_crc = crc32(buf[:crc_data_len])
    if actual_crc != expected_crc:
        raise ValueError("CRC-32 integrity check failed")

    if num_keys == 0:
        return {}

    # Parse trie config
    reader = BitReader(buf)
    data_start = TP_HEADER_SIZE * 8

    reader.seek(data_start)
    bps = reader.read_bits(4)
    # Symbol codes index 256-entry maps, so a symbol may not be wider than a
    # byte; 0 would make the trie unreadable.
    if bps < 1 or bps > 8:
        raise ValueError(f"Invalid trie config: bits_per_symbol must be 1..8, got {bps}")
    symbol_count = reader.read_bits(8)
    # Must leave room for the control codes and fit in bits_per_symbol.
    if symbol_count < NUM_CONTROL_CODES or symbol_count > (1 << bps):
        raise ValueError(f"Invalid trie config: symbol_count out of range: {symbol_count}")

    # Read control codes
    ctrl_codes = [0] * NUM_CONTROL_CODES
    code_is_ctrl = [False] * 256
    for c in range(NUM_CONTROL_CODES):
        ctrl_codes[c] = reader.read_bits(bps)
        if ctrl_codes[c] < 256:
            code_is_ctrl[ctrl_codes[c]] = True

    # Read symbol table
    reverse_map = [0] * 256
    for cd in range(NUM_CONTROL_CODES, symbol_count):
        cp = read_var_uint(reader)
        if cd < 256 and cp < 256:
            reverse_map[cd] = cp

    trie_start = data_start + trie_data_offset
    value_start = data_start + value_store_offset
    # The trie occupies [trie_start, value_start); the value store (when
    # present), the byte padding and the CRC follow it.
    trie_end = value_start

    # DFS iteration
    result = {}
    key_stack = []

    # Every subtree knows where it ends: a child with a SKIP ends at
    # child_start + skip_dist, the last child ends where its parent does, and
    # the root ends at trie_end. `end` is the only authority on whether more
    # symbols belong to this subtree -- the bits that happen to follow are
    # not, because past the last terminal they are padding and CRC.
    def dfs_walk(r, end):
        while True:
            if r.position >= end:
                return
            sym = r.read_bits(bps)

            if sym == ctrl_codes[CTRL_END]:
                key_str = bytes(key_stack).decode("utf-8")
                result[key_str] = None
                walk_branch_if_present(r, end)
                return

            if sym == ctrl_codes[CTRL_END_VAL]:
                read_var_uint(r)  # value index
                key_str = bytes(key_stack).decode("utf-8")
                result[key_str] = None
                walk_branch_if_present(r, end)
                return

            if sym == ctrl_codes[CTRL_BRANCH]:
                child_count = read_var_uint(r)
                walk_branch(r, child_count, end)
                return

            # Regular symbol
            byte_val = reverse_map[sym] if sym < 256 else 0
            key_stack.append(byte_val)

    # A terminal is followed by a BRANCH exactly when the subtree has not
    # reached its end -- keys that share this terminal as a prefix.
    def walk_branch_if_present(r, end):
        if r.position >= end:
            return
        sym = r.read_bits(bps)
        if sym != ctrl_codes[CTRL_BRANCH]:
            raise ValueError("Malformed trie: expected BRANCH after terminal")
        child_count = read_var_uint(r)
        walk_branch(r, child_count, end)

    def walk_branch(r, child_count, end):
        saved_key_len = len(key_stack)
        for ci in range(child_count):
            has_skip = ci < child_count - 1
            skip_dist = 0

            if has_skip:
                r.read_bits(bps)  # SKIP control code
                skip_dist = read_var_uint(r)

            child_start_pos = r.position
            del key_stack[saved_key_len:]
            dfs_walk(r, child_start_pos + skip_dist if has_skip else end)

            if has_skip:
                r.seek(child_start_pos + skip_dist)

        del key_stack[saved_key_len:]

    # Walk the trie
    reader.seek(trie_start)
    if num_keys > 0:
        dfs_walk(reader, trie_end)

    # Decode values
    if has_values:
        reader.seek(value_start)
        sorted_keys = sorted(result.keys(), key=lambda k: k.encode("utf-8"))
        for key in sorted_keys:
            result[key] = decode_value(reader)

    return result
