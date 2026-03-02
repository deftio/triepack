'use strict';

const { BitWriter, BitReader } = require('../src/bitstream');

describe('BitWriter', () => {
    test('write and read single bits', () => {
        const w = new BitWriter();
        w.writeBit(1);
        w.writeBit(0);
        w.writeBit(1);
        w.writeBit(1);
        w.writeBit(0);
        w.writeBit(1);
        w.writeBit(0);
        w.writeBit(1);
        // MSB-first: 10110101 = 0xB5
        const buf = w.toUint8Array();
        expect(buf.length).toBe(1);
        expect(buf[0]).toBe(0xB5);
    });

    test('writeBits multi-bit values', () => {
        const w = new BitWriter();
        w.writeBits(0b1010, 4);
        w.writeBits(0b1100, 4);
        const buf = w.toUint8Array();
        expect(buf[0]).toBe(0xAC);
    });

    test('writeU8 / writeU16 / writeU32', () => {
        const w = new BitWriter();
        w.writeU8(0xAB);
        w.writeU16(0xCDEF);
        w.writeU32(0x12345678);
        const r = new BitReader(w.toUint8Array());
        expect(r.readU8()).toBe(0xAB);
        expect(r.readU16()).toBe(0xCDEF);
        expect(r.readU32()).toBe(0x12345678);
    });

    test('alignToByte pads with zeros', () => {
        const w = new BitWriter();
        w.writeBits(0b111, 3);
        w.alignToByte();
        expect(w.position).toBe(8);
        const buf = w.toUint8Array();
        // 111_00000 = 0xE0
        expect(buf[0]).toBe(0xE0);
    });

    test('writeBytes', () => {
        const w = new BitWriter();
        w.writeBytes(new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]));
        const r = new BitReader(w.toUint8Array());
        expect(r.readU32()).toBe(0xDEADBEEF);
    });
});

describe('BitReader', () => {
    test('readBits across byte boundaries', () => {
        const buf = new Uint8Array([0xFF, 0x00]);
        const r = new BitReader(buf);
        expect(r.readBits(4)).toBe(0xF);
        expect(r.readBits(8)).toBe(0xF0);
        expect(r.readBits(4)).toBe(0x0);
    });

    test('peek does not advance', () => {
        const buf = new Uint8Array([0xAB]);
        const r = new BitReader(buf);
        const peeked = r.peekBits(4);
        expect(peeked).toBe(0xA);
        expect(r.position).toBe(0);
        expect(r.readBits(4)).toBe(0xA);
        expect(r.position).toBe(4);
    });

    test('seek and advance', () => {
        const buf = new Uint8Array([0xFF, 0x00]);
        const r = new BitReader(buf);
        r.seek(8);
        expect(r.readBits(8)).toBe(0x00);
        r.seek(0);
        r.advance(4);
        expect(r.readBits(4)).toBe(0xF);
    });

    test('alignToByte', () => {
        const buf = new Uint8Array([0xFF, 0xAB]);
        const r = new BitReader(buf);
        r.readBits(3);
        r.alignToByte();
        expect(r.position).toBe(8);
        expect(r.readU8()).toBe(0xAB);
    });

    test('readBytes', () => {
        const orig = new Uint8Array([0x01, 0x02, 0x03]);
        const r = new BitReader(orig);
        const out = r.readBytes(3);
        expect(out).toEqual(orig);
    });

    test('EOF throws', () => {
        const r = new BitReader(new Uint8Array([0xFF]));
        r.readBits(8);
        expect(() => r.readBits(1)).toThrow('EOF');
    });

    test('remaining property', () => {
        const r = new BitReader(new Uint8Array([0xFF, 0x00]));
        expect(r.remaining).toBe(16);
        r.readBits(5);
        expect(r.remaining).toBe(11);
    });
});
