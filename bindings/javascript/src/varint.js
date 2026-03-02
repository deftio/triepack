'use strict';

/**
 * LEB128 unsigned VarInt + zigzag signed VarInt — matches bitstream_varint.c.
 *
 * Uses Math.floor(value / 128) instead of >>> 7 to handle values > 2^32.
 */

const VARINT_MAX_GROUPS = 10;

function writeVarUint(writer, value) {
    if (value < 0) throw new RangeError('writeVarUint: negative value');
    do {
        let byte = value & 0x7F;
        value = Math.floor(value / 128);
        if (value > 0) byte |= 0x80;
        writer.writeU8(byte);
    } while (value > 0);
}

function readVarUint(reader) {
    let val = 0;
    let shift = 0;
    for (let group = 0; group < VARINT_MAX_GROUPS; group++) {
        const byte = reader.readU8();
        val += (byte & 0x7F) * Math.pow(2, shift);
        if ((byte & 0x80) === 0) {
            return val;
        }
        shift += 7;
    }
    throw new Error('VarInt overflow');
}

function writeVarInt(writer, value) {
    // Zigzag encode: (value << 1) ^ (value >> 63)
    // For JS: we need to handle sign carefully
    let raw;
    if (value >= 0) {
        raw = value * 2;
    } else {
        raw = (-value) * 2 - 1;
    }
    writeVarUint(writer, raw);
}

function readVarInt(reader) {
    const raw = readVarUint(reader);
    // Zigzag decode
    if (raw & 1) {
        return -Math.floor(raw / 2) - 1;
    }
    return Math.floor(raw / 2);
}

function varUintBits(val) {
    let bits = 0;
    do {
        bits += 8;
        val = Math.floor(val / 128);
    } while (val > 0);
    return bits;
}

module.exports = { writeVarUint, readVarUint, writeVarInt, readVarInt, varUintBits };
