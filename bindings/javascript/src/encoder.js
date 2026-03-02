'use strict';

/**
 * Trie encoder — port of src/core/encoder.c.
 */

const { BitWriter } = require('./bitstream');
const { writeVarUint, varUintBits } = require('./varint');
const { encodeValue } = require('./values');
const { crc32 } = require('./crc32');

// Format constants (match core_internal.h)
const TP_MAGIC = [0x54, 0x52, 0x50, 0x00]; // "TRP\0"
const TP_VERSION_MAJOR = 1;
const TP_VERSION_MINOR = 0;
const TP_HEADER_SIZE = 32;
const TP_FLAG_HAS_VALUES = 1;

// Control code indices
const CTRL_END = 0;
const CTRL_END_VAL = 1;
const CTRL_SKIP = 2;
// const CTRL_SUFFIX = 3;
// const CTRL_ESCAPE = 4;
const CTRL_BRANCH = 5;
const NUM_CONTROL_CODES = 6;

function encode(data) {
    if (data === null || data === undefined || typeof data !== 'object') {
        throw new TypeError('encode expects a plain object');
    }

    // Collect entries: {keyBytes: Uint8Array, val: any}
    const entries = [];
    const keys = Object.keys(data);
    for (let i = 0; i < keys.length; i++) {
        const k = keys[i];
        const keyBytes = Buffer.from(k, 'utf8');
        entries.push({ keyBytes, val: data[k] });
    }

    // Sort lexicographically by key bytes
    entries.sort((a, b) => {
        const minLen = Math.min(a.keyBytes.length, b.keyBytes.length);
        for (let i = 0; i < minLen; i++) {
            if (a.keyBytes[i] !== b.keyBytes[i])
                return a.keyBytes[i] - b.keyBytes[i];
        }
        return a.keyBytes.length - b.keyBytes.length;
    });

    // Dedup (keep last)
    const deduped = [];
    for (let i = 0; i < entries.length; i++) {
        if (i + 1 < entries.length) {
            const a = entries[i].keyBytes;
            const b = entries[i + 1].keyBytes;
            if (a.length === b.length) {
                let same = true;
                for (let j = 0; j < a.length; j++) {
                    if (a[j] !== b[j]) { same = false; break; }
                }
                if (same) continue; // skip duplicate, keep later one
            }
        }
        deduped.push(entries[i]);
    }

    // Determine if any entries have non-null values
    let hasValues = false;
    for (let i = 0; i < deduped.length; i++) {
        const v = deduped[i].val;
        if (v !== null && v !== undefined) {
            hasValues = true;
            break;
        }
    }

    // Symbol analysis
    const used = new Uint8Array(256);
    for (let i = 0; i < deduped.length; i++) {
        const kb = deduped[i].keyBytes;
        for (let j = 0; j < kb.length; j++) {
            used[kb[j]] = 1;
        }
    }

    let alphabetSize = 0;
    for (let i = 0; i < 256; i++) {
        if (used[i]) alphabetSize++;
    }

    const totalSymbols = alphabetSize + NUM_CONTROL_CODES;
    let bps = 1;
    while ((1 << bps) < totalSymbols) bps++;

    // Control codes: 0..5
    const ctrlCodes = new Uint32Array(NUM_CONTROL_CODES);
    for (let c = 0; c < NUM_CONTROL_CODES; c++) ctrlCodes[c] = c;

    // Symbol map: byte value -> N-bit code
    const symbolMap = new Uint32Array(256);
    const reverseMap = new Uint8Array(256);
    let code = NUM_CONTROL_CODES;
    for (let i = 0; i < 256; i++) {
        if (used[i]) {
            symbolMap[i] = code;
            if (code < 256) reverseMap[code] = i;
            code++;
        }
    }

    // Build the bitstream
    const w = new BitWriter(256);

    // Write 32-byte header placeholder
    // magic (4)
    for (let i = 0; i < 4; i++) w.writeU8(TP_MAGIC[i]);
    // version (2)
    w.writeU8(TP_VERSION_MAJOR);
    w.writeU8(TP_VERSION_MINOR);
    // flags (2)
    const flags = hasValues ? TP_FLAG_HAS_VALUES : 0;
    w.writeU16(flags);
    // num_keys (4)
    w.writeU32(deduped.length);
    // trie_data_offset (4) — placeholder
    w.writeU32(0);
    // value_store_offset (4) — placeholder
    w.writeU32(0);
    // suffix_table_offset (4) — placeholder
    w.writeU32(0);
    // total_data_bits (4) — placeholder
    w.writeU32(0);
    // reserved (4)
    w.writeU32(0);

    const dataStart = w.position; // should be 256 (32 bytes * 8)

    // Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
    w.writeBits(bps, 4);
    w.writeBits(totalSymbols, 8);

    // Control code mappings (6 * bps bits)
    for (let c = 0; c < NUM_CONTROL_CODES; c++) {
        w.writeBits(ctrlCodes[c], bps);
    }

    // Symbol table: VarInt byte values for non-control symbols
    for (let cd = NUM_CONTROL_CODES; cd < totalSymbols; cd++) {
        const byteVal = cd < 256 ? reverseMap[cd] : 0;
        writeVarUint(w, byteVal);
    }

    // Record trie data offset
    const trieDataOffset = w.position - dataStart;

    // Write prefix trie
    const ctx = { deduped, bps, symbolMap, ctrlCodes, hasValues };
    let valueIdx = { val: 0 };
    if (deduped.length > 0) {
        trieWrite(ctx, w, 0, deduped.length, 0, valueIdx);
    }

    // Record value store offset
    const valueStoreOffset = w.position - dataStart;

    // Write value store
    if (hasValues) {
        for (let i = 0; i < deduped.length; i++) {
            encodeValue(w, deduped[i].val);
        }
    }

    // Total data bits
    const totalDataBits = w.position - dataStart;

    // Align to byte boundary
    w.alignToByte();

    // Get pre-CRC buffer
    const preCrcBuf = w.toUint8Array();
    const preCrcLen = preCrcBuf.length;

    // Compute CRC-32 over everything so far
    let crcVal = crc32(preCrcBuf);

    // Append CRC bytes (will overwrite after header patch)
    w.writeU32(crcVal);

    // Get final buffer
    const outBuf = w.toUint8Array();

    // Patch header: trie_data_offset at byte 12
    outBuf[12] = (trieDataOffset >>> 24) & 0xFF;
    outBuf[13] = (trieDataOffset >>> 16) & 0xFF;
    outBuf[14] = (trieDataOffset >>> 8) & 0xFF;
    outBuf[15] = trieDataOffset & 0xFF;
    // value_store_offset at byte 16
    outBuf[16] = (valueStoreOffset >>> 24) & 0xFF;
    outBuf[17] = (valueStoreOffset >>> 16) & 0xFF;
    outBuf[18] = (valueStoreOffset >>> 8) & 0xFF;
    outBuf[19] = valueStoreOffset & 0xFF;
    // suffix_table_offset at byte 20 = 0
    outBuf[20] = 0;
    outBuf[21] = 0;
    outBuf[22] = 0;
    outBuf[23] = 0;
    // total_data_bits at byte 24
    outBuf[24] = (totalDataBits >>> 24) & 0xFF;
    outBuf[25] = (totalDataBits >>> 16) & 0xFF;
    outBuf[26] = (totalDataBits >>> 8) & 0xFF;
    outBuf[27] = totalDataBits & 0xFF;

    // Recompute CRC over patched data (everything except last 4 bytes)
    const crcDataLen = outBuf.length - 4;
    crcVal = crc32(outBuf.subarray(0, crcDataLen));
    outBuf[crcDataLen] = (crcVal >>> 24) & 0xFF;
    outBuf[crcDataLen + 1] = (crcVal >>> 16) & 0xFF;
    outBuf[crcDataLen + 2] = (crcVal >>> 8) & 0xFF;
    outBuf[crcDataLen + 3] = crcVal & 0xFF;

    return outBuf;
}

// Compute subtree size in bits (dry run) — mirrors trie_subtree_size()
function trieSubtreeSize(ctx, start, end, prefixLen, valueIdx) {
    if (start >= end) return 0;

    const { deduped, bps, hasValues } = ctx;
    let bits = 0;

    // Find common prefix beyond prefixLen
    let common = prefixLen;
    while (true) {
        let allHave = true;
        let ch = 0;
        for (let i = start; i < end; i++) {
            if (deduped[i].keyBytes.length <= common) { allHave = false; break; }
            if (i === start) ch = deduped[i].keyBytes[common];
            else if (deduped[i].keyBytes[common] !== ch) { allHave = false; break; }
        }
        if (!allHave) break;
        common++;
    }

    // Common prefix symbols
    bits += (common - prefixLen) * bps;

    // Terminal check
    let hasTerminal = false;
    if (deduped[start].keyBytes.length === common) {
        hasTerminal = true;
    }

    // Count children
    const childrenStart = hasTerminal ? start + 1 : start;
    let childCount = 0;
    {
        let cs = childrenStart;
        while (cs < end) {
            childCount++;
            const ch = deduped[cs].keyBytes[common];
            let ce = cs + 1;
            while (ce < end && deduped[ce].keyBytes[common] === ch) ce++;
            cs = ce;
        }
    }

    if (hasTerminal && childCount === 0) {
        if (hasValues && deduped[start].val !== null && deduped[start].val !== undefined) {
            bits += bps; // END_VAL
            bits += varUintBits(valueIdx.val);
        } else {
            bits += bps; // END
        }
        valueIdx.val++;
    } else if (hasTerminal && childCount > 0) {
        if (hasValues && deduped[start].val !== null && deduped[start].val !== undefined) {
            bits += bps; // END_VAL
            bits += varUintBits(valueIdx.val);
        } else {
            bits += bps; // END
        }
        valueIdx.val++;
        bits += bps; // BRANCH
        bits += varUintBits(childCount);

        let cs = childrenStart;
        let childI = 0;
        while (cs < end) {
            const ch = deduped[cs].keyBytes[common];
            let ce = cs + 1;
            while (ce < end && deduped[ce].keyBytes[common] === ch) ce++;

            if (childI < childCount - 1) {
                const childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                bits += bps; // SKIP
                bits += varUintBits(childSz);
                bits += childSz;
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx);
            }
            childI++;
            cs = ce;
        }
    } else if (!hasTerminal && childCount === 1) {
        bits += trieSubtreeSize(ctx, childrenStart, end, common + 1, valueIdx);
    } else if (!hasTerminal && childCount > 1) {
        bits += bps; // BRANCH
        bits += varUintBits(childCount);

        let cs = childrenStart;
        let childI = 0;
        while (cs < end) {
            const ch = deduped[cs].keyBytes[common];
            let ce = cs + 1;
            while (ce < end && deduped[ce].keyBytes[common] === ch) ce++;

            if (childI < childCount - 1) {
                const childSz = trieSubtreeSize(ctx, cs, ce, common, valueIdx);
                bits += bps; // SKIP
                bits += varUintBits(childSz);
                bits += childSz;
            } else {
                bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx);
            }
            childI++;
            cs = ce;
        }
    }

    return bits;
}

// Write the trie subtree — mirrors trie_write()
function trieWrite(ctx, w, start, end, prefixLen, valueIdx) {
    if (start >= end) return;

    const { deduped, bps, symbolMap, ctrlCodes, hasValues } = ctx;

    // Find common prefix beyond prefixLen
    let common = prefixLen;
    while (true) {
        let allHave = true;
        let ch = 0;
        for (let i = start; i < end; i++) {
            if (deduped[i].keyBytes.length <= common) { allHave = false; break; }
            if (i === start) ch = deduped[i].keyBytes[common];
            else if (deduped[i].keyBytes[common] !== ch) { allHave = false; break; }
        }
        if (!allHave) break;
        common++;
    }

    // Write common prefix symbols
    for (let i = prefixLen; i < common; i++) {
        const ch = deduped[start].keyBytes[i];
        w.writeBits(symbolMap[ch], bps);
    }

    // Terminal check
    let hasTerminal = false;
    if (deduped[start].keyBytes.length === common) {
        hasTerminal = true;
    }

    // Count children
    const childrenStart = hasTerminal ? start + 1 : start;
    let childCount = 0;
    {
        let cs = childrenStart;
        while (cs < end) {
            childCount++;
            const ch = deduped[cs].keyBytes[common];
            let ce = cs + 1;
            while (ce < end && deduped[ce].keyBytes[common] === ch) ce++;
            cs = ce;
        }
    }

    // Write terminal if present
    if (hasTerminal) {
        if (hasValues && deduped[start].val !== null && deduped[start].val !== undefined) {
            w.writeBits(ctrlCodes[CTRL_END_VAL], bps);
            writeVarUint(w, valueIdx.val);
        } else {
            w.writeBits(ctrlCodes[CTRL_END], bps);
        }
        valueIdx.val++;
    }

    if (childCount === 0) {
        return;
    } else if (childCount === 1 && !hasTerminal) {
        trieWrite(ctx, w, childrenStart, end, common + 1, valueIdx);
        return;
    }

    // BRANCH
    w.writeBits(ctrlCodes[CTRL_BRANCH], bps);
    writeVarUint(w, childCount);

    // For each child: optionally write SKIP, then recurse.
    // Pass 'common' (not common+1) so each child writes its own
    // distinguishing character as the first symbol.
    let cs = childrenStart;
    let childI = 0;
    while (cs < end) {
        const ch = deduped[cs].keyBytes[common];
        let ce = cs + 1;
        while (ce < end && deduped[ce].keyBytes[common] === ch) ce++;

        if (childI < childCount - 1) {
            const viCopy = { val: valueIdx.val };
            const childSz = trieSubtreeSize(ctx, cs, ce, common, viCopy);
            w.writeBits(ctrlCodes[CTRL_SKIP], bps);
            writeVarUint(w, childSz);
        }

        trieWrite(ctx, w, cs, ce, common, valueIdx);

        childI++;
        cs = ce;
    }
}

module.exports = { encode };
