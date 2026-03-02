'use strict';

/**
 * Trie decoder — port of src/core/decoder.c.
 */

const { BitReader } = require('./bitstream');
const { readVarUint } = require('./varint');
const { decodeValue } = require('./values');
const { crc32 } = require('./crc32');

// Format constants
const TP_MAGIC = [0x54, 0x52, 0x50, 0x00];
const TP_HEADER_SIZE = 32;
const TP_FLAG_HAS_VALUES = 1;
const NUM_CONTROL_CODES = 6;

// Control code indices
const CTRL_END = 0;
const CTRL_END_VAL = 1;
const CTRL_SKIP = 2;
const CTRL_BRANCH = 5;

function decode(buffer) {
    if (!(buffer instanceof Uint8Array) && !Buffer.isBuffer(buffer)) {
        throw new TypeError('decode expects a Uint8Array or Buffer');
    }
    if (buffer.length < TP_HEADER_SIZE + 4) {
        throw new Error('Data too short for .trp format');
    }

    const buf = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);

    // Validate magic
    for (let i = 0; i < 4; i++) {
        if (buf[i] !== TP_MAGIC[i]) {
            throw new Error('Invalid magic bytes — not a .trp file');
        }
    }

    // Version
    const versionMajor = buf[4];
    if (versionMajor !== 1) {
        throw new Error('Unsupported format version: ' + versionMajor);
    }

    // Flags
    const flags = (buf[6] << 8) | buf[7];
    const hasValues = (flags & TP_FLAG_HAS_VALUES) !== 0;

    // num_keys
    const numKeys = ((buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11]) >>> 0;

    // Offsets (relative to data start)
    const trieDataOffset = ((buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15]) >>> 0;
    const valueStoreOffset = ((buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19]) >>> 0;

    // CRC verification
    const crcDataLen = buf.length - 4;
    const expectedCrc =
        ((buf[crcDataLen] << 24) | (buf[crcDataLen + 1] << 16) |
         (buf[crcDataLen + 2] << 8) | buf[crcDataLen + 3]) >>> 0;
    const actualCrc = crc32(buf.subarray(0, crcDataLen));
    if (actualCrc !== expectedCrc) {
        throw new Error('CRC-32 integrity check failed');
    }

    if (numKeys === 0) {
        return {};
    }

    // Parse trie config
    const reader = new BitReader(buf);
    const dataStart = TP_HEADER_SIZE * 8; // bit 256
    reader.seek(dataStart);

    // bits_per_symbol (4 bits) + symbol_count (8 bits)
    const bps = reader.readBits(4);
    const symbolCount = reader.readBits(8);

    // Read control codes
    const ctrlCodes = new Uint32Array(NUM_CONTROL_CODES);
    const codeIsCtrl = new Uint8Array(256);
    for (let c = 0; c < NUM_CONTROL_CODES; c++) {
        ctrlCodes[c] = reader.readBits(bps);
        if (ctrlCodes[c] < 256) codeIsCtrl[ctrlCodes[c]] = 1;
    }

    // Read symbol table
    const symbolMap = new Uint32Array(256);
    const reverseMap = new Uint8Array(256);
    for (let cd = NUM_CONTROL_CODES; cd < symbolCount; cd++) {
        const cp = readVarUint(reader);
        if (cd < 256 && cp < 256) {
            reverseMap[cd] = cp;
            symbolMap[cp] = cd;
        }
    }

    const trieStart = dataStart + trieDataOffset;
    const valueStart = dataStart + valueStoreOffset;

    // DFS iteration to collect all key-value pairs
    const result = {};
    const keyStack = []; // accumulated key bytes

    function dfsWalk(r) {
        while (true) {
            if (r.position >= r._bitLen) return;
            const sym = r.readBits(bps);

            if (sym === ctrlCodes[CTRL_END]) {
                // Terminal with no value (null)
                const keyStr = Buffer.from(keyStack).toString('utf8');
                result[keyStr] = null;

                // Check if BRANCH follows
                if (r.position + bps <= r._bitLen) {
                    const nextSym = r.peekBits(bps);
                    if (nextSym === ctrlCodes[CTRL_BRANCH]) {
                        r.readBits(bps); // consume BRANCH
                        const childCount = readVarUint(r);
                        walkBranch(r, childCount);
                    }
                }
                return;
            }

            if (sym === ctrlCodes[CTRL_END_VAL]) {
                readVarUint(r); // value index (consumed but we decode values in order)
                const keyStr = Buffer.from(keyStack).toString('utf8');
                result[keyStr] = null; // placeholder, replaced later if hasValues

                // Check if BRANCH follows
                if (r.position + bps <= r._bitLen) {
                    const nextSym = r.peekBits(bps);
                    if (nextSym === ctrlCodes[CTRL_BRANCH]) {
                        r.readBits(bps); // consume BRANCH
                        const childCount = readVarUint(r);
                        walkBranch(r, childCount);
                    }
                }
                return;
            }

            if (sym === ctrlCodes[CTRL_BRANCH]) {
                const childCount = readVarUint(r);
                walkBranch(r, childCount);
                return;
            }

            // Regular symbol
            if (sym < 256 && codeIsCtrl[sym]) {
                throw new Error('Unexpected control code in trie');
            }
            const byteVal = sym < 256 ? reverseMap[sym] : 0;
            keyStack.push(byteVal);
        }
    }

    function walkBranch(r, childCount) {
        const savedKeyLen = keyStack.length;
        for (let ci = 0; ci < childCount; ci++) {
            const hasSkip = ci < childCount - 1;
            let skipDist = 0;

            if (hasSkip) {
                r.readBits(bps); // SKIP control code
                skipDist = readVarUint(r);
            }

            const childStartPos = r.position;
            keyStack.length = savedKeyLen;
            dfsWalk(r);

            // If this wasn't the last child, verify position matches skip
            if (hasSkip) {
                r.seek(childStartPos + skipDist);
            }
        }
        keyStack.length = savedKeyLen;
    }

    // Walk the trie
    reader.seek(trieStart);
    if (numKeys > 0) {
        // The trie root may start with a single child (no explicit BRANCH)
        // so we just start dfsWalk from the root
        dfsWalk(reader);
    }

    // Now decode values if present
    if (hasValues) {
        reader.seek(valueStart);
        // Decode values for all keys in sorted order
        const sortedKeys = Object.keys(result).sort((a, b) => {
            const ab = Buffer.from(a, 'utf8');
            const bb = Buffer.from(b, 'utf8');
            const minLen = Math.min(ab.length, bb.length);
            for (let i = 0; i < minLen; i++) {
                if (ab[i] !== bb[i]) return ab[i] - bb[i];
            }
            return ab.length - bb.length;
        });
        for (let i = 0; i < sortedKeys.length; i++) {
            result[sortedKeys[i]] = decodeValue(reader);
        }
    }

    return result;
}

module.exports = { decode };
