/*
 * BitWriter.java
 *
 * Arbitrary-width bit I/O writer -- MSB-first, matching src/bitstream/.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

/**
 * Write arbitrary-width bit fields into a growable buffer (MSB-first).
 */
public class BitWriter {

    private byte[] buf;
    private int pos; // bit position

    public BitWriter() {
        this(256);
    }

    public BitWriter(int initialCap) {
        if (initialCap < 1) {
            initialCap = 256;
        }
        this.buf = new byte[initialCap];
        this.pos = 0;
    }

    public int getPosition() {
        return pos;
    }

    private void ensure(int nBits) {
        int needed = (pos + nBits + 7) >>> 3;
        if (needed <= buf.length) {
            return;
        }
        int newCap = buf.length * 2;
        while (newCap < needed) {
            newCap *= 2;
        }
        byte[] newBuf = new byte[newCap];
        System.arraycopy(buf, 0, newBuf, 0, buf.length);
        buf = newBuf;
    }

    public void writeBits(long value, int n) {
        if (n < 1 || n > 64) {
            throw new IllegalArgumentException("writeBits: n must be 1..64");
        }
        ensure(n);
        for (int i = 0; i < n; i++) {
            int bit = (int) ((value >>> (n - 1 - i)) & 1);
            int byteIdx = pos >>> 3;
            int bitIdx = 7 - (pos & 7);
            if (bit != 0) {
                buf[byteIdx] |= (byte) (1 << bitIdx);
            } else {
                buf[byteIdx] &= (byte) ~(1 << bitIdx);
            }
            pos++;
        }
    }

    public void writeBit(int val) {
        ensure(1);
        int byteIdx = pos >>> 3;
        int bitIdx = 7 - (pos & 7);
        if (val != 0) {
            buf[byteIdx] |= (byte) (1 << bitIdx);
        } else {
            buf[byteIdx] &= (byte) ~(1 << bitIdx);
        }
        pos++;
    }

    public void writeU8(int val) {
        writeBits(val & 0xFFL, 8);
    }

    public void writeU16(int val) {
        writeBits(val & 0xFFFFL, 16);
    }

    public void writeU32(long val) {
        writeBits(val & 0xFFFFFFFFL, 32);
    }

    public void writeU64Parts(long hi32, long lo32) {
        writeBits(hi32 & 0xFFFFFFFFL, 32);
        writeBits(lo32 & 0xFFFFFFFFL, 32);
    }

    public void writeBytes(byte[] data) {
        for (byte b : data) {
            writeU8(b & 0xFF);
        }
    }

    public void alignToByte() {
        int rem = pos & 7;
        if (rem != 0) {
            int pad = 8 - rem;
            ensure(pad);
            pos += pad;
        }
    }

    public byte[] toByteArray() {
        int length = (pos + 7) >>> 3;
        byte[] result = new byte[length];
        System.arraycopy(buf, 0, result, 0, length);
        return result;
    }

    /**
     * Returns the internal buffer directly (for patching).
     */
    public byte[] getBuffer() {
        return buf;
    }
}
