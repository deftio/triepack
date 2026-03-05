/*
 * Values.kt
 *
 * Value encoder/decoder for 8 type tags -- matches src/core/value.c.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

// Type tags (4-bit, matches tp_value_type enum)
const val TP_NULL: Int = 0
const val TP_BOOL: Int = 1
const val TP_INT: Int = 2
const val TP_UINT: Int = 3
const val TP_FLOAT32: Int = 4
const val TP_FLOAT64: Int = 5
const val TP_STRING: Int = 6
const val TP_BLOB: Int = 7

/**
 * Sealed class representing TriePack typed values.
 */
sealed class TpValue {
    object Null : TpValue() {
        override fun toString(): String = "TpValue.Null"
    }

    data class Bool(val value: Boolean) : TpValue()
    data class Int(val value: Long) : TpValue()
    data class UInt(val value: Long) : TpValue()
    data class Float64(val value: Double) : TpValue()
    data class Str(val value: String) : TpValue()
    data class Blob(val value: ByteArray) : TpValue() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Blob) return false
            return value.contentEquals(other.value)
        }

        override fun hashCode(): Int = value.contentHashCode()
    }
}

/**
 * Encode a TpValue into the bitstream.
 */
fun encodeValue(writer: BitWriter, value: TpValue?) {
    if (value == null || value is TpValue.Null) {
        writer.writeBits(TP_NULL.toLong(), 4)
        return
    }

    when (value) {
        is TpValue.Bool -> {
            writer.writeBits(TP_BOOL.toLong(), 4)
            writer.writeBit(if (value.value) 1 else 0)
        }
        is TpValue.Int -> {
            writer.writeBits(TP_INT.toLong(), 4)
            writeVarInt(writer, value.value)
        }
        is TpValue.UInt -> {
            writer.writeBits(TP_UINT.toLong(), 4)
            writeVarUint(writer, value.value)
        }
        is TpValue.Float64 -> {
            writer.writeBits(TP_FLOAT64.toLong(), 4)
            val bits = java.lang.Double.doubleToLongBits(value.value)
            val hi = (bits ushr 32) and 0xFFFFFFFFL
            val lo = bits and 0xFFFFFFFFL
            writer.writeU64(hi, lo)
        }
        is TpValue.Str -> {
            val encoded = value.value.toByteArray(Charsets.UTF_8)
            writer.writeBits(TP_STRING.toLong(), 4)
            writeVarUint(writer, encoded.size.toLong())
            writer.alignToByte()
            writer.writeBytes(encoded)
        }
        is TpValue.Blob -> {
            writer.writeBits(TP_BLOB.toLong(), 4)
            writeVarUint(writer, value.value.size.toLong())
            writer.alignToByte()
            writer.writeBytes(value.value)
        }
        is TpValue.Null -> { /* already handled above */ }
    }
}

/**
 * Decode a value from the bitstream.
 */
fun decodeValue(reader: BitReader): TpValue? {
    val tag = reader.readBits(4).toInt()

    return when (tag) {
        TP_NULL -> null
        TP_BOOL -> TpValue.Bool(reader.readBit() != 0)
        TP_INT -> TpValue.Int(readVarInt(reader))
        TP_UINT -> TpValue.UInt(readVarUint(reader))
        TP_FLOAT32 -> {
            val bits = reader.readU32()
            val floatVal = java.lang.Float.intBitsToFloat(bits.toInt())
            TpValue.Float64(floatVal.toDouble())
        }
        TP_FLOAT64 -> {
            val hi = reader.readU32()
            val lo = reader.readU32()
            val bits = (hi shl 32) or lo
            TpValue.Float64(java.lang.Double.longBitsToDouble(bits))
        }
        TP_STRING -> {
            val sLen = readVarUint(reader).toInt()
            reader.alignToByte()
            val raw = reader.readBytes(sLen)
            TpValue.Str(String(raw, Charsets.UTF_8))
        }
        TP_BLOB -> {
            val bLen = readVarUint(reader).toInt()
            reader.alignToByte()
            TpValue.Blob(reader.readBytes(bLen))
        }
        else -> throw IllegalArgumentException("Unknown value type tag: $tag")
    }
}
