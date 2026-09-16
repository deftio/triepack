# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import math
import os

import pytest

import triepack


def test_version():
    assert triepack.__version__ == "1.3.2"


def test_empty_object():
    buf = triepack.encode({})
    assert isinstance(buf, bytes)
    assert len(buf) >= 36  # 32-byte header + 4-byte CRC
    result = triepack.decode(buf)
    assert result == {}


def test_single_key_null():
    data = {"hello": None}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_integer():
    data = {"key": 42}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_negative():
    data = {"neg": -100}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_bool_true():
    data = {"flag": True}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_bool_false():
    data = {"flag": False}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_float64():
    data = {"pi": 3.14159}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert math.isclose(result["pi"], 3.14159, rel_tol=1e-10)


def test_single_key_string():
    data = {"greeting": "hello world"}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_key_blob():
    blob = b"\xDE\xAD\xBE\xEF"
    data = {"binary": blob}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result["binary"] == blob


def test_multiple_keys_mixed():
    data = {
        "bool": True,
        "f64": 3.14159,
        "int": -100,
        "str": "hello",
        "uint": 200,
    }
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result["bool"] is True
    assert math.isclose(result["f64"], 3.14159, rel_tol=1e-10)
    assert result["int"] == -100
    assert result["str"] == "hello"
    assert result["uint"] == 200


def test_shared_prefix():
    data = {"abc": 10, "abd": 20, "xyz": 30}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_keys_only_null():
    data = {"apple": None, "banana": None, "cherry": None}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_large_dictionary():
    data = {}
    for i in range(100):
        data[f"key_{i:04d}"] = i
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert len(result) == 100
    for i in range(100):
        assert result[f"key_{i:04d}"] == i


def test_magic_bytes():
    buf = triepack.encode({"test": 1})
    assert buf[0] == 0x54  # T
    assert buf[1] == 0x52  # R
    assert buf[2] == 0x50  # P
    assert buf[3] == 0x00


def test_version_header():
    buf = triepack.encode({"test": 1})
    assert buf[4] == 1  # major
    assert buf[5] == 0  # minor


def test_crc_corruption():
    buf = bytearray(triepack.encode({"test": 1}))
    buf[10] ^= 0x01
    with pytest.raises(ValueError, match="CRC"):
        triepack.decode(bytes(buf))


def test_invalid_magic():
    buf = bytearray(40)
    buf[0] = 0xFF
    with pytest.raises(ValueError, match="magic"):
        triepack.decode(bytes(buf))


def test_encode_rejects_non_dict():
    with pytest.raises(TypeError):
        triepack.encode("string")
    with pytest.raises(TypeError):
        triepack.encode(42)
    with pytest.raises(TypeError):
        triepack.encode(None)


def test_decode_rejects_short():
    with pytest.raises(ValueError):
        triepack.decode(b"\x00" * 10)


def test_utf8_keys():
    data = {"café": 1, "naïve": 2, "über": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_single_char_keys():
    data = {"a": 1, "b": 2, "c": 3, "d": 4, "e": 5}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_empty_string_key():
    data = {"": 99}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result[""] == 99


def test_long_shared_prefix():
    data = {"aaaaaa1": 1, "aaaaaa2": 2, "aaaaaa3": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_decode_non_bytes():
    with pytest.raises(TypeError):
        triepack.decode("not bytes")


def test_decode_bad_version():
    buf = bytearray(triepack.encode({"a": 1}))
    buf[4] = 2
    from triepack.crc32 import crc32

    crc_data = buf[:-4]
    new_crc = crc32(bytes(crc_data))
    buf[-4] = (new_crc >> 24) & 0xFF
    buf[-3] = (new_crc >> 16) & 0xFF
    buf[-2] = (new_crc >> 8) & 0xFF
    buf[-1] = new_crc & 0xFF
    with pytest.raises(ValueError, match="version"):
        triepack.decode(bytes(buf))


def test_prefix_key_keys_only():
    """Keys-only mode with prefix key: triggers CTRL_END + CTRL_BRANCH."""
    data = {"a": None, "ab": None, "ac": None}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert set(result.keys()) == {"a", "ab", "ac"}
    for v in result.values():
        assert v is None


def test_prefix_key_with_values():
    """Has-values mode with prefix key: triggers CTRL_END_VAL + CTRL_BRANCH."""
    data = {"a": 1, "ab": 2, "ac": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_terminal_with_children_nonlast():
    """Terminal-with-children subtree as non-last child in branch."""
    data = {"a": 1, "ab": 2, "b": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_terminal_with_children_null_value():
    """Terminal-with-children where terminal has null value."""
    data = {"a": None, "ab": 2, "b": 3}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_null_value_in_mixed_dict():
    """Explicit None alongside non-None values."""
    data = {"alpha": None, "beta": 42}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_subtree_multi_entry_groups():
    """Terminal-with-children subtree as non-last child, with 2+ children
    and multi-entry child groups, covering subtree size inner loop."""
    data = {"a": 1, "aba": 2, "abb": 3, "ac": 4, "b": 5}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_non_terminal_multi_entry_children():
    """Non-terminal subtree as non-last child with multi-entry child group."""
    data = {"aba": 1, "abb": 2, "ac": 3, "b": 4}
    buf = triepack.encode(data)
    result = triepack.decode(buf)
    assert result == data


def test_decoder_eof_during_dfs():
    """Truncated trie data triggers EOF check in dfs_walk."""
    from triepack.bitstream import BitWriter as BW
    from triepack.crc32 import crc32
    from triepack.encoder import TP_MAGIC, TP_VERSION_MAJOR, TP_VERSION_MINOR
    from triepack.varint import write_var_uint

    w = BW()
    for b in TP_MAGIC:
        w.write_u8(b)
    w.write_u8(TP_VERSION_MAJOR)
    w.write_u8(TP_VERSION_MINOR)
    w.write_u16(0)  # flags: no values
    w.write_u32(1)  # num_keys = 1
    w.write_u32(0)  # trie_data_offset placeholder
    w.write_u32(0)  # value_store_offset placeholder
    w.write_u32(0)  # suffix
    w.write_u32(0)  # total_data_bits
    w.write_u32(0)  # reserved

    data_start = w.position
    bps = 4
    w.write_bits(bps, 4)
    total_symbols = 7  # 6 control + 1 regular
    w.write_bits(total_symbols, 8)
    for c in range(6):
        w.write_bits(c, bps)
    write_var_uint(w, ord("a"))

    # Point trie_data_offset far past the actual data
    trie_data_offset = 99999
    value_store_offset = trie_data_offset

    w.align_to_byte()
    buf = bytearray(w.to_bytes())

    def patch_u32(b, off, val):
        b[off] = (val >> 24) & 0xFF
        b[off + 1] = (val >> 16) & 0xFF
        b[off + 2] = (val >> 8) & 0xFF
        b[off + 3] = val & 0xFF

    patch_u32(buf, 12, trie_data_offset)
    patch_u32(buf, 16, value_store_offset)

    crc_val = crc32(bytes(buf))
    buf.append((crc_val >> 24) & 0xFF)
    buf.append((crc_val >> 16) & 0xFF)
    buf.append((crc_val >> 8) & 0xFF)
    buf.append(crc_val & 0xFF)

    result = triepack.decode(bytes(buf))
    assert isinstance(result, dict)


# Regression: issue #1 -- the walker used to guess whether a BRANCH followed a
# terminal by peeking at the next bits. Past the last terminal those bits are
# the byte padding and the CRC, which for some key sets look exactly like a
# BRANCH code, sending the walker off the end of the buffer (EOFError). The
# trie end comes from the header, so the walker must stop there.
#
# Each of these key sets produced a trailing BRANCH-looking code and raised
# EOFError before the fix.
TRAILING_BRANCH_KEY_SETS = [
    ["eddb", "h"],
    ["aad", "ebg", "ec", "ehhbf", "h", "hebad"],
    ["aghed", "bae", "bfffa", "cad", "d", "ebbhb", "fa", "feb"],
    ["a", "aad", "bg", "bgc", "egba", "gahad", "ghehg", "hbdab", "hg"],
]


@pytest.mark.parametrize("keys", TRAILING_BRANCH_KEY_SETS)
def test_walk_stops_at_trie_end_keys_only(keys):
    data = {k: None for k in keys}
    assert triepack.decode(triepack.encode(data)) == data


@pytest.mark.parametrize("keys", TRAILING_BRANCH_KEY_SETS)
def test_walk_stops_at_trie_end_with_values(keys):
    data = {k: i for i, k in enumerate(keys)}
    assert triepack.decode(triepack.encode(data)) == data


def test_round_trips_generated_key_sets():
    """The trigger is set-dependent (the trailing bits have to land on the
    BRANCH code), so cover many small key sets deterministically."""
    seed = 1

    def rnd():
        nonlocal seed
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        return seed / 0x7FFFFFFF

    alpha = "abcdefgh"

    for _ in range(2000):
        keys = set()
        n = 2 + int(rnd() * 12)
        while len(keys) < n:
            length = 1 + int(rnd() * 5)
            keys.add("".join(alpha[int(rnd() * len(alpha))] for _ in range(length)))
        data = {k: None for k in keys}
        assert triepack.decode(triepack.encode(data)) == data


def test_header_declares_exactly_the_bits_written():
    buf = triepack.encode({"eddb": None, "h": None})
    value_store_offset = int.from_bytes(buf[16:20], "big")
    total_data_bits = int.from_bytes(buf[24:28], "big")
    # No values, so the trie is the whole data section, and the data section
    # plus its byte padding and the 4-byte CRC is the buffer.
    assert value_store_offset == total_data_bits
    assert len(buf) == 32 + (total_data_bits + 7) // 8 + 4


# -- Version metadata ------------------------------------------------------
#
# The version a build reports has to equal triepack-version.txt, the single
# source of truth. Reading the file rather than a copy of the string is the
# point: a stale constant fails.

VERSION_FILE = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "triepack-version.txt"
)


def declared_version():
    with open(VERSION_FILE, "r", encoding="utf-8") as f:
        return f.read().strip()


def test_version_matches_the_source_of_truth():
    assert triepack.__version__ == declared_version()
    assert triepack.version()["version"] == declared_version()


def test_version_metadata_shape():
    declared = declared_version()
    major, minor, patch = (int(p) for p in declared.split("."))
    assert triepack.version() == {
        "name": "triepack",
        "implementation": "python",
        "version": declared,
        "version_major": major,
        "version_minor": minor,
        "version_patch": patch,
        "format_version_major": 1,
        "format_version_minor": 0,
        "max_alphabet_size": 249,
    }


def test_alphabet_larger_than_the_format_allows_is_rejected():
    """The trie config packs symbol_count into 8 header bits, so the alphabet
    plus the 6 control codes has to fit in 255. A wider alphabet used to
    produce a buffer with a valid CRC that decoded to nothing, so the encoder
    has to refuse it rather than lose the keys.

    Keys are given as bytes: str keys are UTF-8 encoded, which cannot reach
    250 distinct byte values however they are chosen."""
    entries = {bytes([b]): b for b in range(1, 256)}
    with pytest.raises(ValueError, match="too many distinct byte values"):
        triepack.encode(entries)


def test_alphabet_at_the_limit_still_encodes():
    """249 symbols is the documented maximum and must keep working. The keys
    are not valid UTF-8, so this checks encoding only — decode returns str."""
    entries = {bytes([b]): b for b in range(1, 250)}
    buf = triepack.encode(entries)
    assert buf[:4] == b"TRP\x00"
    assert triepack.version()["max_alphabet_size"] == 249


def test_terminal_without_the_branch_it_promises_is_rejected():
    """A terminal whose subtree has not ended must be followed by a BRANCH.
    Corrupting that symbol has to be caught rather than walked past."""
    import triepack.crc32 as crc32mod

    buf = bytearray(triepack.encode({"he": 1, "hello": 2}))

    # Flip bits through the trie body until one lands on the BRANCH symbol
    # that follows the terminal for "he". The CRC is repaired each time so
    # the failure comes from the walk, not the checksum.
    saw_malformed = False
    for byte in range(32, len(buf) - 4):
        for bit in range(8):
            trial = bytearray(buf)
            trial[byte] ^= 1 << bit
            crc = crc32mod.crc32(bytes(trial[:-4]))
            trial[-4:] = crc.to_bytes(4, "big")
            try:
                triepack.decode(bytes(trial))
            except ValueError as exc:
                if "expected BRANCH after terminal" in str(exc):
                    saw_malformed = True
            except Exception:
                pass
    assert saw_malformed, "no corruption reached the BRANCH-after-terminal check"
