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
    /// Keys use more distinct byte values than the format can address.
    /// The payload is the alphabet size that was rejected.
    case alphabetTooLarge(Int)
}

/// Native Swift implementation of the Triepack .trp binary format.
/// Metadata about a triepack build. Every implementation reports the same
/// fields, so a polyglot system can ask each one what it is.
public struct VersionInfo: Equatable {
    public let name: String
    public let implementation: String
    public let version: String
    public let versionMajor: Int
    public let versionMinor: Int
    public let versionPatch: Int
    public let formatVersionMajor: Int
    public let formatVersionMinor: Int
    public let maxAlphabetSize: Int
}

public struct Triepack {
    /// Library version, kept in step with triepack-version.txt by
    /// scripts/sync_version.sh.
    public static let version = "1.3.2"

    /// Version of the on-disk .trp format this implementation writes.
    /// Distinct from the library version: it changes only when the bytes
    /// change.
    public static let formatVersionMajor = 1
    public static let formatVersionMinor = 0

    /// Return metadata about this build.
    public static func versionInfo() -> VersionInfo {
        let parts = version.split(separator: ".").map { Int($0) ?? 0 }
        return VersionInfo(
            name: "triepack",
            implementation: "swift",
            version: version,
            versionMajor: parts.count > 0 ? parts[0] : 0,
            versionMinor: parts.count > 1 ? parts[1] : 0,
            versionPatch: parts.count > 2 ? parts[2] : 0,
            formatVersionMajor: formatVersionMajor,
            formatVersionMinor: formatVersionMinor,
            maxAlphabetSize: triepackMaxAlphabetSize)
    }

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
