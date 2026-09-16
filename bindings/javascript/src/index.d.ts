// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — type declarations for the native JavaScript implementation of
 * the .trp binary format.
 */

/** Supported value types in a Triepack dictionary. */
export type TriePackValue = null | boolean | number | string | Uint8Array;

/** A key-value record that can be encoded to .trp format. */
export type TriePackData = Record<string, TriePackValue>;

/**
 * Encode a record into the .trp binary format.
 *
 * Keys are stored in a compressed trie; values are typed (null, bool, int,
 * uint, float32, float64, string, blob). The output carries a CRC-32
 * integrity checksum.
 *
 * Integers must be exactly representable as a double: up to
 * `Number.MAX_SAFE_INTEGER`, and down to -2^52 once zigzag encoding is
 * applied. Anything outside that raises a `RangeError` rather than writing
 * bytes that would decode to a different number.
 *
 * @throws RangeError if a number is outside the exactly representable range,
 *   or if the keys together use more than {@link MAX_ALPHABET_SIZE} distinct
 *   byte values.
 */
export function encode(data: TriePackData): Uint8Array;

/**
 * Decode a .trp binary buffer into a record.
 *
 * Validates magic bytes, format version, CRC-32 checksum and trie config
 * before walking the trie and deserialising values.
 *
 * @throws Error on invalid magic, version mismatch, CRC failure, a malformed
 *   trie, or a value too large to represent exactly.
 */
export function decode(buffer: Uint8Array): TriePackData;

/**
 * Largest number of distinct byte values the keys may use.
 *
 * `symbol_count` is an 8-bit header field holding the alphabet plus the six
 * control codes, so the alphabet cannot exceed 255 - 6. UTF-8 keys top out
 * around 243 distinct bytes, so this is effectively out of reach.
 */
export const MAX_ALPHABET_SIZE: number;
