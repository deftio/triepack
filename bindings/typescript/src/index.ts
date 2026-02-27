// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — Native TypeScript implementation of the Triepack .trp binary format.
 */

/**
 * Encode a record into the .trp binary format.
 *
 * @param data - The key-value data to encode.
 * @returns The encoded binary data.
 * @throws Error when called (not yet implemented).
 */
export function encode(data: Record<string, unknown>): Uint8Array {
    throw new Error("Not implemented");
}

/**
 * Decode a .trp binary buffer into a record.
 *
 * @param buffer - The .trp binary data.
 * @returns The decoded key-value data.
 * @throws Error when called (not yet implemented).
 */
export function decode(buffer: Uint8Array): Record<string, unknown> {
    throw new Error("Not implemented");
}
