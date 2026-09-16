/*
 * Decoder.kt
 *
 * Trie decoder -- port of src/core/decoder.c (via Python decoder.py).
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

/**
 * Internal decoder state for DFS trie walk.
 */
private class TrieDecoder(
    private val reader: BitReader,
    private val bps: Int,
    private val ctrlCodes: IntArray,
    private val reverseMap: IntArray
) {
    val result = LinkedHashMap<String, TpValue?>()
    private val keyStack = mutableListOf<Int>()

    /**
     * DFS walk through the trie collecting keys.
     *
     * Every subtree knows where it ends: a child with a SKIP ends at
     * `childStart + skipDist`, the last child ends where its parent does, and
     * the root ends at the start of the value store. [end] is the only
     * authority on whether more symbols belong to this subtree -- the bits
     * that happen to follow are not, because past the last terminal they are
     * padding and CRC.
     */
    fun dfsWalk(end: Int) {
        while (true) {
            if (reader.position >= end) return
            val sym = reader.readBits(bps).toInt()

            if (sym == ctrlCodes[CTRL_END]) {
                val keyStr = String(ByteArray(keyStack.size) { keyStack[it].toByte() }, Charsets.UTF_8)
                result[keyStr] = null
                walkBranchIfPresent(end)
                return
            }

            if (sym == ctrlCodes[CTRL_END_VAL]) {
                readVarUint(reader) // value index
                val keyStr = String(ByteArray(keyStack.size) { keyStack[it].toByte() }, Charsets.UTF_8)
                result[keyStr] = null
                walkBranchIfPresent(end)
                return
            }

            if (sym == ctrlCodes[CTRL_BRANCH]) {
                val childCount = readVarUint(reader).toInt()
                walkBranch(childCount, end)
                return
            }

            // Regular symbol
            val byteVal = if (sym < 256) reverseMap[sym] else 0
            keyStack.add(byteVal)
        }
    }

    /**
     * A terminal is followed by a BRANCH exactly when the subtree has not
     * reached its end -- keys that share this terminal as a prefix.
     */
    private fun walkBranchIfPresent(end: Int) {
        if (reader.position >= end) return
        val sym = reader.readBits(bps).toInt()
        if (sym != ctrlCodes[CTRL_BRANCH]) {
            throw IllegalArgumentException("Malformed trie: expected BRANCH after terminal")
        }
        val childCount = readVarUint(reader).toInt()
        walkBranch(childCount, end)
    }

    private fun walkBranch(childCount: Int, end: Int) {
        val savedKeyLen = keyStack.size
        for (ci in 0 until childCount) {
            val hasSkip = ci < childCount - 1
            var skipDist = 0

            if (hasSkip) {
                reader.readBits(bps) // SKIP control code
                skipDist = readVarUint(reader).toInt()
            }

            val childStartPos = reader.position
            while (keyStack.size > savedKeyLen) {
                keyStack.removeAt(keyStack.size - 1)
            }
            dfsWalk(if (hasSkip) childStartPos + skipDist else end)

            if (hasSkip) {
                reader.seek(childStartPos + skipDist)
            }
        }
        while (keyStack.size > savedKeyLen) {
            keyStack.removeAt(keyStack.size - 1)
        }
    }
}

/**
 * Decode a .trp binary buffer into a map. Returns Map<String, TpValue?>.
 */
fun decode(buffer: ByteArray): Map<String, TpValue?> {
    require(buffer.size >= TP_HEADER_SIZE + 4) { "Data too short for .trp format" }

    // Validate magic
    for (i in 0 until 4) {
        if (buffer[i] != TP_MAGIC[i]) {
            throw IllegalArgumentException("Invalid magic bytes -- not a .trp file")
        }
    }

    // Version
    val versionMajor = buffer[4].toInt() and 0xFF
    if (versionMajor != 1) {
        throw IllegalArgumentException("Unsupported format version: $versionMajor")
    }

    // Flags
    val flags = ((buffer[6].toInt() and 0xFF) shl 8) or (buffer[7].toInt() and 0xFF)
    val hasValues = (flags and TP_FLAG_HAS_VALUES) != 0

    // num_keys
    val numKeys = ((buffer[8].toInt() and 0xFF) shl 24) or
        ((buffer[9].toInt() and 0xFF) shl 16) or
        ((buffer[10].toInt() and 0xFF) shl 8) or
        (buffer[11].toInt() and 0xFF)

    // Offsets
    val trieDataOffset = ((buffer[12].toInt() and 0xFF) shl 24) or
        ((buffer[13].toInt() and 0xFF) shl 16) or
        ((buffer[14].toInt() and 0xFF) shl 8) or
        (buffer[15].toInt() and 0xFF)

    val valueStoreOffset = ((buffer[16].toInt() and 0xFF) shl 24) or
        ((buffer[17].toInt() and 0xFF) shl 16) or
        ((buffer[18].toInt() and 0xFF) shl 8) or
        (buffer[19].toInt() and 0xFF)

    // CRC verification
    val crcDataLen = buffer.size - 4
    val expectedCrc = ((buffer[crcDataLen].toInt() and 0xFF).toLong() shl 24) or
        ((buffer[crcDataLen + 1].toInt() and 0xFF).toLong() shl 16) or
        ((buffer[crcDataLen + 2].toInt() and 0xFF).toLong() shl 8) or
        (buffer[crcDataLen + 3].toInt() and 0xFF).toLong()
    val actualCrc = Crc32.compute(buffer, 0, crcDataLen)
    if (actualCrc != expectedCrc) {
        throw IllegalArgumentException("CRC-32 integrity check failed")
    }

    if (numKeys == 0) {
        return emptyMap()
    }

    // Parse trie config
    val reader = BitReader(buffer)
    val dataStart = TP_HEADER_SIZE * 8

    reader.seek(dataStart)
    val bps = reader.readBits(4).toInt()
    // Symbol codes index 256-entry maps, so a symbol may not be wider than a
    // byte; 0 would make the trie unreadable.
    require(bps in 1..8) { "Invalid trie config: bits_per_symbol must be 1..8, got $bps" }
    val symbolCount = reader.readBits(8).toInt()
    // Must leave room for the control codes and fit in bits_per_symbol.
    require(symbolCount >= NUM_CONTROL_CODES && symbolCount <= (1 shl bps)) {
        "Invalid trie config: symbol_count out of range: $symbolCount"
    }

    // Read control codes
    val ctrlCodes = IntArray(NUM_CONTROL_CODES)
    for (c in 0 until NUM_CONTROL_CODES) {
        ctrlCodes[c] = reader.readBits(bps).toInt()
    }

    // Read symbol table
    val reverseMap = IntArray(256)
    for (cd in NUM_CONTROL_CODES until symbolCount) {
        val cp = readVarUint(reader).toInt()
        if (cd < 256 && cp < 256) {
            reverseMap[cd] = cp
        }
    }

    val trieStart = dataStart + trieDataOffset
    val valueStart = dataStart + valueStoreOffset
    // The trie occupies [trieStart, valueStart); the value store (when
    // present), the byte padding and the CRC follow it.
    val trieEnd = valueStart

    // Walk the trie
    val decoder = TrieDecoder(reader, bps, ctrlCodes, reverseMap)
    reader.seek(trieStart)
    if (numKeys > 0) {
        decoder.dfsWalk(trieEnd)
    }

    val result = decoder.result

    // Decode values
    if (hasValues) {
        reader.seek(valueStart)
        val sortedKeys = result.keys.toList().sortedWith(Comparator { a, b ->
            val aBytes = a.toByteArray(Charsets.UTF_8)
            val bBytes = b.toByteArray(Charsets.UTF_8)
            val minLen = minOf(aBytes.size, bBytes.size)
            for (i in 0 until minLen) {
                val cmp = (aBytes[i].toInt() and 0xFF) - (bBytes[i].toInt() and 0xFF)
                if (cmp != 0) return@Comparator cmp
            }
            aBytes.size - bBytes.size
        })
        for (key in sortedKeys) {
            result[key] = decodeValue(reader)
        }
    }

    return result
}
