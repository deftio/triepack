// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Errors thrown by the Triepack library.
public enum TriepackError: Error {
    case notImplemented
}

/// Native Swift implementation of the Triepack .trp binary format.
public struct Triepack {
    /// Encode a dictionary into the .trp binary format.
    ///
    /// - Parameter data: The dictionary to encode.
    /// - Returns: The encoded binary data.
    /// - Throws: `TriepackError.notImplemented`
    public static func encode(_ data: [String: Any]) throws -> Data {
        throw TriepackError.notImplemented
    }

    /// Decode a .trp binary buffer into a dictionary.
    ///
    /// - Parameter buffer: The .trp binary data.
    /// - Returns: The decoded dictionary.
    /// - Throws: `TriepackError.notImplemented`
    public static func decode(_ buffer: Data) throws -> [String: Any] {
        throw TriepackError.notImplemented
    }
}
