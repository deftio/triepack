// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import Foundation

/// Typed value representation for Triepack entries.
public enum TriepackValue: Equatable {
    case null
    case bool(Bool)
    case int(Int64)         // signed (negative)
    case uint(UInt64)       // unsigned (non-negative)
    case float64(Double)
    case string(String)
    case blob(Data)
}

/// Errors thrown by the Triepack library.
public enum TriepackError: Error, Equatable {
    case invalidMagic
    case unsupportedVersion
    case crcMismatch
    case dataTooShort
    case overflow
    case eof
    case invalidData(String)
}

/// Native Swift implementation of the Triepack .trp binary format.
public struct Triepack {
    /// Encode a dictionary into the .trp binary format.
    ///
    /// - Parameter data: The dictionary to encode.
    /// - Returns: The encoded binary data.
    public static func encode(_ data: [String: TriepackValue]) throws -> Data {
        return try triepackEncode(data)
    }

    /// Decode a .trp binary buffer into a dictionary.
    ///
    /// - Parameter buffer: The .trp binary data.
    /// - Returns: The decoded dictionary.
    public static func decode(_ buffer: Data) throws -> [String: TriepackValue] {
        return try triepackDecode(buffer)
    }
}
