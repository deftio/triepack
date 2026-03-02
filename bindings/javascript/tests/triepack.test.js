'use strict';

const { encode, decode } = require('../src/index');

describe('triepack encode/decode roundtrip', () => {
    test('empty object', () => {
        const buf = encode({});
        expect(buf).toBeInstanceOf(Uint8Array);
        expect(buf.length).toBeGreaterThanOrEqual(36); // 32-byte header + 4-byte CRC
        const result = decode(buf);
        expect(result).toEqual({});
    });

    test('single key with null value', () => {
        const data = { hello: null };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with integer value', () => {
        const data = { key: 42 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with negative integer', () => {
        const data = { neg: -100 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with boolean true', () => {
        const data = { flag: true };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with boolean false', () => {
        const data = { flag: false };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with float64', () => {
        const data = { pi: 3.14159 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result.pi).toBeCloseTo(3.14159, 10);
    });

    test('single key with string value', () => {
        const data = { greeting: 'hello world' };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single key with blob value', () => {
        const blob = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]);
        const data = { binary: blob };
        const buf = encode(data);
        const result = decode(buf);
        expect(result.binary).toEqual(blob);
    });

    test('multiple keys with mixed types', () => {
        const data = {
            bool: true,
            f64: 3.14159,
            int: -100,
            str: 'hello',
            uint: 200,
        };
        const buf = encode(data);
        const result = decode(buf);
        expect(result.bool).toBe(true);
        expect(result.f64).toBeCloseTo(3.14159, 10);
        expect(result.int).toBe(-100);
        expect(result.str).toBe('hello');
        expect(result.uint).toBe(200);
    });

    test('shared prefix keys', () => {
        const data = { abc: 10, abd: 20, xyz: 30 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('keys only (all null values)', () => {
        const data = { apple: null, banana: null, cherry: null };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('large dictionary (100 entries)', () => {
        const data = {};
        for (let i = 0; i < 100; i++) {
            const key = 'key_' + String(i).padStart(4, '0');
            data[key] = i;
        }
        const buf = encode(data);
        const result = decode(buf);
        expect(Object.keys(result).length).toBe(100);
        for (let i = 0; i < 100; i++) {
            const key = 'key_' + String(i).padStart(4, '0');
            expect(result[key]).toBe(i);
        }
    });

    test('duplicate keys (last wins)', () => {
        const data = { a: 1, b: 2, c: 3 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('magic bytes are TRP\\0', () => {
        const buf = encode({ test: 1 });
        expect(buf[0]).toBe(0x54); // T
        expect(buf[1]).toBe(0x52); // R
        expect(buf[2]).toBe(0x50); // P
        expect(buf[3]).toBe(0x00); // \0
    });

    test('version is 1.0', () => {
        const buf = encode({ test: 1 });
        expect(buf[4]).toBe(1); // major
        expect(buf[5]).toBe(0); // minor
    });

    test('CRC corruption detection', () => {
        const buf = encode({ test: 1 });
        const corrupted = new Uint8Array(buf);
        corrupted[10] ^= 0x01;
        expect(() => decode(corrupted)).toThrow('CRC');
    });

    test('invalid magic rejected', () => {
        const buf = new Uint8Array(40);
        buf[0] = 0xFF;
        expect(() => decode(buf)).toThrow('magic');
    });

    test('undefined values treated as null', () => {
        const data = { a: undefined };
        const buf = encode(data);
        const result = decode(buf);
        expect(result.a).toBeNull();
    });

    test('encode rejects non-object input', () => {
        expect(() => encode('string')).toThrow();
        expect(() => encode(42)).toThrow();
        expect(() => encode(null)).toThrow();
    });

    test('decode rejects too-short input', () => {
        expect(() => decode(new Uint8Array(10))).toThrow();
    });

    test('UTF-8 keys', () => {
        const data = { 'café': 1, 'naïve': 2, 'über': 3 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('single character keys', () => {
        const data = { a: 1, b: 2, c: 3, d: 4, e: 5 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('empty string key', () => {
        const data = { '': 99 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result['']).toBe(99);
    });

    test('long shared prefix', () => {
        const data = {
            'aaaaaa1': 1,
            'aaaaaa2': 2,
            'aaaaaa3': 3,
        };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });
});
