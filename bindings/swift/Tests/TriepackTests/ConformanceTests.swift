// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

// Cross-language conformance suite.
//
// Reads tests/conformance/cases.txt -- the corpus every implementation shares
// -- and checks this binding against the C-generated fixtures: decoding a
// fixture must produce the corpus values, and encoding the corpus values must
// produce the fixture byte for byte.
//
// See tests/conformance/README.md.

import XCTest
@testable import Triepack

final class ConformanceTests: XCTestCase {

    // MARK: - Corpus location

    private var conformanceDir: URL {
        // This file lives at bindings/swift/Tests/TriepackTests/.
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()   // TriepackTests/
            .deletingLastPathComponent()   // Tests/
            .deletingLastPathComponent()   // swift/
            .deletingLastPathComponent()   // bindings/
            .deletingLastPathComponent()   // repository root
            .appendingPathComponent("tests")
            .appendingPathComponent("conformance")
    }

    private func fixtureURL(_ name: String) -> URL {
        conformanceDir.appendingPathComponent("fixtures").appendingPathComponent(name + ".trp")
    }

    // MARK: - Corpus parsing

    /// Decode a corpus token ("~" for empty, %XX escapes) into bytes.
    private func tokenBytes(_ tok: String) -> [UInt8] {
        if tok == "~" { return [] }
        var out: [UInt8] = []
        let chars = Array(tok.utf8)
        var i = 0
        while i < chars.count {
            if chars[i] == UInt8(ascii: "%") {
                let hex = String(bytes: chars[(i + 1)...(i + 2)], encoding: .utf8)!
                out.append(UInt8(hex, radix: 16)!)
                i += 3
            } else {
                out.append(chars[i])
                i += 1
            }
        }
        return out
    }

    private func tokenString(_ tok: String) -> String {
        String(bytes: tokenBytes(tok), encoding: .utf8)!
    }

    private func hexBytes(_ tok: String) -> [UInt8] {
        if tok == "~" { return [] }
        var out: [UInt8] = []
        let chars = Array(tok)
        var i = 0
        while i < chars.count {
            out.append(UInt8(String(chars[i...(i + 1)]), radix: 16)!)
            i += 2
        }
        return out
    }

    private struct Case {
        let name: String
        let encodeExpected: Bool
        var entries: [(key: String, type: String, arg: String?)] = []
    }

    private func parseCases() throws -> [Case] {
        let url = conformanceDir.appendingPathComponent("cases.txt")
        let text = try String(contentsOf: url, encoding: .utf8)
        var cases: [Case] = []
        for raw in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.isEmpty || line.hasPrefix("#") { continue }
            let parts = line.split(separator: " ").map(String.init)
            if parts[0] == "case" {
                cases.append(Case(name: parts[1], encodeExpected: !parts.contains("encode=no")))
            } else if parts[0] == "key" {
                cases[cases.count - 1].entries.append(
                    (key: tokenString(parts[1]),
                     type: parts[2],
                     arg: parts.count > 3 ? parts[3] : nil))
            } else {
                XCTFail("unknown corpus directive: \(parts[0])")
            }
        }
        return cases
    }

    /// Build the Swift value a corpus entry describes.
    private func corpusValue(_ type: String, _ arg: String?) -> TriepackValue {
        switch type {
        case "null": return .null
        case "bool": return .bool(arg == "1")
        case "int": return .int(Int64(arg!)!)
        case "uint": return .uint(UInt64(arg!)!)
        case "f64": return .float64(Double(bitPattern: UInt64(arg!, radix: 16)!))
        case "f32": return .float64(Double(Float(bitPattern: UInt32(arg!, radix: 16)!)))
        case "str": return .string(tokenString(arg!))
        case "blob": return .blob(Data(hexBytes(arg!)))
        default:
            XCTFail("unknown corpus type: \(type)")
            return .null
        }
    }

    private func caseData(_ c: Case) -> [String: TriepackValue] {
        var data: [String: TriepackValue] = [:]
        for e in c.entries { data[e.key] = corpusValue(e.type, e.arg) }
        return data
    }

    /// Doubles compare by bit pattern so -0.0 is distinguished, with all NaNs
    /// treated as equal.
    private func valuesMatch(_ got: TriepackValue?, _ want: TriepackValue) -> Bool {
        guard let got = got else { return false }
        if case .float64(let g) = got, case .float64(let w) = want {
            if g.isNaN && w.isNaN { return true }
            return g.bitPattern == w.bitPattern
        }
        return got == want
    }

    // MARK: - The suite

    func testConformanceCorpusAndFixturesPresent() throws {
        let cases = try parseCases()
        XCTAssertFalse(cases.isEmpty, "corpus is empty")
        for c in cases {
            XCTAssertTrue(FileManager.default.fileExists(atPath: fixtureURL(c.name).path),
                          "case \(c.name): missing fixture")
        }
    }

    func testConformanceDecodesCFixtures() throws {
        for c in try parseCases() {
            let fixture = try Data(contentsOf: fixtureURL(c.name))
            let result = try Triepack.decode(fixture)
            let want = caseData(c)
            XCTAssertEqual(result.count, want.count, "case \(c.name): key count")
            for (k, v) in want {
                XCTAssertTrue(valuesMatch(result[k], v),
                              "case \(c.name): key \(k): got \(String(describing: result[k])), want \(v)")
            }
        }
    }

    /// Each file is a valid fixture with one field damaged. Damage inside the
    /// data is re-sealed with a correct CRC, so the reader has to catch it
    /// rather than being handed a checksum failure.
    func testConformanceRejectsMalformed() throws {
        let dir = conformanceDir.appendingPathComponent("malformed")
        let names = try FileManager.default.contentsOfDirectory(atPath: dir.path)
            .filter { $0.hasSuffix(".trp") }.sorted()
        XCTAssertFalse(names.isEmpty, "malformed corpus is empty")
        for name in names {
            let buf = try Data(contentsOf: dir.appendingPathComponent(name))
            XCTAssertThrowsError(try Triepack.decode(buf), "\(name): accepted malformed input")
        }
    }

    func testConformanceEncodesLikeCReference() throws {
        for c in try parseCases() {
            // Cases marked encode=no describe values this binding cannot
            // reproduce exactly (float32, or doubles some languages read back
            // as integers).
            guard c.encodeExpected else { continue }
            let got = try Triepack.encode(caseData(c))
            let want = try Data(contentsOf: fixtureURL(c.name))
            XCTAssertEqual(got, want,
                           "case \(c.name): encoded \(got.count) bytes, C reference is \(want.count)")
        }
    }
}
