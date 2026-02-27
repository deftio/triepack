// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import { encode, decode } from "../src/index";

describe("triepack", () => {
    test("encode throws not implemented", () => {
        expect(() => encode({ hello: "world" })).toThrow("Not implemented");
    });

    test("decode throws not implemented", () => {
        expect(() => decode(new Uint8Array([]))).toThrow("Not implemented");
    });
});
