// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Type tags for encoded values (4-bit).
enum TypeTag {
    static let null: UInt64 = 0
    static let bool: UInt64 = 1
    static let int: UInt64 = 2
    static let uint: UInt64 = 3
    static let float32: UInt64 = 4
    static let float64: UInt64 = 5
    static let string: UInt64 = 6
    static let blob: UInt64 = 7
}

/// Encode a TriepackValue into the bitstream.
func encodeValue(_ writer: BitWriter, _ val: TriepackValue) {
    switch val {
    case .null:
        writer.writeBits(value: TypeTag.null, count: 4)

    case .bool(let b):
        writer.writeBits(value: TypeTag.bool, count: 4)
        writer.writeBit(b ? 1 : 0)

    case .int(let v):
        writer.writeBits(value: TypeTag.int, count: 4)
        VarInt.writeInt(writer, v)

    case .uint(let v):
        writer.writeBits(value: TypeTag.uint, count: 4)
        VarInt.writeUInt(writer, v)

    case .float64(let v):
        writer.writeBits(value: TypeTag.float64, count: 4)
        let bits = v.bitPattern
        let hi = UInt32(bits >> 32)
        let lo = UInt32(bits & 0xFFFFFFFF)
        writer.writeU64Parts(hi: hi, lo: lo)

    case .string(let s):
        let encoded = [UInt8](s.utf8)
        writer.writeBits(value: TypeTag.string, count: 4)
        VarInt.writeUInt(writer, UInt64(encoded.count))
        writer.alignToByte()
        writer.writeBytes(encoded)

    case .blob(let data):
        writer.writeBits(value: TypeTag.blob, count: 4)
        VarInt.writeUInt(writer, UInt64(data.count))
        writer.alignToByte()
        writer.writeBytes(data)
    }
}

/// Decode a TriepackValue from the bitstream.
func decodeValue(_ reader: BitReader) throws -> TriepackValue {
    let tag = try reader.readBits(4)

    switch tag {
    case TypeTag.null:
        return .null

    case TypeTag.bool:
        let bit = try reader.readBit()
        return .bool(bit != 0)

    case TypeTag.int:
        let v = try VarInt.readInt(reader)
        return .int(v)

    case TypeTag.uint:
        let v = try VarInt.readUInt(reader)
        return .uint(v)

    case TypeTag.float32:
        let bits = try reader.readU32()
        let f = Float(bitPattern: bits)
        return .float64(Double(f))

    case TypeTag.float64:
        let hi = UInt64(try reader.readU32())
        let lo = UInt64(try reader.readU32())
        let bits = (hi << 32) | lo
        let d = Double(bitPattern: bits)
        return .float64(d)

    case TypeTag.string:
        let slen = try VarInt.readUInt(reader)
        reader.alignToByte()
        let raw = try reader.readBytes(Int(slen))
        guard let s = String(bytes: raw, encoding: .utf8) else {
            throw TriepackError.invalidData("Invalid UTF-8 in string value")
        }
        return .string(s)

    case TypeTag.blob:
        let blen = try VarInt.readUInt(reader)
        reader.alignToByte()
        let raw = try reader.readBytes(Int(blen))
        return .blob(Data(raw))

    default:
        throw TriepackError.invalidData("Unknown value type tag: \(tag)")
    }
}
