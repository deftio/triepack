/*
 * BitReader.java
 *
 * Arbitrary-width bit I/O reader -- MSB-first, matching src/bitstream/.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

/**
 * Read arbitrary-width bit fields from a buffer (MSB-first).
 */
public class BitReader {

    private final byte[] buf;
    private final int bitLen;
    private int pos;

    public BitReader(byte[] buf) {
        this.buf = buf;
        this.bitLen = buf.length * 8;
        this.pos = 0;
    }

    public int getPosition() {
        return pos;
    }

    public int getRemaining() {
        return bitLen - pos;
    }

    public int getBitLen() {
        return bitLen;
    }

    public void seek(int bitPos) {
        this.pos = bitPos;
    }

    public long readBits(int n) {
        if (n < 1 || n > 64) {
            throw new IllegalArgumentException("readBits: n must be 1..64");
        }
        if (pos + n > bitLen) {
            throw new RuntimeException("EOF");
        }
        long val = 0;
        for (int i = 0; i < n; i++) {
            int byteIdx = pos >>> 3;
            int bitIdx = 7 - (pos & 7);
            val = (val << 1) | ((buf[byteIdx] >>> bitIdx) & 1);
            pos++;
        }
        return val;
    }

    public int readBit() {
        if (pos >= bitLen) {
            throw new RuntimeException("EOF");
        }
        int byteIdx = pos >>> 3;
        int bitIdx = 7 - (pos & 7);
        int bit = (buf[byteIdx] >>> bitIdx) & 1;
        pos++;
        return bit;
    }

    public int readU8() {
        return (int) readBits(8);
    }

    public int readU16() {
        return (int) readBits(16);
    }

    public long readU32() {
        return readBits(32);
    }

    public long readU64() {
        long hi = readU32();
        long lo = readU32();
        return hi * 0x100000000L + lo;
    }

    public byte[] readBytes(int n) {
        byte[] out = new byte[n];
        for (int i = 0; i < n; i++) {
            out[i] = (byte) readU8();
        }
        return out;
    }

    public long peekBits(int n) {
        int saved = pos;
        long val = readBits(n);
        pos = saved;
        return val;
    }

    public void alignToByte() {
        int rem = pos & 7;
        if (rem != 0) {
            pos += 8 - rem;
        }
    }
}
