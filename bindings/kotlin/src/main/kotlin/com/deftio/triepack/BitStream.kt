/*
 * BitStream.kt
 *
 * Arbitrary-width bit I/O -- MSB-first, matching src/bitstream/.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

/**
 * Write arbitrary-width bit fields into a growable buffer.
 */
class BitWriter(initialCap: Int = 256) {
    private var buf: ByteArray = ByteArray(maxOf(initialCap, 256))
    private var pos: Int = 0

    val position: Int get() = pos

    private fun ensure(nBits: Int) {
        val needed = (pos + nBits + 7) ushr 3
        if (needed <= buf.size) return
        var newCap = buf.size * 2
        while (newCap < needed) newCap *= 2
        buf = buf.copyOf(newCap)
    }

    fun writeBits(value: Long, n: Int) {
        require(n in 1..64) { "writeBits: n must be 1..64" }
        ensure(n)
        for (i in 0 until n) {
            val bit = ((value ushr (n - 1 - i)) and 1L).toInt()
            val byteIdx = pos ushr 3
            val bitIdx = 7 - (pos and 7)
            if (bit != 0) {
                buf[byteIdx] = (buf[byteIdx].toInt() or (1 shl bitIdx)).toByte()
            } else {
                buf[byteIdx] = (buf[byteIdx].toInt() and (1 shl bitIdx).inv()).toByte()
            }
            pos++
        }
    }

    fun writeBit(v: Int) {
        ensure(1)
        val byteIdx = pos ushr 3
        val bitIdx = 7 - (pos and 7)
        if (v != 0) {
            buf[byteIdx] = (buf[byteIdx].toInt() or (1 shl bitIdx)).toByte()
        } else {
            buf[byteIdx] = (buf[byteIdx].toInt() and (1 shl bitIdx).inv()).toByte()
        }
        pos++
    }

    fun writeU8(v: Int) {
        writeBits((v and 0xFF).toLong(), 8)
    }

    fun writeU16(v: Int) {
        writeBits((v and 0xFFFF).toLong(), 16)
    }

    fun writeU32(v: Long) {
        writeBits(v and 0xFFFFFFFFL, 32)
    }

    fun writeU64(hi32: Long, lo32: Long) {
        writeBits(hi32 and 0xFFFFFFFFL, 32)
        writeBits(lo32 and 0xFFFFFFFFL, 32)
    }

    fun writeBytes(data: ByteArray) {
        for (b in data) {
            writeU8(b.toInt() and 0xFF)
        }
    }

    fun alignToByte() {
        val rem = pos and 7
        if (rem != 0) {
            val pad = 8 - rem
            ensure(pad)
            // Zero the padding bits
            for (i in 0 until pad) {
                val byteIdx = pos ushr 3
                val bitIdx = 7 - (pos and 7)
                buf[byteIdx] = (buf[byteIdx].toInt() and (1 shl bitIdx).inv()).toByte()
                pos++
            }
        }
    }

    fun toByteArray(): ByteArray {
        val length = (pos + 7) ushr 3
        return buf.copyOf(length)
    }

    fun getBuffer(): ByteArray = buf
}

/**
 * Read arbitrary-width bit fields from a buffer.
 */
class BitReader(private val buf: ByteArray) {
    private val bitLen: Int = buf.size * 8
    private var pos: Int = 0

    val position: Int get() = pos
    val remaining: Int get() = bitLen - pos
    val totalBitLength: Int get() = bitLen

    fun seek(bitPos: Int) {
        pos = bitPos
    }

    fun advance(nBits: Int) {
        pos += nBits
    }

    fun readBits(n: Int): Long {
        require(n in 1..64) { "readBits: n must be 1..64" }
        if (pos + n > bitLen) throw RuntimeException("EOF")
        var v = 0L
        for (i in 0 until n) {
            val byteIdx = pos ushr 3
            val bitIdx = 7 - (pos and 7)
            v = (v shl 1) or (((buf[byteIdx].toInt() ushr bitIdx) and 1).toLong())
            pos++
        }
        return v
    }

    fun readBit(): Int {
        if (pos >= bitLen) throw RuntimeException("EOF")
        val byteIdx = pos ushr 3
        val bitIdx = 7 - (pos and 7)
        val bit = (buf[byteIdx].toInt() ushr bitIdx) and 1
        pos++
        return bit
    }

    fun readU8(): Int {
        return readBits(8).toInt()
    }

    fun readU16(): Int {
        return readBits(16).toInt()
    }

    fun readU32(): Long {
        return readBits(32)
    }

    fun readU64(): Long {
        val hi = readU32()
        val lo = readU32()
        return hi * 0x100000000L + lo
    }

    fun readBytes(n: Int): ByteArray {
        val out = ByteArray(n)
        for (i in 0 until n) {
            out[i] = readU8().toByte()
        }
        return out
    }

    fun peekBits(n: Int): Long {
        val saved = pos
        val v = readBits(n)
        pos = saved
        return v
    }

    fun alignToByte() {
        val rem = pos and 7
        if (rem != 0) {
            pos += 8 - rem
        }
    }

    fun isAligned(): Boolean {
        return (pos and 7) == 0
    }
}
