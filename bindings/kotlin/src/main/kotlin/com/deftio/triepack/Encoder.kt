/*
 * Encoder.kt
 *
 * Trie encoder -- port of src/core/encoder.c (via Python encoder.py).
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

// Format constants (match core_internal.h)
internal val TP_MAGIC = byteArrayOf(0x54, 0x52, 0x50, 0x00) // "TRP\0"
internal const val TP_VERSION_MAJOR = 1
internal const val TP_VERSION_MINOR = 0
internal const val TP_HEADER_SIZE = 32
internal const val TP_FLAG_HAS_VALUES = 1

// Control code indices
internal const val CTRL_END = 0
internal const val CTRL_END_VAL = 1
internal const val CTRL_SKIP = 2
// internal const val CTRL_SUFFIX = 3
// internal const val CTRL_ESCAPE = 4
internal const val CTRL_BRANCH = 5
internal const val NUM_CONTROL_CODES = 6

/**
 * Internal context passed through trie encoding.
 */
private class EncodeContext(
    val entries: List<Pair<ByteArray, TpValue?>>,
    val bps: Int,
    val symbolMap: IntArray,
    val ctrlCodes: IntArray,
    val hasValues: Boolean
)

/**
 * Encode a map into the .trp binary format. Returns ByteArray.
 */
fun encode(data: Map<String, TpValue?>): ByteArray {
    // Sort lexicographically by key bytes (unsigned byte comparison)
    val sortedEntries = data.map { (k, v) ->
        Pair(k.toByteArray(Charsets.UTF_8), v)
    }.sortedWith(Comparator { a, b ->
        val aBytes = a.first
        val bBytes = b.first
        val minLen = minOf(aBytes.size, bBytes.size)
        for (i in 0 until minLen) {
            val cmp = (aBytes[i].toInt() and 0xFF) - (bBytes[i].toInt() and 0xFF)
            if (cmp != 0) return@Comparator cmp
        }
        aBytes.size - bBytes.size
    })

    // Determine if any entries have non-null values
    val hasValues = sortedEntries.any { (_, v) -> v != null && v !is TpValue.Null }

    // Symbol analysis
    val used = BooleanArray(256)
    for ((kb, _) in sortedEntries) {
        for (b in kb) {
            used[b.toInt() and 0xFF] = true
        }
    }

    val alphabetSize = used.count { it }
    val totalSymbols = alphabetSize + NUM_CONTROL_CODES
    var bps = 1
    while ((1 shl bps) < totalSymbols) {
        bps++
    }

    // Control codes: 0..5
    val ctrlCodes = IntArray(NUM_CONTROL_CODES) { it }

    // Symbol map: byte value -> N-bit code
    val symbolMap = IntArray(256)
    val reverseMap = IntArray(256)
    var code = NUM_CONTROL_CODES
    for (i in 0 until 256) {
        if (used[i]) {
            symbolMap[i] = code
            if (code < 256) {
                reverseMap[code] = i
            }
            code++
        }
    }

    // Build the bitstream
    val w = BitWriter(256)

    // Write 32-byte header placeholder
    for (b in TP_MAGIC) {
        w.writeU8(b.toInt() and 0xFF)
    }
    w.writeU8(TP_VERSION_MAJOR)
    w.writeU8(TP_VERSION_MINOR)
    val flags = if (hasValues) TP_FLAG_HAS_VALUES else 0
    w.writeU16(flags)
    w.writeU32(sortedEntries.size.toLong())   // num_keys
    w.writeU32(0)                              // trie_data_offset placeholder
    w.writeU32(0)                              // value_store_offset placeholder
    w.writeU32(0)                              // suffix_table_offset placeholder
    w.writeU32(0)                              // total_data_bits placeholder
    w.writeU32(0)                              // reserved

    val dataStart = w.position  // should be 256 (32 bytes * 8)

    // Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
    w.writeBits(bps.toLong(), 4)
    w.writeBits(totalSymbols.toLong(), 8)

    // Control code mappings
    for (c in 0 until NUM_CONTROL_CODES) {
        w.writeBits(ctrlCodes[c].toLong(), bps)
    }

    // Symbol table
    for (cd in NUM_CONTROL_CODES until totalSymbols) {
        val byteVal = if (cd < 256) reverseMap[cd] else 0
        writeVarUint(w, byteVal.toLong())
    }

    val trieDataOffset = w.position - dataStart

    // Write prefix trie
    val ctx = EncodeContext(sortedEntries, bps, symbolMap, ctrlCodes, hasValues)
    val valueIdx = intArrayOf(0)
    if (sortedEntries.isNotEmpty()) {
        trieWrite(ctx, w, 0, sortedEntries.size, 0, valueIdx)
    }

    val valueStoreOffset = w.position - dataStart

    // Write value store
    if (hasValues) {
        for ((_, v) in sortedEntries) {
            encodeValue(w, v)
        }
    }

    val totalDataBits = w.position - dataStart

    w.alignToByte()

    // Get pre-CRC buffer
    val preCrcBuf = w.toByteArray()
    val crcVal = Crc32.compute(preCrcBuf)

    // Append CRC
    w.writeU32(crcVal)

    val outBuf = w.toByteArray().copyOf()

    // Patch header offsets
    patchU32(outBuf, 12, trieDataOffset.toLong())
    patchU32(outBuf, 16, valueStoreOffset.toLong())
    patchU32(outBuf, 20, 0)  // suffix_table_offset
    patchU32(outBuf, 24, totalDataBits.toLong())

    // Recompute CRC over patched data
    val crcDataLen = outBuf.size - 4
    val finalCrc = Crc32.compute(outBuf, 0, crcDataLen)
    patchU32(outBuf, crcDataLen, finalCrc)

    return outBuf
}

private fun patchU32(buf: ByteArray, offset: Int, value: Long) {
    buf[offset] = ((value ushr 24) and 0xFFL).toByte()
    buf[offset + 1] = ((value ushr 16) and 0xFFL).toByte()
    buf[offset + 2] = ((value ushr 8) and 0xFFL).toByte()
    buf[offset + 3] = (value and 0xFFL).toByte()
}

private fun trieSubtreeSize(
    ctx: EncodeContext,
    start: Int,
    end: Int,
    prefixLen: Int,
    valueIdx: IntArray
): Int {
    val entries = ctx.entries
    val bps = ctx.bps
    val hasValues = ctx.hasValues
    var bits = 0

    // Find common prefix beyond prefixLen
    var common = prefixLen
    while (true) {
        var allHave = true
        var ch = 0
        for (i in start until end) {
            if (entries[i].first.size <= common) {
                allHave = false
                break
            }
            if (i == start) {
                ch = entries[i].first[common].toInt() and 0xFF
            } else if ((entries[i].first[common].toInt() and 0xFF) != ch) {
                allHave = false
                break
            }
        }
        if (!allHave) break
        common++
    }

    bits += (common - prefixLen) * bps

    val hasTerminal = entries[start].first.size == common
    val childrenStart = if (hasTerminal) start + 1 else start

    // Count children
    var childCount = 0
    var cs = childrenStart
    while (cs < end) {
        childCount++
        val ch = entries[cs].first[common].toInt() and 0xFF
        var ce = cs + 1
        while (ce < end && (entries[ce].first[common].toInt() and 0xFF) == ch) {
            ce++
        }
        cs = ce
    }

    if (hasTerminal && childCount == 0) {
        if (hasValues && entries[start].second != null && entries[start].second !is TpValue.Null) {
            bits += bps
            bits += varUintBits(valueIdx[0].toLong())
        } else {
            bits += bps
        }
        valueIdx[0]++
    } else if (hasTerminal && childCount > 0) {
        if (hasValues && entries[start].second != null && entries[start].second !is TpValue.Null) {
            bits += bps
            bits += varUintBits(valueIdx[0].toLong())
        } else {
            bits += bps
        }
        valueIdx[0]++
        bits += bps
        bits += varUintBits(childCount.toLong())

        cs = childrenStart
        var childI = 0
        while (cs < end) {
            val ch = entries[cs].first[common].toInt() and 0xFF
            var ce = cs + 1
            while (ce < end && (entries[ce].first[common].toInt() and 0xFF) == ch) {
                ce++
            }
            if (childI < childCount - 1) {
                val childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx)
                bits += bps
                bits += varUintBits(childSz.toLong())
                bits += childSz
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx)
            }
            childI++
            cs = ce
        }
    } else {
        // not hasTerminal, childCount > 0
        bits += bps
        bits += varUintBits(childCount.toLong())

        cs = childrenStart
        var childI = 0
        while (cs < end) {
            val ch = entries[cs].first[common].toInt() and 0xFF
            var ce = cs + 1
            while (ce < end && (entries[ce].first[common].toInt() and 0xFF) == ch) {
                ce++
            }
            if (childI < childCount - 1) {
                val childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx)
                bits += bps
                bits += varUintBits(childSz.toLong())
                bits += childSz
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx)
            }
            childI++
            cs = ce
        }
    }

    return bits
}

private fun trieWrite(
    ctx: EncodeContext,
    w: BitWriter,
    start: Int,
    end: Int,
    prefixLen: Int,
    valueIdx: IntArray
) {
    val entries = ctx.entries
    val bps = ctx.bps
    val symbolMap = ctx.symbolMap
    val ctrlCodes = ctx.ctrlCodes
    val hasValues = ctx.hasValues

    // Find common prefix beyond prefixLen
    var common = prefixLen
    while (true) {
        var allHave = true
        var ch = 0
        for (i in start until end) {
            if (entries[i].first.size <= common) {
                allHave = false
                break
            }
            if (i == start) {
                ch = entries[i].first[common].toInt() and 0xFF
            } else if ((entries[i].first[common].toInt() and 0xFF) != ch) {
                allHave = false
                break
            }
        }
        if (!allHave) break
        common++
    }

    // Write common prefix symbols
    for (i in prefixLen until common) {
        val ch = entries[start].first[i].toInt() and 0xFF
        w.writeBits(symbolMap[ch].toLong(), bps)
    }

    val hasTerminal = entries[start].first.size == common
    val childrenStart = if (hasTerminal) start + 1 else start

    // Count children
    var childCount = 0
    var cs = childrenStart
    while (cs < end) {
        childCount++
        val ch = entries[cs].first[common].toInt() and 0xFF
        var ce = cs + 1
        while (ce < end && (entries[ce].first[common].toInt() and 0xFF) == ch) {
            ce++
        }
        cs = ce
    }

    // Write terminal
    if (hasTerminal) {
        if (hasValues && entries[start].second != null && entries[start].second !is TpValue.Null) {
            w.writeBits(ctrlCodes[CTRL_END_VAL].toLong(), bps)
            writeVarUint(w, valueIdx[0].toLong())
        } else {
            w.writeBits(ctrlCodes[CTRL_END].toLong(), bps)
        }
        valueIdx[0]++
    }

    if (childCount == 0) return

    // BRANCH
    w.writeBits(ctrlCodes[CTRL_BRANCH].toLong(), bps)
    writeVarUint(w, childCount.toLong())

    cs = childrenStart
    var childI = 0
    while (cs < end) {
        val ch = entries[cs].first[common].toInt() and 0xFF
        var ce = cs + 1
        while (ce < end && (entries[ce].first[common].toInt() and 0xFF) == ch) {
            ce++
        }

        if (childI < childCount - 1) {
            val viCopy = intArrayOf(valueIdx[0])
            val childSz = trieSubtreeSize(ctx, cs, ce, common, viCopy)
            w.writeBits(ctrlCodes[CTRL_SKIP].toLong(), bps)
            writeVarUint(w, childSz.toLong())
        }

        trieWrite(ctx, w, cs, ce, common, valueIdx)

        childI++
        cs = ce
    }
}
