// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Write arbitrary-width bit fields into a growable buffer (MSB-first).
final class BitWriter {
    private var buf: [UInt8]
    private(set) var position: Int = 0

    init(initialCapacity: Int = 256) {
        buf = [UInt8](repeating: 0, count: max(initialCapacity, 256))
    }

    private func ensure(_ nBits: Int) {
        let needed = (position + nBits + 7) >> 3
        if needed <= buf.count { return }
        var newCap = buf.count * 2
        while newCap < needed { newCap *= 2 }
        buf.append(contentsOf: [UInt8](repeating: 0, count: newCap - buf.count))
    }

    func writeBits(value: UInt64, count n: Int) {
        precondition(n >= 1 && n <= 64)
        ensure(n)
        for i in 0..<n {
            let bit = (value >> UInt64(n - 1 - i)) & 1
            let byteIdx = position >> 3
            let bitIdx = 7 - (position & 7)
            if bit != 0 {
                buf[byteIdx] |= UInt8(1 << bitIdx)
            } else {
                buf[byteIdx] &= ~UInt8(1 << bitIdx)
            }
            position += 1
        }
    }

    func writeBit(_ val: Int) {
        ensure(1)
        let byteIdx = position >> 3
        let bitIdx = 7 - (position & 7)
        if val != 0 {
            buf[byteIdx] |= UInt8(1 << bitIdx)
        } else {
            buf[byteIdx] &= ~UInt8(1 << bitIdx)
        }
        position += 1
    }

    func writeU8(_ val: UInt8) {
        writeBits(value: UInt64(val), count: 8)
    }

    func writeU16(_ val: UInt16) {
        writeBits(value: UInt64(val), count: 16)
    }

    func writeU32(_ val: UInt32) {
        writeBits(value: UInt64(val), count: 32)
    }

    func writeU64Parts(hi: UInt32, lo: UInt32) {
        writeBits(value: UInt64(hi), count: 32)
        writeBits(value: UInt64(lo), count: 32)
    }

    func writeBytes(_ data: [UInt8]) {
        for b in data {
            writeU8(b)
        }
    }

    func writeBytes(_ data: Data) {
        for b in data {
            writeU8(b)
        }
    }

    func alignToByte() {
        let rem = position & 7
        if rem != 0 {
            let pad = 8 - rem
            ensure(pad)
            position += pad
        }
    }

    func toData() -> Data {
        let length = (position + 7) >> 3
        return Data(buf[0..<length])
    }

    /// Direct access to the internal buffer for patching.
    var buffer: [UInt8] {
        get { buf }
        set { buf = newValue }
    }
}

/// Read arbitrary-width bit fields from a buffer (MSB-first).
final class BitReader {
    private let buf: [UInt8]
    let bitLen: Int
    private(set) var position: Int = 0

    var remaining: Int { bitLen - position }

    init(_ data: Data) {
        self.buf = [UInt8](data)
        self.bitLen = data.count * 8
    }

    init(_ bytes: [UInt8]) {
        self.buf = bytes
        self.bitLen = bytes.count * 8
    }

    func seek(_ bitPos: Int) {
        position = bitPos
    }

    func readBits(_ n: Int) throws -> UInt64 {
        guard n >= 1 && n <= 64 else {
            throw TriepackError.invalidData("readBits: n must be 1..64")
        }
        guard position + n <= bitLen else {
            throw TriepackError.eof
        }
        var val: UInt64 = 0
        for _ in 0..<n {
            let byteIdx = position >> 3
            let bitIdx = 7 - (position & 7)
            val = (val << 1) | UInt64((buf[byteIdx] >> bitIdx) & 1)
            position += 1
        }
        return val
    }

    func readBit() throws -> UInt64 {
        guard position < bitLen else { throw TriepackError.eof }
        let byteIdx = position >> 3
        let bitIdx = 7 - (position & 7)
        let bit = UInt64((buf[byteIdx] >> bitIdx) & 1)
        position += 1
        return bit
    }

    func readU8() throws -> UInt8 {
        return UInt8(try readBits(8))
    }

    func readU16() throws -> UInt16 {
        return UInt16(try readBits(16))
    }

    func readU32() throws -> UInt32 {
        return UInt32(try readBits(32))
    }

    func readU64() throws -> UInt64 {
        let hi = UInt64(try readU32())
        let lo = UInt64(try readU32())
        return hi * 0x1_0000_0000 + lo
    }

    func readBytes(_ n: Int) throws -> [UInt8] {
        var out = [UInt8](repeating: 0, count: n)
        for i in 0..<n {
            out[i] = try readU8()
        }
        return out
    }

    func peekBits(_ n: Int) throws -> UInt64 {
        let saved = position
        let val = try readBits(n)
        position = saved
        return val
    }

    func alignToByte() {
        let rem = position & 7
        if rem != 0 {
            position += 8 - rem
        }
    }
}
