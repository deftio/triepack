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

describe('VarInt error handling', () => {
    test('readVarUint overflow throws', () => {
        const w = new BitWriter();
        for (let i = 0; i < 11; i++) w.writeU8(0x80);
        const r = new BitReader(w.toUint8Array());
        expect(() => readVarUint(r)).toThrow('overflow');
    });

    test('writeVarUint rejects negative', () => {
        const w = new BitWriter();
        expect(() => writeVarUint(w, -1)).toThrow();
    });
});

// JavaScript keeps integers in a double, so values past 2^53-1 are already
// rounded by the time they reach the encoder. Refusing beats writing bytes
// that decode to a different number.
describe('integer range limits', () => {
    const { BitWriter, BitReader } = require('../src/bitstream');

    test('writeVarUint refuses values beyond MAX_SAFE_INTEGER', () => {
        const w = new BitWriter();
        expect(() => writeVarUint(w, Number.MAX_SAFE_INTEGER)).not.toThrow();
        expect(() => writeVarUint(w, Number.MAX_SAFE_INTEGER + 2)).toThrow(RangeError);
        expect(() => writeVarUint(w, 2 ** 64)).toThrow(RangeError);
    });

    test('writeVarInt refuses values beyond the exact zigzag range', () => {
        const w = new BitWriter();
        // Zigzag is 2v for v >= 0 and 2|v|-1 below, so the exact range is
        // -2^52 .. 2^52-1, one wider on the negative side.
        expect(() => writeVarInt(w, -(2 ** 52))).not.toThrow();
        expect(() => writeVarInt(w, 2 ** 52 - 1)).not.toThrow();
        expect(() => writeVarInt(w, 2 ** 52)).toThrow(RangeError);
        expect(() => writeVarInt(w, -(2 ** 52) - 1)).toThrow(RangeError);
        expect(() => writeVarInt(w, -9007199254740991)).toThrow(RangeError);
    });

    test('readVarUint refuses a value it cannot represent exactly', () => {
        // 2^60 written by hand as LEB128 — a C encoder can produce this.
        const w = new BitWriter();
        let v = 2 ** 60;
        const bytes = [];
        do {
            let byte = v % 128;
            v = Math.floor(v / 128);
            if (v > 0) byte |= 0x80;
            bytes.push(byte);
        } while (v > 0);
        bytes.forEach(b => w.writeU8(b));

        const r = new BitReader(w.toUint8Array());
        expect(() => readVarUint(r)).toThrow(RangeError);
    });

    test('values at the edge still round-trip', () => {
        for (const v of [0, 1, 127, 128, Number.MAX_SAFE_INTEGER]) {
            const w = new BitWriter();
            writeVarUint(w, v);
            expect(readVarUint(new BitReader(w.toUint8Array()))).toBe(v);
        }
        for (const v of [-1, -2, -(2 ** 52), 2 ** 52 - 1]) {
            const w = new BitWriter();
            writeVarInt(w, v);
            expect(readVarInt(new BitReader(w.toUint8Array()))).toBe(v);
        }
    });
});
