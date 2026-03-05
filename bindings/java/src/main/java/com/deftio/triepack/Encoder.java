/*
 * Encoder.java
 *
 * Trie encoder -- port of src/core/encoder.c (via Python encoder.py).
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

/**
 * Encodes a Map into the .trp binary format.
 */
public class Encoder {

    // Format constants (match core_internal.h)
    static final byte[] TP_MAGIC = {0x54, 0x52, 0x50, 0x00}; // "TRP\0"
    static final int TP_VERSION_MAJOR = 1;
    static final int TP_VERSION_MINOR = 0;
    static final int TP_HEADER_SIZE = 32;
    static final int TP_FLAG_HAS_VALUES = 1;

    // Control code indices
    static final int CTRL_END = 0;
    static final int CTRL_END_VAL = 1;
    static final int CTRL_SKIP = 2;
    // static final int CTRL_SUFFIX = 3;
    // static final int CTRL_ESCAPE = 4;
    static final int CTRL_BRANCH = 5;
    static final int NUM_CONTROL_CODES = 6;

    /**
     * Encode a map into the .trp binary format. Returns the encoded bytes.
     */
    public static byte[] encode(Map<String, TpValue> data) {
        if (data == null) {
            throw new IllegalArgumentException("encode expects a non-null map");
        }

        // Collect entries: (key_bytes, val)
        List<byte[]> keyBytesList = new ArrayList<>();
        List<TpValue> valList = new ArrayList<>();
        for (Map.Entry<String, TpValue> entry : data.entrySet()) {
            byte[] keyBytes = entry.getKey().getBytes(StandardCharsets.UTF_8);
            keyBytesList.add(keyBytes);
            valList.add(entry.getValue());
        }

        // Build index array and sort by key bytes (lexicographic)
        int n = keyBytesList.size();
        Integer[] indices = new Integer[n];
        for (int i = 0; i < n; i++) {
            indices[i] = i;
        }
        java.util.Arrays.sort(indices, Comparator.comparing(
            idx -> keyBytesList.get(idx),
            Encoder::compareBytes
        ));

        // Build sorted arrays
        byte[][] sortedKeys = new byte[n][];
        TpValue[] sortedVals = new TpValue[n];
        for (int i = 0; i < n; i++) {
            sortedKeys[i] = keyBytesList.get(indices[i]);
            sortedVals[i] = valList.get(indices[i]);
        }

        // Determine if any entries have non-null values
        boolean hasValues = false;
        for (TpValue v : sortedVals) {
            if (v != null && v.getType() != TpValue.Type.NULL) {
                hasValues = true;
                break;
            }
        }

        // Symbol analysis
        boolean[] used = new boolean[256];
        for (byte[] kb : sortedKeys) {
            for (byte b : kb) {
                used[b & 0xFF] = true;
            }
        }

        int alphabetSize = 0;
        for (boolean u : used) {
            if (u) alphabetSize++;
        }
        int totalSymbols = alphabetSize + NUM_CONTROL_CODES;
        int bps = 1;
        while ((1 << bps) < totalSymbols) {
            bps++;
        }

        // Control codes: 0..5
        int[] ctrlCodes = new int[NUM_CONTROL_CODES];
        for (int c = 0; c < NUM_CONTROL_CODES; c++) {
            ctrlCodes[c] = c;
        }

        // Symbol map: byte value -> N-bit code
        int[] symbolMap = new int[256];
        int[] reverseMap = new int[256];
        int code = NUM_CONTROL_CODES;
        for (int i = 0; i < 256; i++) {
            if (used[i]) {
                symbolMap[i] = code;
                if (code < 256) {
                    reverseMap[code] = i;
                }
                code++;
            }
        }

        // Build the bitstream
        BitWriter w = new BitWriter(256);

        // Write 32-byte header placeholder
        for (byte b : TP_MAGIC) {
            w.writeU8(b & 0xFF);
        }
        w.writeU8(TP_VERSION_MAJOR);
        w.writeU8(TP_VERSION_MINOR);
        int flags = hasValues ? TP_FLAG_HAS_VALUES : 0;
        w.writeU16(flags);
        w.writeU32(n);  // num_keys
        w.writeU32(0);  // trie_data_offset placeholder
        w.writeU32(0);  // value_store_offset placeholder
        w.writeU32(0);  // suffix_table_offset placeholder
        w.writeU32(0);  // total_data_bits placeholder
        w.writeU32(0);  // reserved

        int dataStart = w.getPosition(); // should be 256 (32 bytes * 8)

        // Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
        w.writeBits(bps, 4);
        w.writeBits(totalSymbols, 8);

        // Control code mappings
        for (int c = 0; c < NUM_CONTROL_CODES; c++) {
            w.writeBits(ctrlCodes[c], bps);
        }

        // Symbol table
        for (int cd = NUM_CONTROL_CODES; cd < totalSymbols; cd++) {
            int byteVal = cd < 256 ? reverseMap[cd] : 0;
            VarInt.writeVarUint(w, byteVal);
        }

        int trieDataOffset = w.getPosition() - dataStart;

        // Write prefix trie
        TrieContext ctx = new TrieContext(sortedKeys, sortedVals, bps,
                                          symbolMap, ctrlCodes, hasValues);
        int[] valueIdx = {0};
        if (n > 0) {
            trieWrite(ctx, w, 0, n, 0, valueIdx);
        }

        int valueStoreOffset = w.getPosition() - dataStart;

        // Write value store
        if (hasValues) {
            for (TpValue val : sortedVals) {
                Values.encodeValue(w, val);
            }
        }

        int totalDataBits = w.getPosition() - dataStart;

        w.alignToByte();

        // Get pre-CRC buffer
        byte[] preCrcBuf = w.toByteArray();
        long crcVal = Crc32.crc32(preCrcBuf);

        // Append CRC
        w.writeU32(crcVal);

        byte[] outBuf = w.toByteArray();

        // Patch header offsets (big-endian at byte offsets 12,16,20,24)
        patchU32(outBuf, 12, trieDataOffset);
        patchU32(outBuf, 16, valueStoreOffset);
        patchU32(outBuf, 20, 0); // suffix_table_offset
        patchU32(outBuf, 24, totalDataBits);

        // Recompute CRC over patched data
        int crcDataLen = outBuf.length - 4;
        crcVal = Crc32.crc32(outBuf, 0, crcDataLen);
        patchU32(outBuf, crcDataLen, crcVal);

        return outBuf;
    }

    private static void patchU32(byte[] buf, int offset, long value) {
        buf[offset] = (byte) ((value >>> 24) & 0xFF);
        buf[offset + 1] = (byte) ((value >>> 16) & 0xFF);
        buf[offset + 2] = (byte) ((value >>> 8) & 0xFF);
        buf[offset + 3] = (byte) (value & 0xFF);
    }

    /**
     * Compare two byte arrays lexicographically (unsigned).
     */
    private static int compareBytes(byte[] a, byte[] b) {
        int len = Math.min(a.length, b.length);
        for (int i = 0; i < len; i++) {
            int ai = a[i] & 0xFF;
            int bi = b[i] & 0xFF;
            if (ai != bi) {
                return ai - bi;
            }
        }
        return a.length - b.length;
    }

    /**
     * Compute subtree size in bits (dry run).
     */
    private static int trieSubtreeSize(TrieContext ctx, int start, int end,
                                        int prefixLen, int[] valueIdx) {
        int bps = ctx.bps;
        boolean hasValues = ctx.hasValues;
        byte[][] keys = ctx.keys;
        TpValue[] vals = ctx.vals;
        int bits = 0;

        // Find common prefix beyond prefixLen
        int common = prefixLen;
        while (true) {
            boolean allHave = true;
            int ch = 0;
            for (int i = start; i < end; i++) {
                if (keys[i].length <= common) {
                    allHave = false;
                    break;
                }
                if (i == start) {
                    ch = keys[i][common] & 0xFF;
                } else if ((keys[i][common] & 0xFF) != ch) {
                    allHave = false;
                    break;
                }
            }
            if (!allHave) break;
            common++;
        }

        bits += (common - prefixLen) * bps;

        boolean hasTerminal = keys[start].length == common;
        int childrenStart = hasTerminal ? start + 1 : start;

        // Count children
        int childCount = 0;
        int cs = childrenStart;
        while (cs < end) {
            childCount++;
            int ch = keys[cs][common] & 0xFF;
            int ce = cs + 1;
            while (ce < end && (keys[ce][common] & 0xFF) == ch) {
                ce++;
            }
            cs = ce;
        }

        if (hasTerminal && childCount == 0) {
            if (hasValues && vals[start] != null && vals[start].getType() != TpValue.Type.NULL) {
                bits += bps;
                bits += VarInt.varUintBits(valueIdx[0]);
            } else {
                bits += bps;
            }
            valueIdx[0]++;
        } else if (hasTerminal && childCount > 0) {
            if (hasValues && vals[start] != null && vals[start].getType() != TpValue.Type.NULL) {
                bits += bps;
                bits += VarInt.varUintBits(valueIdx[0]);
            } else {
                bits += bps;
            }
            valueIdx[0]++;
            bits += bps; // BRANCH
            bits += VarInt.varUintBits(childCount);

            cs = childrenStart;
            int childI = 0;
            while (cs < end) {
                int ch = keys[cs][common] & 0xFF;
                int ce = cs + 1;
                while (ce < end && (keys[ce][common] & 0xFF) == ch) {
                    ce++;
                }
                if (childI < childCount - 1) {
                    int childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                    bits += bps; // SKIP
                    bits += VarInt.varUintBits(childSz);
                    bits += childSz;
                } else {
                    bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                }
                childI++;
                cs = ce;
            }
        } else {
            // not hasTerminal, childCount > 0
            bits += bps; // BRANCH
            bits += VarInt.varUintBits(childCount);

            cs = childrenStart;
            int childI = 0;
            while (cs < end) {
                int ch = keys[cs][common] & 0xFF;
                int ce = cs + 1;
                while (ce < end && (keys[ce][common] & 0xFF) == ch) {
                    ce++;
                }
                if (childI < childCount - 1) {
                    int childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                    bits += bps; // SKIP
                    bits += VarInt.varUintBits(childSz);
                    bits += childSz;
                } else {
                    bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                }
                childI++;
                cs = ce;
            }
        }

        return bits;
    }

    /**
     * Write the trie subtree.
     */
    private static void trieWrite(TrieContext ctx, BitWriter w,
                                   int start, int end, int prefixLen,
                                   int[] valueIdx) {
        int bps = ctx.bps;
        int[] symbolMap = ctx.symbolMap;
        int[] ctrlCodes = ctx.ctrlCodes;
        boolean hasValues = ctx.hasValues;
        byte[][] keys = ctx.keys;
        TpValue[] vals = ctx.vals;

        // Find common prefix beyond prefixLen
        int common = prefixLen;
        while (true) {
            boolean allHave = true;
            int ch = 0;
            for (int i = start; i < end; i++) {
                if (keys[i].length <= common) {
                    allHave = false;
                    break;
                }
                if (i == start) {
                    ch = keys[i][common] & 0xFF;
                } else if ((keys[i][common] & 0xFF) != ch) {
                    allHave = false;
                    break;
                }
            }
            if (!allHave) break;
            common++;
        }

        // Write common prefix symbols
        for (int i = prefixLen; i < common; i++) {
            int ch = keys[start][i] & 0xFF;
            w.writeBits(symbolMap[ch], bps);
        }

        boolean hasTerminal = keys[start].length == common;
        int childrenStart = hasTerminal ? start + 1 : start;

        // Count children
        int childCount = 0;
        int cs = childrenStart;
        while (cs < end) {
            childCount++;
            int ch = keys[cs][common] & 0xFF;
            int ce = cs + 1;
            while (ce < end && (keys[ce][common] & 0xFF) == ch) {
                ce++;
            }
            cs = ce;
        }

        // Write terminal
        if (hasTerminal) {
            if (hasValues && vals[start] != null && vals[start].getType() != TpValue.Type.NULL) {
                w.writeBits(ctrlCodes[CTRL_END_VAL], bps);
                VarInt.writeVarUint(w, valueIdx[0]);
            } else {
                w.writeBits(ctrlCodes[CTRL_END], bps);
            }
            valueIdx[0]++;
        }

        if (childCount == 0) {
            return;
        }

        // BRANCH
        w.writeBits(ctrlCodes[CTRL_BRANCH], bps);
        VarInt.writeVarUint(w, childCount);

        cs = childrenStart;
        int childI = 0;
        while (cs < end) {
            int ch = keys[cs][common] & 0xFF;
            int ce = cs + 1;
            while (ce < end && (keys[ce][common] & 0xFF) == ch) {
                ce++;
            }

            if (childI < childCount - 1) {
                int[] viCopy = {valueIdx[0]};
                int childSz = trieSubtreeSize(ctx, cs, ce, common, viCopy);
                w.writeBits(ctrlCodes[CTRL_SKIP], bps);
                VarInt.writeVarUint(w, childSz);
            }

            trieWrite(ctx, w, cs, ce, common, valueIdx);

            childI++;
            cs = ce;
        }
    }

    /**
     * Context passed through recursive trie writing.
     */
    private static class TrieContext {
        final byte[][] keys;
        final TpValue[] vals;
        final int bps;
        final int[] symbolMap;
        final int[] ctrlCodes;
        final boolean hasValues;

        TrieContext(byte[][] keys, TpValue[] vals, int bps,
                    int[] symbolMap, int[] ctrlCodes, boolean hasValues) {
            this.keys = keys;
            this.vals = vals;
            this.bps = bps;
            this.symbolMap = symbolMap;
            this.ctrlCodes = ctrlCodes;
            this.hasValues = hasValues;
        }
    }
}
