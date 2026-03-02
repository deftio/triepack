'use strict';

const { BitWriter, BitReader } = require('../src/bitstream');
const { writeVarUint, readVarUint, writeVarInt, readVarInt, varUintBits } = require('../src/varint');

function roundtripUnsigned(val) {
    const w = new BitWriter();
    writeVarUint(w, val);
    const r = new BitReader(w.toUint8Array());
    return readVarUint(r);
}

function roundtripSigned(val) {
    const w = new BitWriter();
    writeVarInt(w, val);
    const r = new BitReader(w.toUint8Array());
    return readVarInt(r);
}

describe('VarUint (LEB128)', () => {
    test('zero', () => {
        expect(roundtripUnsigned(0)).toBe(0);
    });

    test('small values (1 byte)', () => {
        for (const v of [1, 42, 63, 127]) {
            expect(roundtripUnsigned(v)).toBe(v);
        }
    });

    test('medium values (2 bytes)', () => {
        for (const v of [128, 255, 300, 16383]) {
            expect(roundtripUnsigned(v)).toBe(v);
        }
    });

    test('larger values', () => {
        for (const v of [16384, 65535, 100000, 1000000]) {
            expect(roundtripUnsigned(v)).toBe(v);
        }
    });

    test('power of 2 values', () => {
        for (let i = 0; i < 32; i++) {
            const v = Math.pow(2, i);
            expect(roundtripUnsigned(v)).toBe(v);
        }
    });

    test('varUintBits matches actual encoding size', () => {
        for (const val of [0, 1, 127, 128, 16383, 16384, 2097151]) {
            const w = new BitWriter();
            writeVarUint(w, val);
            expect(varUintBits(val)).toBe(w.position);
        }
    });
});

describe('VarInt (zigzag)', () => {
    test('zero', () => {
        expect(roundtripSigned(0)).toBe(0);
    });

    test('positive values', () => {
        for (const v of [1, 42, 127, 128, 10000]) {
            expect(roundtripSigned(v)).toBe(v);
        }
    });

    test('negative values', () => {
        for (const v of [-1, -42, -128, -129, -10000]) {
            expect(roundtripSigned(v)).toBe(v);
        }
    });

    test('zigzag encoding: small values are compact', () => {
        // -1 -> raw=1 (1 byte), 1 -> raw=2 (1 byte)
        const w1 = new BitWriter();
        writeVarInt(w1, -1);
        expect(w1.position).toBe(8);

        const w2 = new BitWriter();
        writeVarInt(w2, 1);
        expect(w2.position).toBe(8);
    });

    test('symmetric positive/negative roundtrip', () => {
        for (let v = -1000; v <= 1000; v++) {
            expect(roundtripSigned(v)).toBe(v);
        }
    });
});
