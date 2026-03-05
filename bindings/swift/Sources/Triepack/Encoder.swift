// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Format constants matching core_internal.h.
private let tpMagic: [UInt8] = [0x54, 0x52, 0x50, 0x00]
private let tpVersionMajor: UInt8 = 1
private let tpVersionMinor: UInt8 = 0
private let tpHeaderSize = 32
private let tpFlagHasValues: UInt16 = 1

/// Control code indices.
private let ctrlEnd = 0
private let ctrlEndVal = 1
private let ctrlSkip = 2
// private let ctrlSuffix = 3
// private let ctrlEscape = 4
private let ctrlBranch = 5
private let numControlCodes = 6

/// Internal context for trie encoding.
private struct EncodeContext {
    let entries: [([UInt8], TriepackValue)]
    let bps: Int
    let symbolMap: [Int]
    let ctrlCodes: [Int]
    let hasValues: Bool
}

/// Encode a dictionary into the .trp binary format.
func triepackEncode(_ data: [String: TriepackValue]) throws -> Data {
    // Collect entries: (keyBytes, value)
    var entries: [([UInt8], TriepackValue)] = []
    for (k, v) in data {
        let keyBytes = [UInt8](k.utf8)
        entries.append((keyBytes, v))
    }

    // Sort lexicographically by key bytes
    entries.sort { a, b in
        let minLen = min(a.0.count, b.0.count)
        for i in 0..<minLen {
            if a.0[i] != b.0[i] {
                return a.0[i] < b.0[i]
            }
        }
        return a.0.count < b.0.count
    }

    // Determine if any entries have non-null values
    var hasValues = false
    for (_, v) in entries {
        if case .null = v {
            // skip
        } else {
            hasValues = true
            break
        }
    }

    // Symbol analysis
    var used = [Bool](repeating: false, count: 256)
    for (kb, _) in entries {
        for b in kb {
            used[Int(b)] = true
        }
    }

    let alphabetSize = used.filter { $0 }.count
    let totalSymbols = alphabetSize + numControlCodes
    var bps = 1
    while (1 << bps) < totalSymbols {
        bps += 1
    }

    // Control codes: 0..5
    let ctrlCodes = Array(0..<numControlCodes)

    // Symbol map: byte value -> N-bit code
    var symbolMap = [Int](repeating: 0, count: 256)
    var reverseMap = [Int](repeating: 0, count: 256)
    var code = numControlCodes
    for i in 0..<256 {
        if used[i] {
            symbolMap[i] = code
            if code < 256 {
                reverseMap[code] = i
            }
            code += 1
        }
    }

    // Build the bitstream
    let w = BitWriter(initialCapacity: 256)

    // Write 32-byte header placeholder
    for b in tpMagic {
        w.writeU8(b)
    }
    w.writeU8(tpVersionMajor)
    w.writeU8(tpVersionMinor)
    let flags: UInt16 = hasValues ? tpFlagHasValues : 0
    w.writeU16(flags)
    w.writeU32(UInt32(entries.count))  // num_keys
    w.writeU32(0)  // trie_data_offset placeholder
    w.writeU32(0)  // value_store_offset placeholder
    w.writeU32(0)  // suffix_table_offset placeholder
    w.writeU32(0)  // total_data_bits placeholder
    w.writeU32(0)  // reserved

    let dataStart = w.position  // should be 256 (32 bytes * 8)

    // Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
    w.writeBits(value: UInt64(bps), count: 4)
    w.writeBits(value: UInt64(totalSymbols), count: 8)

    // Control code mappings
    for c in 0..<numControlCodes {
        w.writeBits(value: UInt64(ctrlCodes[c]), count: bps)
    }

    // Symbol table
    for cd in numControlCodes..<totalSymbols {
        let byteVal = cd < 256 ? reverseMap[cd] : 0
        VarInt.writeUInt(w, UInt64(byteVal))
    }

    let trieDataOffset = w.position - dataStart

    // Write prefix trie
    let ctx = EncodeContext(
        entries: entries,
        bps: bps,
        symbolMap: symbolMap,
        ctrlCodes: ctrlCodes,
        hasValues: hasValues
    )
    var valueIdx = 0
    if !entries.isEmpty {
        trieWrite(ctx, w, 0, entries.count, 0, &valueIdx)
    }

    let valueStoreOffset = w.position - dataStart

    // Write value store
    if hasValues {
        for (_, val) in entries {
            encodeValue(w, val)
        }
    }

    let totalDataBits = w.position - dataStart

    w.alignToByte()

    // Get pre-CRC buffer
    let preCrcData = w.toData()
    let crcVal = CRC32.compute(preCrcData)

    // Append CRC
    w.writeU32(crcVal)

    var outBuf = [UInt8](w.toData())

    // Patch header offsets (big-endian u32 at byte offsets 12, 16, 20, 24)
    patchU32(&outBuf, 12, UInt32(trieDataOffset))
    patchU32(&outBuf, 16, UInt32(valueStoreOffset))
    patchU32(&outBuf, 20, 0)  // suffix_table_offset
    patchU32(&outBuf, 24, UInt32(totalDataBits))

    // Recompute CRC over patched data
    let crcDataLen = outBuf.count - 4
    let newCrc = CRC32.compute(Array(outBuf[0..<crcDataLen]))
    patchU32(&outBuf, crcDataLen, newCrc)

    return Data(outBuf)
}

private func patchU32(_ buf: inout [UInt8], _ offset: Int, _ value: UInt32) {
    buf[offset] = UInt8((value >> 24) & 0xFF)
    buf[offset + 1] = UInt8((value >> 16) & 0xFF)
    buf[offset + 2] = UInt8((value >> 8) & 0xFF)
    buf[offset + 3] = UInt8(value & 0xFF)
}

/// Compute subtree size in bits (dry run) without writing.
private func trieSubtreeSize(
    _ ctx: EncodeContext,
    _ start: Int,
    _ end: Int,
    _ prefixLen: Int,
    _ valueIdx: inout Int
) -> Int {
    let entries = ctx.entries
    let bps = ctx.bps
    let hasValues = ctx.hasValues
    var bits = 0

    // Find common prefix beyond prefixLen
    var common = prefixLen
    while true {
        var allHave = true
        var ch: UInt8 = 0
        for i in start..<end {
            if entries[i].0.count <= common {
                allHave = false
                break
            }
            if i == start {
                ch = entries[i].0[common]
            } else if entries[i].0[common] != ch {
                allHave = false
                break
            }
        }
        if !allHave { break }
        common += 1
    }

    bits += (common - prefixLen) * bps

    let hasTerminal = entries[start].0.count == common
    let childrenStart = hasTerminal ? start + 1 : start

    // Count children
    var childCount = 0
    var cs = childrenStart
    while cs < end {
        childCount += 1
        let ch = entries[cs].0[common]
        var ce = cs + 1
        while ce < end && entries[ce].0[common] == ch {
            ce += 1
        }
        cs = ce
    }

    if hasTerminal && childCount == 0 {
        if hasValues {
            if case .null = entries[start].1 {
                bits += bps
            } else {
                bits += bps
                bits += VarInt.uintBits(UInt64(valueIdx))
            }
        } else {
            bits += bps
        }
        valueIdx += 1
    } else if hasTerminal && childCount > 0 {
        if hasValues {
            if case .null = entries[start].1 {
                bits += bps
            } else {
                bits += bps
                bits += VarInt.uintBits(UInt64(valueIdx))
            }
        } else {
            bits += bps
        }
        valueIdx += 1

        bits += bps  // BRANCH
        bits += VarInt.uintBits(UInt64(childCount))

        cs = childrenStart
        var childI = 0
        while cs < end {
            let ch = entries[cs].0[common]
            var ce = cs + 1
            while ce < end && entries[ce].0[common] == ch {
                ce += 1
            }
            if childI < childCount - 1 {
                // In subtree size, advance valueIdx through non-last children
                // (copy is only used in trieWrite, not here)
                let childSz = trieSubtreeSize(ctx, cs, ce, common, &valueIdx)
                bits += bps  // SKIP
                bits += VarInt.uintBits(UInt64(childSz))
                bits += childSz
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, &valueIdx)
            }
            childI += 1
            cs = ce
        }
    } else {
        // not hasTerminal, childCount > 0
        bits += bps  // BRANCH
        bits += VarInt.uintBits(UInt64(childCount))

        cs = childrenStart
        var childI = 0
        while cs < end {
            let ch = entries[cs].0[common]
            var ce = cs + 1
            while ce < end && entries[ce].0[common] == ch {
                ce += 1
            }
            if childI < childCount - 1 {
                // In subtree size, advance valueIdx through non-last children
                let childSz = trieSubtreeSize(ctx, cs, ce, common, &valueIdx)
                bits += bps  // SKIP
                bits += VarInt.uintBits(UInt64(childSz))
                bits += childSz
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, &valueIdx)
            }
            childI += 1
            cs = ce
        }
    }

    return bits
}

/// Write the trie subtree.
private func trieWrite(
    _ ctx: EncodeContext,
    _ w: BitWriter,
    _ start: Int,
    _ end: Int,
    _ prefixLen: Int,
    _ valueIdx: inout Int
) {
    let entries = ctx.entries
    let bps = ctx.bps
    let symbolMap = ctx.symbolMap
    let ctrlCodes = ctx.ctrlCodes
    let hasValues = ctx.hasValues

    // Find common prefix beyond prefixLen
    var common = prefixLen
    while true {
        var allHave = true
        var ch: UInt8 = 0
        for i in start..<end {
            if entries[i].0.count <= common {
                allHave = false
                break
            }
            if i == start {
                ch = entries[i].0[common]
            } else if entries[i].0[common] != ch {
                allHave = false
                break
            }
        }
        if !allHave { break }
        common += 1
    }

    // Write common prefix symbols
    for i in prefixLen..<common {
        let ch = entries[start].0[i]
        w.writeBits(value: UInt64(symbolMap[Int(ch)]), count: bps)
    }

    let hasTerminal = entries[start].0.count == common
    let childrenStart = hasTerminal ? start + 1 : start

    // Count children
    var childCount = 0
    var cs = childrenStart
    while cs < end {
        childCount += 1
        let ch = entries[cs].0[common]
        var ce = cs + 1
        while ce < end && entries[ce].0[common] == ch {
            ce += 1
        }
        cs = ce
    }

    // Write terminal
    if hasTerminal {
        if hasValues {
            if case .null = entries[start].1 {
                w.writeBits(value: UInt64(ctrlCodes[ctrlEnd]), count: bps)
            } else {
                w.writeBits(value: UInt64(ctrlCodes[ctrlEndVal]), count: bps)
                VarInt.writeUInt(w, UInt64(valueIdx))
            }
        } else {
            w.writeBits(value: UInt64(ctrlCodes[ctrlEnd]), count: bps)
        }
        valueIdx += 1
    }

    if childCount == 0 { return }

    // BRANCH
    w.writeBits(value: UInt64(ctrlCodes[ctrlBranch]), count: bps)
    VarInt.writeUInt(w, UInt64(childCount))

    cs = childrenStart
    var childI = 0
    while cs < end {
        let ch = entries[cs].0[common]
        var ce = cs + 1
        while ce < end && entries[ce].0[common] == ch {
            ce += 1
        }

        if childI < childCount - 1 {
            var viCopy = valueIdx
            let childSz = trieSubtreeSize(ctx, cs, ce, common, &viCopy)
            w.writeBits(value: UInt64(ctrlCodes[ctrlSkip]), count: bps)
            VarInt.writeUInt(w, UInt64(childSz))
        }

        trieWrite(ctx, w, cs, ce, common, &valueIdx)

        childI += 1
        cs = ce
    }
}
