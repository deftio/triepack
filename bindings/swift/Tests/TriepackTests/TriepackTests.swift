// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import XCTest
@testable import Triepack

final class TriepackTests: XCTestCase {
    func testEncodeThrowsNotImplemented() {
        XCTAssertThrowsError(try Triepack.encode(["hello": "world"])) { error in
            XCTAssertEqual(error as? TriepackError, TriepackError.notImplemented)
        }
    }
}
