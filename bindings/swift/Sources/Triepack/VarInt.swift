// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// LEB128 unsigned VarInt + zigzag signed VarInt.
enum VarInt {
    private static let maxGroups = 10

    /// Write an unsigned LEB128 VarInt.
    static func writeUInt(_ writer: BitWriter, _ value: UInt64) {
        var v = value
        while true {
            var byte = UInt8(v & 0x7F)
            v >>= 7
            if v > 0 {
                byte |= 0x80
            }
            writer.writeU8(byte)
            if v == 0 { break }
        }
    }

    /// Read an unsigned LEB128 VarInt.
    static func readUInt(_ reader: BitReader) throws -> UInt64 {
        var val: UInt64 = 0
        var shift: UInt64 = 0
        for _ in 0..<maxGroups {
            let byte = try reader.readU8()
            val |= UInt64(byte & 0x7F) << shift
            if (byte & 0x80) == 0 {
                return val
            }
            shift += 7
        }
        throw TriepackError.overflow
    }

    /// Write a signed zigzag VarInt.
    ///
    /// Zigzag is computed on the bit pattern: negating traps at Int64.min, and
    /// doubling traps near Int64.max.
    static func writeInt(_ writer: BitWriter, _ value: Int64) {
        let raw = (UInt64(bitPattern: value) << 1) ^ UInt64(bitPattern: value >> 63)
        writeUInt(writer, raw)
    }

    /// Read a signed zigzag VarInt.
    static func readInt(_ reader: BitReader) throws -> Int64 {
        let raw = try readUInt(reader)
        return Int64(bitPattern: raw >> 1) ^ -Int64(raw & 1)
    }

    /// Return the number of bits needed to encode val as a VarInt.
    static func uintBits(_ val: UInt64) -> Int {
        var bits = 0
        var v = val
        while true {
            bits += 8
            v >>= 7
            if v == 0 { break }
        }
        return bits
    }
}
