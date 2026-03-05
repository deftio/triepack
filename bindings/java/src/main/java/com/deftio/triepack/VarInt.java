/*
 * VarInt.java
 *
 * LEB128 unsigned VarInt + zigzag signed VarInt -- matches bitstream_varint.c.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

/**
 * LEB128 unsigned VarInt and zigzag signed VarInt encoding/decoding.
 */
public class VarInt {

    private static final int VARINT_MAX_GROUPS = 10;

    /**
     * Write an unsigned LEB128 VarInt.
     */
    public static void writeVarUint(BitWriter writer, long value) {
        if (value < 0) {
            throw new IllegalArgumentException("writeVarUint: negative value");
        }
        while (true) {
            int b = (int) (value & 0x7F);
            value >>>= 7;
            if (value > 0) {
                b |= 0x80;
            }
            writer.writeU8(b);
            if (value == 0) {
                break;
            }
        }
    }

    /**
     * Read an unsigned LEB128 VarInt.
     */
    public static long readVarUint(BitReader reader) {
        long val = 0;
        int shift = 0;
        for (int i = 0; i < VARINT_MAX_GROUPS; i++) {
            int b = reader.readU8();
            val |= ((long) (b & 0x7F)) << shift;
            if ((b & 0x80) == 0) {
                return val;
            }
            shift += 7;
        }
        throw new ArithmeticException("VarInt overflow");
    }

    /**
     * Write a signed zigzag VarInt.
     */
    public static void writeVarInt(BitWriter writer, long value) {
        long raw;
        if (value >= 0) {
            raw = value * 2;
        } else {
            raw = (-value) * 2 - 1;
        }
        writeVarUint(writer, raw);
    }

    /**
     * Read a signed zigzag VarInt.
     */
    public static long readVarInt(BitReader reader) {
        long raw = readVarUint(reader);
        if ((raw & 1) != 0) {
            return -(raw >>> 1) - 1;
        }
        return raw >>> 1;
    }

    /**
     * Return the number of bits needed to encode val as a VarUint.
     */
    public static int varUintBits(long val) {
        int bits = 0;
        while (true) {
            bits += 8;
            val >>>= 7;
            if (val == 0) {
                break;
            }
        }
        return bits;
    }
}
