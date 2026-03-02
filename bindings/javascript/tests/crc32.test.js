'use strict';

const { crc32 } = require('../src/crc32');

describe('CRC-32', () => {
    test('known answer: "123456789" → 0xCBF43926', () => {
        const data = Buffer.from('123456789', 'ascii');
        expect(crc32(data)).toBe(0xCBF43926);
    });

    test('empty buffer → 0x00000000', () => {
        expect(crc32(new Uint8Array(0))).toBe(0x00000000);
    });

    test('single zero byte', () => {
        const data = new Uint8Array([0x00]);
        expect(crc32(data)).toBe(0xD202EF8D);
    });

    test('single 0xFF byte', () => {
        const data = new Uint8Array([0xFF]);
        expect(crc32(data)).toBe(0xFF000000);
    });

    test('all zeroes (4 bytes)', () => {
        const data = new Uint8Array([0, 0, 0, 0]);
        expect(crc32(data)).toBe(0x2144DF1C);
    });

    test('deterministic — same input same output', () => {
        const data = Buffer.from('hello world', 'utf8');
        const a = crc32(data);
        const b = crc32(data);
        expect(a).toBe(b);
    });

    test('different inputs produce different results', () => {
        const a = crc32(Buffer.from('abc'));
        const b = crc32(Buffer.from('abd'));
        expect(a).not.toBe(b);
    });
});
