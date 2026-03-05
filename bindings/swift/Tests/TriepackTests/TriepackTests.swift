// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import XCTest
@testable import Triepack

final class TriepackTests: XCTestCase {

    // MARK: - Fixture Helpers

    private func fixtureURL(_ name: String) -> URL {
        // Try Bundle.module first (SPM resource bundle)
        #if swift(>=5.3)
        if let url = Bundle.module.url(forResource: name, withExtension: nil, subdirectory: "fixtures") {
            return url
        }
        #endif
        // Fallback: navigate from test file location to fixtures
        let testFile = URL(fileURLWithPath: #filePath)
        let fixturesDir = testFile.deletingLastPathComponent().appendingPathComponent("fixtures")
        return fixturesDir.appendingPathComponent(name)
    }

    private func readFixture(_ name: String) throws -> Data {
        let url = fixtureURL(name)
        return try Data(contentsOf: url)
    }

    // MARK: - Decode Fixture Tests

    func testDecodeEmpty() throws {
        let buf = try readFixture("empty.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result.count, 0)
    }

    func testDecodeSingleNull() throws {
        let buf = try readFixture("single_null.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result, ["hello": .null])
    }

    func testDecodeSingleInt() throws {
        let buf = try readFixture("single_int.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result, ["key": .uint(42)])
    }

    func testDecodeMultiMixed() throws {
        let buf = try readFixture("multi_mixed.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result["bool"], .bool(true))
        if case .float64(let d) = result["f64"] {
            XCTAssertEqual(d, 3.14159, accuracy: 1e-10)
        } else {
            XCTFail("Expected float64 for f64 key")
        }
        XCTAssertEqual(result["int"], .int(-100))
        XCTAssertEqual(result["str"], .string("hello"))
        XCTAssertEqual(result["uint"], .uint(200))
    }

    func testDecodeSharedPrefix() throws {
        let buf = try readFixture("shared_prefix.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result, ["abc": .uint(10), "abd": .uint(20), "xyz": .uint(30)])
    }

    func testDecodeLarge() throws {
        let buf = try readFixture("large.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result.count, 100)
        for i in 0..<100 {
            let key = String(format: "key_%04d", i)
            XCTAssertEqual(result[key], .uint(UInt64(i)), "Mismatch at \(key)")
        }
    }

    func testDecodeKeysOnly() throws {
        let buf = try readFixture("keys_only.trp")
        let result = try Triepack.decode(buf)
        XCTAssertEqual(result, ["apple": .null, "banana": .null, "cherry": .null])
    }

    // MARK: - Byte-Identical Encode Tests

    func testEncodeEmptyMatchesFixture() throws {
        let swBuf = try Triepack.encode([:])
        let cBuf = try readFixture("empty.trp")
        XCTAssertEqual(swBuf, cBuf, "Empty encode does not match fixture")
    }

    func testEncodeSingleNullMatchesFixture() throws {
        let swBuf = try Triepack.encode(["hello": .null])
        let cBuf = try readFixture("single_null.trp")
        XCTAssertEqual(swBuf, cBuf, "single_null encode does not match fixture")
    }

    func testEncodeSingleIntMatchesFixture() throws {
        let swBuf = try Triepack.encode(["key": .uint(42)])
        let cBuf = try readFixture("single_int.trp")
        XCTAssertEqual(swBuf, cBuf, "single_int encode does not match fixture")
    }

    func testEncodeMultiMixedMatchesFixture() throws {
        let swBuf = try Triepack.encode([
            "bool": .bool(true),
            "f64": .float64(3.14159),
            "int": .int(-100),
            "str": .string("hello"),
            "uint": .uint(200),
        ])
        let cBuf = try readFixture("multi_mixed.trp")
        XCTAssertEqual(swBuf, cBuf, "multi_mixed encode does not match fixture")
    }

    func testEncodeSharedPrefixMatchesFixture() throws {
        let swBuf = try Triepack.encode(["abc": .uint(10), "abd": .uint(20), "xyz": .uint(30)])
        let cBuf = try readFixture("shared_prefix.trp")
        XCTAssertEqual(swBuf, cBuf, "shared_prefix encode does not match fixture")
    }

    func testEncodeLargeMatchesFixture() throws {
        var data: [String: TriepackValue] = [:]
        for i in 0..<100 {
            data[String(format: "key_%04d", i)] = .uint(UInt64(i))
        }
        let swBuf = try Triepack.encode(data)
        let cBuf = try readFixture("large.trp")
        XCTAssertEqual(swBuf, cBuf, "large encode does not match fixture")
    }

    func testEncodeKeysOnlyMatchesFixture() throws {
        let swBuf = try Triepack.encode(["apple": .null, "banana": .null, "cherry": .null])
        let cBuf = try readFixture("keys_only.trp")
        XCTAssertEqual(swBuf, cBuf, "keys_only encode does not match fixture")
    }

    // MARK: - Roundtrip Tests

    func testRoundtripEmpty() throws {
        let original: [String: TriepackValue] = [:]
        let encoded = try Triepack.encode(original)
        let decoded = try Triepack.decode(encoded)
        XCTAssertEqual(decoded, original)
    }

    func testRoundtripSingleKey() throws {
        let original: [String: TriepackValue] = ["test": .uint(123)]
        let encoded = try Triepack.encode(original)
        let decoded = try Triepack.decode(encoded)
        XCTAssertEqual(decoded, original)
    }

    func testRoundtripMixedTypes() throws {
        let original: [String: TriepackValue] = [
            "a_null": .null,
            "b_bool_true": .bool(true),
            "c_bool_false": .bool(false),
            "d_negative": .int(-42),
            "e_positive": .uint(99),
            "f_float": .float64(2.71828),
            "g_string": .string("Hello, world!"),
            "h_blob": .blob(Data([0xDE, 0xAD, 0xBE, 0xEF])),
        ]
        let encoded = try Triepack.encode(original)
        let decoded = try Triepack.decode(encoded)
        XCTAssertEqual(decoded, original)
    }

    func testRoundtripLargeDict() throws {
        var original: [String: TriepackValue] = [:]
        for i in 0..<200 {
            original["item_\(String(format: "%04d", i))"] = .uint(UInt64(i * 3))
        }
        let encoded = try Triepack.encode(original)
        let decoded = try Triepack.decode(encoded)
        XCTAssertEqual(decoded, original)
    }

    func testRoundtripUTF8Keys() throws {
        let original: [String: TriepackValue] = [
            "cafe": .uint(1),
            "naive": .uint(2),
            "zoo": .uint(3),
        ]
        let encoded = try Triepack.encode(original)
        let decoded = try Triepack.decode(encoded)
        XCTAssertEqual(decoded, original)
    }

    // MARK: - Error Tests

    func testCRCCorruption() throws {
        var buf = try readFixture("single_int.trp")
        // Corrupt last byte (part of CRC)
        let lastIdx = buf.count - 1
        buf[lastIdx] = buf[lastIdx] ^ 0xFF
        XCTAssertThrowsError(try Triepack.decode(buf)) { error in
            XCTAssertEqual(error as? TriepackError, TriepackError.crcMismatch)
        }
    }

    func testInvalidMagic() throws {
        var buf = try readFixture("single_int.trp")
        buf[0] = 0x00  // corrupt magic
        XCTAssertThrowsError(try Triepack.decode(buf)) { error in
            XCTAssertEqual(error as? TriepackError, TriepackError.invalidMagic)
        }
    }

    func testDataTooShort() throws {
        let buf = Data([0x54, 0x52, 0x50, 0x00])  // just magic, too short
        XCTAssertThrowsError(try Triepack.decode(buf)) { error in
            XCTAssertEqual(error as? TriepackError, TriepackError.dataTooShort)
        }
    }

    func testUnsupportedVersion() throws {
        var buf = try readFixture("empty.trp")
        buf[4] = 99  // bogus version
        // Also need to fix CRC after modification, but this should fail on version check
        // before CRC (actually the code checks magic, then version, then CRC)
        // The decoder checks CRC after version, so we need to fix CRC too
        // Actually looking at the decoder code, it checks version then CRC, so
        // unsupported version is checked first.
        XCTAssertThrowsError(try Triepack.decode(buf)) { error in
            XCTAssertEqual(error as? TriepackError, TriepackError.unsupportedVersion)
        }
    }

    // MARK: - CRC32 Unit Tests

    func testCRC32EmptyData() {
        let crc = CRC32.compute(Data())
        XCTAssertEqual(crc, 0x00000000)
    }

    func testCRC32KnownValue() {
        // CRC32 of "123456789" is 0xCBF43926
        let data = Data("123456789".utf8)
        let crc = CRC32.compute(data)
        XCTAssertEqual(crc, 0xCBF43926)
    }

    // MARK: - VarInt Unit Tests

    func testVarIntRoundtrip() throws {
        let values: [UInt64] = [0, 1, 127, 128, 16383, 16384, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF]
        for val in values {
            let w = BitWriter()
            VarInt.writeUInt(w, val)
            let r = BitReader(w.toData())
            let decoded = try VarInt.readUInt(r)
            XCTAssertEqual(decoded, val, "VarInt roundtrip failed for \(val)")
        }
    }

    func testSignedVarIntRoundtrip() throws {
        let values: [Int64] = [0, 1, -1, 100, -100, Int64.max, Int64.min + 1]
        for val in values {
            let w = BitWriter()
            VarInt.writeInt(w, val)
            let r = BitReader(w.toData())
            let decoded = try VarInt.readInt(r)
            XCTAssertEqual(decoded, val, "Signed VarInt roundtrip failed for \(val)")
        }
    }
}
