#!/usr/bin/env python3
# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""Generate tests/conformance/malformed/, buffers every reader must reject.

Each file is a valid fixture with one field damaged. Where the damage is inside
the data the CRC is recomputed, so the reader has to catch the problem on its
own rather than being handed a checksum failure.

Run from the repository root, after the conformance fixtures exist:

    python3 tools/gen_malformed_fixtures.py
"""

import os
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tests", "conformance", "fixtures", "shared_prefix.trp")
OUT = os.path.join(ROOT, "tests", "conformance", "malformed")


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def reseal(buf):
    """Recompute the trailing CRC-32 so only the intended damage remains."""
    body = bytes(buf[:-4])
    return body + crc32(body).to_bytes(4, "big")


def patch_trie_config(buf, bps=None, symbol_count=None):
    """Rewrite the 4-bit bits_per_symbol / 8-bit symbol_count at byte 32."""
    b = bytearray(buf)
    cur_bps = b[32] >> 4
    cur_count = ((b[32] & 0x0F) << 4) | (b[33] >> 4)
    if bps is None:
        bps = cur_bps
    if symbol_count is None:
        symbol_count = cur_count
    b[32] = ((bps & 0x0F) << 4) | ((symbol_count >> 4) & 0x0F)
    b[33] = ((symbol_count & 0x0F) << 4) | (b[33] & 0x0F)
    return b


def main():
    with open(SRC, "rb") as f:
        good = f.read()
    os.makedirs(OUT, exist_ok=True)

    cases = {}

    # Header-level damage.
    b = bytearray(good)
    b[0] = 0x58  # "XRP\0"
    cases["bad_magic.trp"] = reseal(b)

    b = bytearray(good)
    b[4] = 2  # format version 2
    cases["bad_version.trp"] = reseal(b)

    b = bytearray(good)
    b[-1] ^= 0xFF  # checksum no longer matches the body
    cases["bad_crc.trp"] = bytes(b)

    cases["truncated_header.trp"] = good[:20]
    cases["truncated_body.trp"] = reseal(bytearray(good[: len(good) // 2]) + b"\0\0\0\0")

    # Trie config out of range. A symbol may not be wider than a byte, and
    # symbol_count must leave room for the 6 control codes and fit in bps.
    cases["bps_zero.trp"] = reseal(patch_trie_config(good, bps=0))
    cases["bps_too_wide.trp"] = reseal(patch_trie_config(good, bps=9))
    cases["bps_max.trp"] = reseal(patch_trie_config(good, bps=15))
    cases["symbol_count_below_control.trp"] = reseal(
        patch_trie_config(good, symbol_count=3)
    )
    cases["symbol_count_zero.trp"] = reseal(patch_trie_config(good, symbol_count=0))
    cases["symbol_count_exceeds_bps.trp"] = reseal(
        patch_trie_config(good, bps=4, symbol_count=200)
    )

    for name, data in sorted(cases.items()):
        path = os.path.join(OUT, name)
        with open(path, "wb") as f:
            f.write(data)
        print("  %-34s %5d bytes" % (name, len(data)))

    print("%d malformed fixtures" % len(cases))


if __name__ == "__main__":
    main()
