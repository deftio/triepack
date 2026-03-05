/*
 * TriePack.java
 *
 * Native Java implementation of the TriePack (.trp) binary format.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import java.util.Map;

/**
 * Encodes and decodes key-value dictionaries in the TriePack (.trp) binary
 * format.
 */
public class TriePack {

    /**
     * Encodes a map of key-value pairs into the .trp binary format.
     *
     * @param data The map to encode (String keys, TpValue values)
     * @return The encoded .trp binary data
     */
    public static byte[] encode(Map<String, TpValue> data) {
        return Encoder.encode(data);
    }

    /**
     * Decodes a .trp binary blob into a map of key-value pairs.
     *
     * @param data The .trp binary data to decode
     * @return The decoded key-value map
     */
    public static Map<String, TpValue> decode(byte[] data) {
        return Decoder.decode(data);
    }
}
