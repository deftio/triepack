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
    require(value >= 0) { "writeVarUint: negative value" }
    var v = value
    do {
        var byte = (v and 0x7FL).toInt()
        v = v ushr 7
        if (v > 0) byte = byte or 0x80
        writer.writeU8(byte)
    } while (v > 0)
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
    val raw = if (value >= 0) {
        value * 2
    } else {
        (-value) * 2 - 1
    }
    writeVarUint(writer, raw)
}

/**
 * Read a signed zigzag VarInt.
 */
fun readVarInt(reader: BitReader): Long {
    val raw = readVarUint(reader)
    return if ((raw and 1L) != 0L) {
        -(raw ushr 1) - 1
    } else {
        raw ushr 1
    }
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
    } while (v > 0)
    return bits
}
