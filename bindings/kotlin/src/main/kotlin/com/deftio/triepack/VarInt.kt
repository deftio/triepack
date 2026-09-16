/*
 * VarInt.kt
 *
 * LEB128 unsigned VarInt + zigzag signed VarInt -- matches bitstream_varint.c.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

private const val VARINT_MAX_GROUPS = 10

/**
 * Write an unsigned LEB128 VarInt.
 */
fun writeVarUint(writer: BitWriter, value: Long) {
    // The Long carries a uint64 bit pattern, so a "negative" value is just one
    // above Long.MAX_VALUE. Shift and test unsigned throughout.
    var v = value
    do {
        var byte = (v and 0x7FL).toInt()
        v = v ushr 7
        if (v != 0L) byte = byte or 0x80
        writer.writeU8(byte)
    } while (v != 0L)
}

/**
 * Read an unsigned LEB128 VarInt.
 */
fun readVarUint(reader: BitReader): Long {
    var result = 0L
    var shift = 0
    for (i in 0 until VARINT_MAX_GROUPS) {
        val byte = reader.readU8()
        result = result or (((byte and 0x7F).toLong()) shl shift)
        if ((byte and 0x80) == 0) return result
        shift += 7
    }
    throw ArithmeticException("VarInt overflow")
}

/**
 * Write a signed zigzag VarInt.
 */
fun writeVarInt(writer: BitWriter, value: Long) {
    // Zigzag on the bit pattern: negating overflows at Long.MIN_VALUE and
    // doubling overflows near Long.MAX_VALUE, both silently.
    val raw = (value shl 1) xor (value shr 63)
    writeVarUint(writer, raw)
}

/**
 * Read a signed zigzag VarInt.
 */
fun readVarInt(reader: BitReader): Long {
    val raw = readVarUint(reader)
    return (raw ushr 1) xor -(raw and 1L)
}

/**
 * Return the number of bits needed to encode val as a VarInt.
 */
fun varUintBits(value: Long): Int {
    var bits = 0
    var v = value
    do {
        bits += 8
        v = v ushr 7
    } while (v != 0L)
    return bits
}
