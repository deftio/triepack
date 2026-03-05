// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — TypeScript wrapper around the JavaScript Triepack implementation.
 *
 * Provides type-safe encode/decode for the .trp binary format.
 */

/* eslint-disable @typescript-eslint/no-var-requires */
const js = require("../../javascript/src/index");

/** Supported value types in a Triepack dictionary. */
export type TriePackValue =
    | null
    | boolean
    | number
    | string
    | Uint8Array;

/** A key-value record that can be encoded to .trp format. */
export type TriePackData = Record<string, TriePackValue>;

/**
 * Encode a record into the .trp binary format.
 *
 * Keys are stored in a compressed trie; values are typed (null, bool,
 * int, uint, float32, float64, string, blob).  The output includes a
 * CRC-32 integrity checksum.
 *
 * @param data - The key-value data to encode.
 * @returns The encoded binary buffer.
 */
export function encode(data: TriePackData): Uint8Array {
    return js.encode(data) as Uint8Array;
}

/**
 * Decode a .trp binary buffer into a record.
 *
 * Validates magic bytes, format version, and CRC-32 checksum before
 * walking the trie and deserialising values.
 *
 * @param buffer - The .trp binary data.
 * @returns The decoded key-value data.
 * @throws Error on invalid magic, version mismatch, or CRC failure.
 */
export function decode(buffer: Uint8Array): TriePackData {
    return js.decode(buffer) as TriePackData;
}
