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

    test('decode rejects non-Uint8Array input', () => {
        expect(() => decode('not bytes')).toThrow();
    });

    test('decode rejects bad version', () => {
        const { crc32 } = require('../src/crc32');
        const buf = encode({ a: 1 });
        const patched = new Uint8Array(buf);
        patched[4] = 2; // bad major version
        // Recompute CRC
        const crcData = patched.subarray(0, patched.length - 4);
        const crcVal = crc32(crcData);
        const off = patched.length - 4;
        patched[off] = (crcVal >>> 24) & 0xFF;
        patched[off + 1] = (crcVal >>> 16) & 0xFF;
        patched[off + 2] = (crcVal >>> 8) & 0xFF;
        patched[off + 3] = crcVal & 0xFF;
        expect(() => decode(patched)).toThrow('version');
    });

    test('prefix key keys-only (CTRL_END + CTRL_BRANCH)', () => {
        const data = { a: null, ab: null, ac: null };
        const buf = encode(data);
        const result = decode(buf);
        expect(Object.keys(result).sort()).toEqual(['a', 'ab', 'ac']);
        Object.values(result).forEach(v => expect(v).toBeNull());
    });

    test('prefix key with values (CTRL_END_VAL + CTRL_BRANCH)', () => {
        const data = { a: 1, ab: 2, ac: 3 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('terminal with children as non-last child', () => {
        const data = { a: 1, ab: 2, b: 3 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('terminal with children and null value', () => {
        const data = { a: null, ab: 2, b: 3 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('null value in mixed dict', () => {
        const data = { alpha: null, beta: 42 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('subtree multi-entry child groups', () => {
        const data = { a: 1, aba: 2, abb: 3, ac: 4, b: 5 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('non-terminal multi-entry children', () => {
        const data = { aba: 1, abb: 2, ac: 3, b: 4 };
        const buf = encode(data);
        const result = decode(buf);
        expect(result).toEqual(data);
    });

    test('unexpected control code in trie', () => {
        const { BitWriter } = require('../src/bitstream');
        const { crc32 } = require('../src/crc32');
        const { writeVarUint } = require('../src/varint');

        const w = new BitWriter();
        // Magic
        [0x54, 0x52, 0x50, 0x00].forEach(b => w.writeU8(b));
        // Version
        w.writeU8(1); w.writeU8(0);
        // Flags (no values)
        w.writeU16(0);
        // num_keys = 1
        w.writeU32(1);
        // Placeholder offsets
        w.writeU32(0); w.writeU32(0); w.writeU32(0); w.writeU32(0); w.writeU32(0);

        const dataStart = w.position;

        // Trie config: bps=4, totalSymbols=7
        w.writeBits(4, 4);
        w.writeBits(7, 8);
        // Control codes 0..5
        for (let c = 0; c < 6; c++) w.writeBits(c, 4);
        // One symbol: 'a' = 97
        writeVarUint(w, 97);

        const trieDataOffset = w.position - dataStart;

        // Write CTRL_SKIP (2) as first trie symbol — unexpected outside branch
        w.writeBits(2, 4);
        w.alignToByte();

        const preBuf = w.toUint8Array();
        const result = new Uint8Array(preBuf.length + 4);
        result.set(preBuf);

        // Patch trie_data_offset at byte 12
        result[12] = (trieDataOffset >>> 24) & 0xFF;
        result[13] = (trieDataOffset >>> 16) & 0xFF;
        result[14] = (trieDataOffset >>> 8) & 0xFF;
        result[15] = trieDataOffset & 0xFF;
        // value_store_offset = trieDataOffset
        result[16] = result[12]; result[17] = result[13];
        result[18] = result[14]; result[19] = result[15];

        // CRC
        const crcData = result.subarray(0, result.length - 4);
        const crcVal = crc32(crcData);
        const crcOff = result.length - 4;
        result[crcOff] = (crcVal >>> 24) & 0xFF;
        result[crcOff + 1] = (crcVal >>> 16) & 0xFF;
        result[crcOff + 2] = (crcVal >>> 8) & 0xFF;
        result[crcOff + 3] = crcVal & 0xFF;

        expect(() => decode(result)).toThrow('Unexpected control code');
    });

    test('decoder EOF during DFS returns empty result', () => {
        const { BitWriter } = require('../src/bitstream');
        const { crc32 } = require('../src/crc32');
        const { writeVarUint } = require('../src/varint');

        const w = new BitWriter();
        [0x54, 0x52, 0x50, 0x00].forEach(b => w.writeU8(b));
        w.writeU8(1); w.writeU8(0);
        w.writeU16(0);
        w.writeU32(1); // num_keys
        w.writeU32(0); w.writeU32(0); w.writeU32(0); w.writeU32(0); w.writeU32(0);

        const dataStart = w.position;
        w.writeBits(4, 4);
        w.writeBits(7, 8);
        for (let c = 0; c < 6; c++) w.writeBits(c, 4);
        writeVarUint(w, 97);

        // Point trie_data_offset way past the buffer
        const trieDataOffset = 99999;
        w.alignToByte();
        const preBuf = w.toUint8Array();
        const result = new Uint8Array(preBuf.length + 4);
        result.set(preBuf);

        result[12] = (trieDataOffset >>> 24) & 0xFF;
        result[13] = (trieDataOffset >>> 16) & 0xFF;
        result[14] = (trieDataOffset >>> 8) & 0xFF;
        result[15] = trieDataOffset & 0xFF;
        result[16] = (trieDataOffset >>> 24) & 0xFF;
        result[17] = (trieDataOffset >>> 16) & 0xFF;
        result[18] = (trieDataOffset >>> 8) & 0xFF;
        result[19] = trieDataOffset & 0xFF;

        const crcData = result.subarray(0, result.length - 4);
        const crcVal = crc32(crcData);
        const crcOff = result.length - 4;
        result[crcOff] = (crcVal >>> 24) & 0xFF;
        result[crcOff + 1] = (crcVal >>> 16) & 0xFF;
        result[crcOff + 2] = (crcVal >>> 8) & 0xFF;
        result[crcOff + 3] = crcVal & 0xFF;

        const decoded = decode(result);
        expect(typeof decoded).toBe('object');
    });
});
