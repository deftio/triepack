// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Format constants.
private let tpMagic: [UInt8] = [0x54, 0x52, 0x50, 0x00]
private let tpHeaderSize = 32
private let tpFlagHasValues: UInt16 = 1
private let numControlCodes = 6

/// Control code indices.
private let ctrlEnd = 0
private let ctrlEndVal = 1
private let ctrlSkip = 2
private let ctrlBranch = 5

/// Decode a .trp binary buffer into a dictionary.
func triepackDecode(_ buffer: Data) throws -> [String: TriepackValue] {
    let buf = [UInt8](buffer)

    guard buf.count >= tpHeaderSize + 4 else {
        throw TriepackError.dataTooShort
    }

    // Validate magic
    for i in 0..<4 {
        guard buf[i] == tpMagic[i] else {
            throw TriepackError.invalidMagic
        }
    }

    // Version
    let versionMajor = buf[4]
    guard versionMajor == 1 else {
        throw TriepackError.unsupportedVersion
    }

    // Flags
    let flags = (UInt16(buf[6]) << 8) | UInt16(buf[7])
    let hasValues = (flags & tpFlagHasValues) != 0

    // num_keys
    let numKeys = (UInt32(buf[8]) << 24) | (UInt32(buf[9]) << 16) |
                  (UInt32(buf[10]) << 8) | UInt32(buf[11])

    // Offsets
    let trieDataOffset = (UInt32(buf[12]) << 24) | (UInt32(buf[13]) << 16) |
                         (UInt32(buf[14]) << 8) | UInt32(buf[15])
    let valueStoreOffset = (UInt32(buf[16]) << 24) | (UInt32(buf[17]) << 16) |
                           (UInt32(buf[18]) << 8) | UInt32(buf[19])

    // CRC verification
    let crcDataLen = buf.count - 4
    let expectedCrc = (UInt32(buf[crcDataLen]) << 24) |
                      (UInt32(buf[crcDataLen + 1]) << 16) |
                      (UInt32(buf[crcDataLen + 2]) << 8) |
                      UInt32(buf[crcDataLen + 3])
    let actualCrc = CRC32.compute(Array(buf[0..<crcDataLen]))
    guard actualCrc == expectedCrc else {
        throw TriepackError.crcMismatch
    }

    if numKeys == 0 {
        return [:]
    }

    // Parse trie config
    let reader = BitReader(buf)
    let dataStart = tpHeaderSize * 8

    reader.seek(dataStart)
    let bps = Int(try reader.readBits(4))
    let symbolCount = Int(try reader.readBits(8))

    // Read control codes
    var ctrlCodes = [Int](repeating: 0, count: numControlCodes)
    var codeIsCtrl = [Bool](repeating: false, count: 256)
    for c in 0..<numControlCodes {
        let code = Int(try reader.readBits(bps))
        ctrlCodes[c] = code
        if code < 256 {
            codeIsCtrl[code] = true
        }
    }

    // Read symbol table
    var reverseMap = [Int](repeating: 0, count: 256)
    for cd in numControlCodes..<symbolCount {
        let cp = Int(try VarInt.readUInt(reader))
        if cd < 256 && cp < 256 {
            reverseMap[cd] = cp
        }
    }

    let trieStart = dataStart + Int(trieDataOffset)
    let valueStart = dataStart + Int(valueStoreOffset)

    // DFS iteration
    var result: [(String, TriepackValue)] = []
    var keyStack: [UInt8] = []

    func dfsWalk(_ r: BitReader) throws {
        while true {
            if r.position >= r.bitLen {
                return
            }
            let sym = Int(try r.readBits(bps))

            if sym == ctrlCodes[ctrlEnd] {
                let keyStr = String(bytes: keyStack, encoding: .utf8) ?? ""
                result.append((keyStr, .null))

                if r.position + bps <= r.bitLen {
                    let nextSym = Int(try r.peekBits(bps))
                    if nextSym == ctrlCodes[ctrlBranch] {
                        _ = try r.readBits(bps)
                        let childCount = Int(try VarInt.readUInt(r))
                        try walkBranch(r, childCount)
                    }
                }
                return
            }

            if sym == ctrlCodes[ctrlEndVal] {
                _ = try VarInt.readUInt(r)  // value index
                let keyStr = String(bytes: keyStack, encoding: .utf8) ?? ""
                result.append((keyStr, .null))

                if r.position + bps <= r.bitLen {
                    let nextSym = Int(try r.peekBits(bps))
                    if nextSym == ctrlCodes[ctrlBranch] {
                        _ = try r.readBits(bps)
                        let childCount = Int(try VarInt.readUInt(r))
                        try walkBranch(r, childCount)
                    }
                }
                return
            }

            if sym == ctrlCodes[ctrlBranch] {
                let childCount = Int(try VarInt.readUInt(r))
                try walkBranch(r, childCount)
                return
            }

            // Regular symbol
            let byteVal = sym < 256 ? reverseMap[sym] : 0
            keyStack.append(UInt8(byteVal))
        }
    }

    func walkBranch(_ r: BitReader, _ childCount: Int) throws {
        let savedKeyLen = keyStack.count
        for ci in 0..<childCount {
            let hasSkip = ci < childCount - 1
            var skipDist = 0

            if hasSkip {
                _ = try r.readBits(bps)  // SKIP control code
                skipDist = Int(try VarInt.readUInt(r))
            }

            let childStartPos = r.position
            keyStack.removeSubrange(savedKeyLen...)
            try dfsWalk(r)

            if hasSkip {
                r.seek(childStartPos + skipDist)
            }
        }
        keyStack.removeSubrange(savedKeyLen...)
    }

    // Walk the trie
    reader.seek(trieStart)
    if numKeys > 0 {
        try dfsWalk(reader)
    }

    // Build result dict (keys-only first pass)
    var dict = [String: TriepackValue]()
    for (key, val) in result {
        dict[key] = val
    }

    // Decode values
    if hasValues {
        reader.seek(valueStart)
        // Sort keys by their UTF-8 bytes to match the encoding order
        let sortedKeys = result.map { $0.0 }.sorted { a, b in
            let aBytes = [UInt8](a.utf8)
            let bBytes = [UInt8](b.utf8)
            let minLen = min(aBytes.count, bBytes.count)
            for i in 0..<minLen {
                if aBytes[i] != bBytes[i] {
                    return aBytes[i] < bBytes[i]
                }
            }
            return aBytes.count < bBytes.count
        }
        for key in sortedKeys {
            dict[key] = try decodeValue(reader)
        }
    }

    return dict
}
