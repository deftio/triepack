/*
 * Decoder.java
 *
 * Trie decoder -- port of src/core/decoder.c (via Python decoder.py).
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Decodes a .trp binary buffer into a Map.
 */
public class Decoder {

    // Format constants
    static final byte[] TP_MAGIC = {0x54, 0x52, 0x50, 0x00};
    static final int TP_HEADER_SIZE = 32;
    static final int TP_FLAG_HAS_VALUES = 1;
    static final int NUM_CONTROL_CODES = 6;

    // Control code indices
    static final int CTRL_END = 0;
    static final int CTRL_END_VAL = 1;
    static final int CTRL_SKIP = 2;
    static final int CTRL_BRANCH = 5;

    /**
     * Decode a .trp binary buffer into a map. Returns a Map of String to TpValue.
     */
    public static Map<String, TpValue> decode(byte[] buffer) {
        if (buffer == null) {
            throw new IllegalArgumentException("decode expects non-null byte array");
        }
        if (buffer.length < TP_HEADER_SIZE + 4) {
            throw new IllegalArgumentException("Data too short for .trp format");
        }

        // Validate magic
        for (int i = 0; i < 4; i++) {
            if (buffer[i] != TP_MAGIC[i]) {
                throw new IllegalArgumentException("Invalid magic bytes -- not a .trp file");
            }
        }

        // Version
        int versionMajor = buffer[4] & 0xFF;
        if (versionMajor != 1) {
            throw new IllegalArgumentException("Unsupported format version: " + versionMajor);
        }

        // Flags
        int flags = ((buffer[6] & 0xFF) << 8) | (buffer[7] & 0xFF);
        boolean hasValues = (flags & TP_FLAG_HAS_VALUES) != 0;

        // num_keys
        int numKeys = ((buffer[8] & 0xFF) << 24) | ((buffer[9] & 0xFF) << 16)
                     | ((buffer[10] & 0xFF) << 8) | (buffer[11] & 0xFF);

        // Offsets
        int trieDataOffset = ((buffer[12] & 0xFF) << 24) | ((buffer[13] & 0xFF) << 16)
                            | ((buffer[14] & 0xFF) << 8) | (buffer[15] & 0xFF);
        int valueStoreOffset = ((buffer[16] & 0xFF) << 24) | ((buffer[17] & 0xFF) << 16)
                              | ((buffer[18] & 0xFF) << 8) | (buffer[19] & 0xFF);

        // CRC verification
        int crcDataLen = buffer.length - 4;
        long expectedCrc = ((long) (buffer[crcDataLen] & 0xFF) << 24)
                         | ((long) (buffer[crcDataLen + 1] & 0xFF) << 16)
                         | ((long) (buffer[crcDataLen + 2] & 0xFF) << 8)
                         | (buffer[crcDataLen + 3] & 0xFF);
        long actualCrc = Crc32.crc32(buffer, 0, crcDataLen);
        if (actualCrc != expectedCrc) {
            throw new IllegalArgumentException("CRC-32 integrity check failed");
        }

        if (numKeys == 0) {
            return new LinkedHashMap<>();
        }

        // Parse trie config
        BitReader reader = new BitReader(buffer);
        int dataStart = TP_HEADER_SIZE * 8;

        reader.seek(dataStart);
        int bps = (int) reader.readBits(4);
        int symbolCount = (int) reader.readBits(8);

        // Read control codes
        int[] ctrlCodes = new int[NUM_CONTROL_CODES];
        boolean[] codeIsCtrl = new boolean[256];
        for (int c = 0; c < NUM_CONTROL_CODES; c++) {
            ctrlCodes[c] = (int) reader.readBits(bps);
            if (ctrlCodes[c] < 256) {
                codeIsCtrl[ctrlCodes[c]] = true;
            }
        }

        // Read symbol table
        int[] reverseMap = new int[256];
        for (int cd = NUM_CONTROL_CODES; cd < symbolCount; cd++) {
            int cp = (int) VarInt.readVarUint(reader);
            if (cd < 256 && cp < 256) {
                reverseMap[cd] = cp;
            }
        }

        int trieStart = dataStart + trieDataOffset;
        int valueStart = dataStart + valueStoreOffset;

        // DFS iteration
        Map<String, TpValue> result = new LinkedHashMap<>();
        List<Integer> keyStack = new ArrayList<>();

        // Walk the trie
        reader.seek(trieStart);
        if (numKeys > 0) {
            dfsWalk(reader, bps, ctrlCodes, reverseMap, keyStack, result);
        }

        // Decode values
        if (hasValues) {
            reader.seek(valueStart);
            // Sort keys by UTF-8 bytes
            List<String> sortedKeys = new ArrayList<>(result.keySet());
            sortedKeys.sort(Comparator.comparing(
                k -> k.getBytes(StandardCharsets.UTF_8),
                Decoder::compareBytes
            ));
            for (String key : sortedKeys) {
                result.put(key, Values.decodeValue(reader));
            }
        }

        return result;
    }

    private static void dfsWalk(BitReader r, int bps, int[] ctrlCodes,
                                 int[] reverseMap, List<Integer> keyStack,
                                 Map<String, TpValue> result) {
        while (true) {
            if (r.getPosition() >= r.getBitLen()) {
                return;
            }
            int sym = (int) r.readBits(bps);

            if (sym == ctrlCodes[CTRL_END]) {
                String keyStr = buildKeyString(keyStack);
                result.put(keyStr, TpValue.ofNull());

                if (r.getPosition() + bps <= r.getBitLen()) {
                    int nextSym = (int) r.peekBits(bps);
                    if (nextSym == ctrlCodes[CTRL_BRANCH]) {
                        r.readBits(bps);
                        int childCount = (int) VarInt.readVarUint(r);
                        walkBranch(r, bps, ctrlCodes, reverseMap,
                                   keyStack, result, childCount);
                    }
                }
                return;
            }

            if (sym == ctrlCodes[CTRL_END_VAL]) {
                VarInt.readVarUint(r); // value index (consumed but unused here)
                String keyStr = buildKeyString(keyStack);
                result.put(keyStr, TpValue.ofNull());

                if (r.getPosition() + bps <= r.getBitLen()) {
                    int nextSym = (int) r.peekBits(bps);
                    if (nextSym == ctrlCodes[CTRL_BRANCH]) {
                        r.readBits(bps);
                        int childCount = (int) VarInt.readVarUint(r);
                        walkBranch(r, bps, ctrlCodes, reverseMap,
                                   keyStack, result, childCount);
                    }
                }
                return;
            }

            if (sym == ctrlCodes[CTRL_BRANCH]) {
                int childCount = (int) VarInt.readVarUint(r);
                walkBranch(r, bps, ctrlCodes, reverseMap,
                           keyStack, result, childCount);
                return;
            }

            // Regular symbol
            int byteVal = sym < 256 ? reverseMap[sym] : 0;
            keyStack.add(byteVal);
        }
    }

    private static void walkBranch(BitReader r, int bps, int[] ctrlCodes,
                                    int[] reverseMap, List<Integer> keyStack,
                                    Map<String, TpValue> result,
                                    int childCount) {
        int savedKeyLen = keyStack.size();
        for (int ci = 0; ci < childCount; ci++) {
            boolean hasSkip = ci < childCount - 1;
            int skipDist = 0;

            if (hasSkip) {
                r.readBits(bps); // SKIP control code
                skipDist = (int) VarInt.readVarUint(r);
            }

            int childStartPos = r.getPosition();
            // Truncate key stack to saved length
            while (keyStack.size() > savedKeyLen) {
                keyStack.remove(keyStack.size() - 1);
            }
            dfsWalk(r, bps, ctrlCodes, reverseMap, keyStack, result);

            if (hasSkip) {
                r.seek(childStartPos + skipDist);
            }
        }
        // Truncate key stack to saved length
        while (keyStack.size() > savedKeyLen) {
            keyStack.remove(keyStack.size() - 1);
        }
    }

    private static String buildKeyString(List<Integer> keyStack) {
        byte[] bytes = new byte[keyStack.size()];
        for (int i = 0; i < keyStack.size(); i++) {
            bytes[i] = (byte) (keyStack.get(i) & 0xFF);
        }
        return new String(bytes, StandardCharsets.UTF_8);
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
}
