'use strict';

/**
 * Value encoder/decoder for 8 type tags — matches src/core/value.c.
 */

const { writeVarUint, readVarUint, writeVarInt, readVarInt } = require('./varint');

// Type tags (4-bit, matches tp_value_type enum)
const TP_NULL = 0;
const TP_BOOL = 1;
const TP_INT = 2;
const TP_UINT = 3;
const TP_FLOAT32 = 4;
const TP_FLOAT64 = 5;
const TP_STRING = 6;
const TP_BLOB = 7;

// Shared DataView for float conversions (big-endian)
const _f64Buf = new ArrayBuffer(8);
const _f64View = new DataView(_f64Buf);

function encodeValue(writer, val) {
    if (val === null || val === undefined) {
        writer.writeBits(TP_NULL, 4);
        return;
    }

    if (typeof val === 'boolean') {
        writer.writeBits(TP_BOOL, 4);
        writer.writeBit(val ? 1 : 0);
        return;
    }

    if (typeof val === 'number') {
        if (Number.isInteger(val)) {
            if (val < 0) {
                writer.writeBits(TP_INT, 4);
                writeVarInt(writer, val);
            } else {
                writer.writeBits(TP_UINT, 4);
                writeVarUint(writer, val);
            }
        } else {
            // Float64
            writer.writeBits(TP_FLOAT64, 4);
            _f64View.setFloat64(0, val, false); // big-endian
            const hi = _f64View.getUint32(0, false);
            const lo = _f64View.getUint32(4, false);
            writer.writeU64BE(hi, lo);
        }
        return;
    }

    if (typeof val === 'string') {
        const bytes = Buffer.from(val, 'utf8');
        writer.writeBits(TP_STRING, 4);
        writeVarUint(writer, bytes.length);
        writer.alignToByte();
        writer.writeBytes(bytes);
        return;
    }

    if (val instanceof Uint8Array || Buffer.isBuffer(val)) {
        writer.writeBits(TP_BLOB, 4);
        writeVarUint(writer, val.length);
        writer.alignToByte();
        writer.writeBytes(val);
        return;
    }

    throw new TypeError('Unsupported value type: ' + typeof val);
}

function decodeValue(reader) {
    const tag = reader.readBits(4);

    switch (tag) {
    case TP_NULL:
        return null;
    case TP_BOOL:
        return reader.readBit() !== 0;
    case TP_INT:
        return readVarInt(reader);
    case TP_UINT:
        return readVarUint(reader);
    case TP_FLOAT32: {
        const bits = reader.readU32();
        _f64View.setUint32(0, bits, false);
        return _f64View.getFloat32(0, false);
    }
    case TP_FLOAT64: {
        const hi = reader.readU32();
        const lo = reader.readU32();
        _f64View.setUint32(0, hi, false);
        _f64View.setUint32(4, lo, false);
        return _f64View.getFloat64(0, false);
    }
    case TP_STRING: {
        const slen = readVarUint(reader);
        reader.alignToByte();
        const bytes = reader.readBytes(slen);
        return Buffer.from(bytes).toString('utf8');
    }
    case TP_BLOB: {
        const blen = readVarUint(reader);
        reader.alignToByte();
        return reader.readBytes(blen);
    }
    default:
        throw new Error('Unknown value type tag: ' + tag);
    }
}

module.exports = {
    encodeValue, decodeValue,
    TP_NULL, TP_BOOL, TP_INT, TP_UINT, TP_FLOAT32, TP_FLOAT64, TP_STRING, TP_BLOB
};
