/*
 * FixturesTest.java
 *
 * Cross-language fixture tests -- mirrors Python test_fixtures.py.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import static org.junit.jupiter.api.Assertions.*;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.LinkedHashMap;
import java.util.Map;

import org.junit.jupiter.api.Test;

class FixturesTest {

    /**
     * Locate the fixtures directory relative to this source file.
     * Works when running from bindings/java/ with gradle.
     */
    private static Path fixtureDir() {
        // Try multiple possible locations
        Path[] candidates = {
            Paths.get("../../tests/fixtures"),
            Paths.get("tests/fixtures"),
            Paths.get("../../../tests/fixtures"),
        };
        for (Path p : candidates) {
            if (Files.isDirectory(p)) {
                return p;
            }
        }
        // Fallback to absolute path
        return Paths.get(System.getProperty("user.dir"))
                     .resolve("../../tests/fixtures").normalize();
    }

    private static byte[] readFixture(String name) throws IOException {
        return Files.readAllBytes(fixtureDir().resolve(name));
    }

    // ── Decode C-generated fixtures ──────────────────────────────

    @Test
    void testDecodeEmpty() throws IOException {
        byte[] buf = readFixture("empty.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertTrue(result.isEmpty());
    }

    @Test
    void testDecodeSingleNull() throws IOException {
        byte[] buf = readFixture("single_null.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.size());
        assertEquals(TpValue.Type.NULL, result.get("hello").getType());
    }

    @Test
    void testDecodeSingleInt() throws IOException {
        byte[] buf = readFixture("single_int.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.size());
        assertEquals(42, result.get("key").uintValue());
    }

    @Test
    void testDecodeMultiMixed() throws IOException {
        byte[] buf = readFixture("multi_mixed.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertTrue(result.get("bool").boolValue());
        assertEquals(3.14159, result.get("f64").float64Value(), 1e-10);
        assertEquals(-100, result.get("int").intValue());
        assertEquals("hello", result.get("str").stringValue());
        assertEquals(200, result.get("uint").uintValue());
    }

    @Test
    void testDecodeSharedPrefix() throws IOException {
        byte[] buf = readFixture("shared_prefix.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(10, result.get("abc").uintValue());
        assertEquals(20, result.get("abd").uintValue());
        assertEquals(30, result.get("xyz").uintValue());
    }

    @Test
    void testDecodeLarge() throws IOException {
        byte[] buf = readFixture("large.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(100, result.size());
        for (int i = 0; i < 100; i++) {
            String key = String.format("key_%04d", i);
            assertEquals(i, result.get(key).uintValue());
        }
    }

    @Test
    void testDecodeKeysOnly() throws IOException {
        byte[] buf = readFixture("keys_only.trp");
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(3, result.size());
        assertEquals(TpValue.Type.NULL, result.get("apple").getType());
        assertEquals(TpValue.Type.NULL, result.get("banana").getType());
        assertEquals(TpValue.Type.NULL, result.get("cherry").getType());
    }

    // ── Byte-for-byte binary reproducibility ─────────────────────

    @Test
    void testEncodeEmptyMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("empty.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeSingleNullMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("hello", TpValue.ofNull());
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("single_null.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeSingleIntMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("key", TpValue.ofUInt(42));
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("single_int.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeMultiMixedMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("bool", TpValue.ofBool(true));
        data.put("f64", TpValue.ofFloat64(3.14159));
        data.put("int", TpValue.ofInt(-100));
        data.put("str", TpValue.ofString("hello"));
        data.put("uint", TpValue.ofUInt(200));
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("multi_mixed.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeSharedPrefixMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("abc", TpValue.ofUInt(10));
        data.put("abd", TpValue.ofUInt(20));
        data.put("xyz", TpValue.ofUInt(30));
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("shared_prefix.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeLargeMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        for (int i = 0; i < 100; i++) {
            data.put(String.format("key_%04d", i), TpValue.ofUInt(i));
        }
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("large.trp");
        assertArrayEquals(cBuf, javaBuf);
    }

    @Test
    void testEncodeKeysOnlyMatchesFixture() throws IOException {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("apple", TpValue.ofNull());
        data.put("banana", TpValue.ofNull());
        data.put("cherry", TpValue.ofNull());
        byte[] javaBuf = TriePack.encode(data);
        byte[] cBuf = readFixture("keys_only.trp");
        assertArrayEquals(cBuf, javaBuf);
    }
}
