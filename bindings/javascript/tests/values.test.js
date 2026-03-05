'use strict';

const { BitWriter, BitReader } = require('../src/bitstream');
const { encodeValue, decodeValue } = require('../src/values');

describe('Value encode/decode', () => {
    test('encode null', () => {
        const w = new BitWriter();
        encodeValue(w, null);
        const r = new BitReader(w.toUint8Array());
        expect(r.readBits(4)).toBe(0); // TP_NULL
    });

    test('encode undefined as null', () => {
        const w = new BitWriter();
        encodeValue(w, undefined);
        const r = new BitReader(w.toUint8Array());
        expect(r.readBits(4)).toBe(0); // TP_NULL
    });

    test('encode bool', () => {
        const w = new BitWriter();
        encodeValue(w, true);
        encodeValue(w, false);
        const r = new BitReader(w.toUint8Array());
        expect(r.readBits(4)).toBe(1); // TP_BOOL
        expect(r.readBit()).toBe(1);
        expect(r.readBits(4)).toBe(1); // TP_BOOL
        expect(r.readBit()).toBe(0);
    });

    test('decode null', () => {
        const w = new BitWriter();
        w.writeBits(0, 4); // TP_NULL
        const r = new BitReader(w.toUint8Array());
        expect(decodeValue(r)).toBeNull();
    });

    test('decode float32', () => {
        const w = new BitWriter();
        w.writeBits(4, 4); // TP_FLOAT32
        const buf = Buffer.alloc(4);
        buf.writeFloatBE(3.140000104904175, 0);
        w.writeU32(buf.readUInt32BE(0));
        const r = new BitReader(w.toUint8Array());
        const val = decodeValue(r);
        expect(Math.abs(val - 3.14)).toBeLessThan(0.01);
    });

    test('encode unsupported type throws', () => {
        const w = new BitWriter();
        expect(() => encodeValue(w, [1, 2, 3])).toThrow('Unsupported');
    });

    test('decode unknown tag throws', () => {
        const w = new BitWriter();
        w.writeBits(15, 4); // invalid tag
        const r = new BitReader(w.toUint8Array());
        expect(() => decodeValue(r)).toThrow('Unknown');
    });
});
