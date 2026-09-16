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
     * Library version, kept in step with triepack-version.txt by
     * scripts/sync_version.sh.
     */
    public static final String VERSION = "1.3.0";

    /**
     * Version of the on-disk .trp format this implementation writes. Distinct
     * from the library version: it changes only when the bytes change.
     */
    public static final int FORMAT_VERSION_MAJOR = 1;

    /** See {@link #FORMAT_VERSION_MAJOR}. */
    public static final int FORMAT_VERSION_MINOR = 0;

    /**
     * Metadata about a triepack build. Every implementation reports the same
     * fields, so a polyglot system can ask each one what it is.
     */
    public static final class VersionInfo {
        public final String name;
        public final String implementation;
        public final String version;
        public final int versionMajor;
        public final int versionMinor;
        public final int versionPatch;
        public final int formatVersionMajor;
        public final int formatVersionMinor;
        public final int maxAlphabetSize;

        VersionInfo(String name, String implementation, String version,
                    int versionMajor, int versionMinor, int versionPatch,
                    int formatVersionMajor, int formatVersionMinor,
                    int maxAlphabetSize) {
            this.name = name;
            this.implementation = implementation;
            this.version = version;
            this.versionMajor = versionMajor;
            this.versionMinor = versionMinor;
            this.versionPatch = versionPatch;
            this.formatVersionMajor = formatVersionMajor;
            this.formatVersionMinor = formatVersionMinor;
            this.maxAlphabetSize = maxAlphabetSize;
        }

        @Override
        public String toString() {
            return name + " (" + implementation + ") " + version
                 + " format " + formatVersionMajor + "." + formatVersionMinor;
        }
    }

    /** Return metadata about this build. */
    public static VersionInfo version() {
        String[] parts = VERSION.split("\\.");
        return new VersionInfo(
            "triepack", "java", VERSION,
            Integer.parseInt(parts[0]),
            Integer.parseInt(parts[1]),
            Integer.parseInt(parts[2]),
            FORMAT_VERSION_MAJOR, FORMAT_VERSION_MINOR,
            Encoder.MAX_ALPHABET_SIZE);
    }


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
