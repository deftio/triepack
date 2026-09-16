'use strict';

/**
 * LEB128 unsigned VarInt + zigzag signed VarInt — matches bitstream_varint.c.
 *
 * Uses Math.floor(value / 128) instead of >>> 7 to handle values > 2^32.
 */

const VARINT_MAX_GROUPS = 10;

function writeVarUint(writer, value) {
    if (value < 0) throw new RangeError('writeVarUint: negative value');
    // Integers live in a double here, so anything past 2^53-1 would be written
    // from a value that is already rounded. Refuse rather than emit bytes that
    // decode to a different number.
    if (!Number.isSafeInteger(value)) {
        throw new RangeError(
            'writeVarUint: ' + value + ' is outside the exactly representable ' +
            'integer range (up to ' + Number.MAX_SAFE_INTEGER + ')'
        );
    }
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
            // Past 2^53-1 a double can no longer tell neighbouring integers
            // apart, so returning would hand back a silently wrong number.
            if (!Number.isSafeInteger(val)) {
                throw new RangeError(
                    'VarInt value exceeds the exactly representable integer ' +
                    'range (up to ' + Number.MAX_SAFE_INTEGER + ')'
                );
            }
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
    // Zigzag roughly doubles the magnitude, so the exact range is about half:
    // outside it the value would round here and decode to a different number.
    // Exactly, that is -2^52 .. 2^52-1.
    if (!Number.isSafeInteger(raw)) {
        throw new RangeError(
            'writeVarInt: ' + value + ' is outside the range this implementation ' +
            'can encode exactly (-4503599627370496 to 4503599627370495)'
        );
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
