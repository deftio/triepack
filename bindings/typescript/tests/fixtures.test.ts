// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import * as fs from "fs";
import * as path from "path";
import { encode, decode, TriePackData } from "../src/index";

const FIXTURE_DIR = path.resolve(__dirname, "../../../tests/fixtures");

function readFixture(name: string): Uint8Array {
    return new Uint8Array(fs.readFileSync(path.join(FIXTURE_DIR, name)));
}

describe("cross-language fixture tests", () => {
    test("fixtures directory exists", () => {
        expect(fs.existsSync(FIXTURE_DIR)).toBe(true);
    });

    describe("decode C-generated fixtures", () => {
        test("empty.trp -> {} (0 keys)", () => {
            const result = decode(readFixture("empty.trp"));
            expect(result).toEqual({});
        });

        test("single_null.trp -> { hello: null }", () => {
            const result = decode(readFixture("single_null.trp"));
            expect(result).toEqual({ hello: null });
        });

        test("single_int.trp -> { key: 42 }", () => {
            const result = decode(readFixture("single_int.trp"));
            expect(result).toEqual({ key: 42 });
        });

        test("multi_mixed.trp -> 5 mixed-type keys", () => {
            const result = decode(readFixture("multi_mixed.trp"));
            expect(result.bool).toBe(true);
            expect(result.f64 as number).toBeCloseTo(3.14159, 10);
            expect(result.int).toBe(-100);
            expect(result.str).toBe("hello");
            expect(result.uint).toBe(200);
        });

        test("shared_prefix.trp -> { abc: 10, abd: 20, xyz: 30 }", () => {
            const result = decode(readFixture("shared_prefix.trp"));
            expect(result).toEqual({ abc: 10, abd: 20, xyz: 30 });
        });

        test("large.trp -> 100 sequential keys", () => {
            const result = decode(readFixture("large.trp"));
            expect(Object.keys(result).length).toBe(100);
            for (let i = 0; i < 100; i++) {
                const key = "key_" + String(i).padStart(4, "0");
                expect(result[key]).toBe(i);
            }
        });

        test("keys_only.trp -> { apple: null, banana: null, cherry: null }", () => {
            const result = decode(readFixture("keys_only.trp"));
            expect(result).toEqual({ apple: null, banana: null, cherry: null });
        });
    });

    describe("byte-for-byte binary reproducibility", () => {
        test("empty -> matches C fixture", () => {
            const tsBuf = encode({});
            const cBuf = readFixture("empty.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("single_null -> matches C fixture", () => {
            const tsBuf = encode({ hello: null });
            const cBuf = readFixture("single_null.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("single_int -> matches C fixture", () => {
            const tsBuf = encode({ key: 42 });
            const cBuf = readFixture("single_int.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("multi_mixed -> matches C fixture", () => {
            const data: TriePackData = {
                bool: true,
                f64: 3.14159,
                int: -100,
                str: "hello",
                uint: 200,
            };
            const tsBuf = encode(data);
            const cBuf = readFixture("multi_mixed.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("shared_prefix -> matches C fixture", () => {
            const tsBuf = encode({ abc: 10, abd: 20, xyz: 30 });
            const cBuf = readFixture("shared_prefix.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("large -> matches C fixture", () => {
            const data: TriePackData = {};
            for (let i = 0; i < 100; i++) {
                data["key_" + String(i).padStart(4, "0")] = i;
            }
            const tsBuf = encode(data);
            const cBuf = readFixture("large.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });

        test("keys_only -> matches C fixture", () => {
            const tsBuf = encode({ apple: null, banana: null, cherry: null });
            const cBuf = readFixture("keys_only.trp");
            expect(Buffer.from(tsBuf).equals(Buffer.from(cBuf))).toBe(true);
        });
    });
});
