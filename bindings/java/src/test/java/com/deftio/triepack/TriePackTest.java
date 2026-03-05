/*
 * TriePackTest.java
 *
 * Roundtrip tests for the Java TriePack binding.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import static org.junit.jupiter.api.Assertions.*;

import java.util.LinkedHashMap;
import java.util.Map;

import org.junit.jupiter.api.Test;

class TriePackTest {

    // ── Roundtrip tests ───────────────────────────────────────────────

    @Test
    void testEmptyObject() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        byte[] buf = TriePack.encode(data);
        assertNotNull(buf);
        assertTrue(buf.length >= 36); // 32-byte header + 4-byte CRC
        Map<String, TpValue> result = TriePack.decode(buf);
        assertTrue(result.isEmpty());
    }

    @Test
    void testSingleKeyNull() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("hello", TpValue.ofNull());
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.size());
        assertEquals(TpValue.Type.NULL, result.get("hello").getType());
    }

    @Test
    void testSingleKeyInteger() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("key", TpValue.ofUInt(42));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(42, result.get("key").uintValue());
    }

    @Test
    void testSingleKeyNegative() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("neg", TpValue.ofInt(-100));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(-100, result.get("neg").intValue());
    }

    @Test
    void testSingleKeyBoolTrue() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("flag", TpValue.ofBool(true));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertTrue(result.get("flag").boolValue());
    }

    @Test
    void testSingleKeyBoolFalse() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("flag", TpValue.ofBool(false));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertFalse(result.get("flag").boolValue());
    }

    @Test
    void testSingleKeyFloat64() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("pi", TpValue.ofFloat64(3.14159));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(3.14159, result.get("pi").float64Value(), 1e-10);
    }

    @Test
    void testSingleKeyString() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("greeting", TpValue.ofString("hello world"));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals("hello world", result.get("greeting").stringValue());
    }

    @Test
    void testSingleKeyBlob() {
        byte[] blob = new byte[]{(byte) 0xDE, (byte) 0xAD, (byte) 0xBE, (byte) 0xEF};
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("binary", TpValue.ofBlob(blob));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertArrayEquals(blob, result.get("binary").blobValue());
    }

    @Test
    void testMultipleKeysMixed() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("bool", TpValue.ofBool(true));
        data.put("f64", TpValue.ofFloat64(3.14159));
        data.put("int", TpValue.ofInt(-100));
        data.put("str", TpValue.ofString("hello"));
        data.put("uint", TpValue.ofUInt(200));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertTrue(result.get("bool").boolValue());
        assertEquals(3.14159, result.get("f64").float64Value(), 1e-10);
        assertEquals(-100, result.get("int").intValue());
        assertEquals("hello", result.get("str").stringValue());
        assertEquals(200, result.get("uint").uintValue());
    }

    @Test
    void testSharedPrefix() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("abc", TpValue.ofUInt(10));
        data.put("abd", TpValue.ofUInt(20));
        data.put("xyz", TpValue.ofUInt(30));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(10, result.get("abc").uintValue());
        assertEquals(20, result.get("abd").uintValue());
        assertEquals(30, result.get("xyz").uintValue());
    }

    @Test
    void testKeysOnlyNull() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("apple", TpValue.ofNull());
        data.put("banana", TpValue.ofNull());
        data.put("cherry", TpValue.ofNull());
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(3, result.size());
        assertEquals(TpValue.Type.NULL, result.get("apple").getType());
        assertEquals(TpValue.Type.NULL, result.get("banana").getType());
        assertEquals(TpValue.Type.NULL, result.get("cherry").getType());
    }

    @Test
    void testLargeDictionary() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        for (int i = 0; i < 100; i++) {
            data.put(String.format("key_%04d", i), TpValue.ofUInt(i));
        }
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(100, result.size());
        for (int i = 0; i < 100; i++) {
            assertEquals(i, result.get(String.format("key_%04d", i)).uintValue());
        }
    }

    // ── Header validation tests ────────────────────────────────────

    @Test
    void testMagicBytes() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("test", TpValue.ofUInt(1));
        byte[] buf = TriePack.encode(data);
        assertEquals(0x54, buf[0] & 0xFF); // T
        assertEquals(0x52, buf[1] & 0xFF); // R
        assertEquals(0x50, buf[2] & 0xFF); // P
        assertEquals(0x00, buf[3] & 0xFF);
    }

    @Test
    void testVersionHeader() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("test", TpValue.ofUInt(1));
        byte[] buf = TriePack.encode(data);
        assertEquals(1, buf[4] & 0xFF); // major
        assertEquals(0, buf[5] & 0xFF); // minor
    }

    // ── Error handling tests ───────────────────────────────────────

    @Test
    void testCrcCorruption() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("test", TpValue.ofUInt(1));
        byte[] buf = TriePack.encode(data);
        byte[] corrupted = buf.clone();
        corrupted[10] ^= 0x01;
        assertThrows(IllegalArgumentException.class, () -> TriePack.decode(corrupted));
    }

    @Test
    void testInvalidMagic() {
        byte[] buf = new byte[40];
        buf[0] = (byte) 0xFF;
        assertThrows(IllegalArgumentException.class, () -> TriePack.decode(buf));
    }

    @Test
    void testDecodeRejectsShort() {
        assertThrows(IllegalArgumentException.class, () -> TriePack.decode(new byte[10]));
    }

    @Test
    void testEncodeRejectsNull() {
        assertThrows(IllegalArgumentException.class, () -> TriePack.encode(null));
    }

    // ── UTF-8 tests ───────────────────────────────────────────────

    @Test
    void testUtf8Keys() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("caf\u00e9", TpValue.ofUInt(1));
        data.put("na\u00efve", TpValue.ofUInt(2));
        data.put("\u00fcber", TpValue.ofUInt(3));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.get("caf\u00e9").uintValue());
        assertEquals(2, result.get("na\u00efve").uintValue());
        assertEquals(3, result.get("\u00fcber").uintValue());
    }

    @Test
    void testSingleCharKeys() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofUInt(1));
        data.put("b", TpValue.ofUInt(2));
        data.put("c", TpValue.ofUInt(3));
        data.put("d", TpValue.ofUInt(4));
        data.put("e", TpValue.ofUInt(5));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(5, result.size());
        assertEquals(1, result.get("a").uintValue());
        assertEquals(5, result.get("e").uintValue());
    }

    @Test
    void testEmptyStringKey() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("", TpValue.ofUInt(99));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(99, result.get("").uintValue());
    }

    @Test
    void testLongSharedPrefix() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("aaaaaa1", TpValue.ofUInt(1));
        data.put("aaaaaa2", TpValue.ofUInt(2));
        data.put("aaaaaa3", TpValue.ofUInt(3));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(3, result.size());
        assertEquals(1, result.get("aaaaaa1").uintValue());
        assertEquals(2, result.get("aaaaaa2").uintValue());
        assertEquals(3, result.get("aaaaaa3").uintValue());
    }

    // ── Trie structure edge cases ─────────────────────────────────

    @Test
    void testPrefixKeyKeysOnly() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofNull());
        data.put("ab", TpValue.ofNull());
        data.put("ac", TpValue.ofNull());
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(3, result.size());
        assertTrue(result.containsKey("a"));
        assertTrue(result.containsKey("ab"));
        assertTrue(result.containsKey("ac"));
    }

    @Test
    void testPrefixKeyWithValues() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofUInt(1));
        data.put("ab", TpValue.ofUInt(2));
        data.put("ac", TpValue.ofUInt(3));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.get("a").uintValue());
        assertEquals(2, result.get("ab").uintValue());
        assertEquals(3, result.get("ac").uintValue());
    }

    @Test
    void testTerminalWithChildrenNonlast() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofUInt(1));
        data.put("ab", TpValue.ofUInt(2));
        data.put("b", TpValue.ofUInt(3));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.get("a").uintValue());
        assertEquals(2, result.get("ab").uintValue());
        assertEquals(3, result.get("b").uintValue());
    }

    @Test
    void testTerminalWithChildrenNullValue() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofNull());
        data.put("ab", TpValue.ofUInt(2));
        data.put("b", TpValue.ofUInt(3));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(TpValue.Type.NULL, result.get("a").getType());
        assertEquals(2, result.get("ab").uintValue());
        assertEquals(3, result.get("b").uintValue());
    }

    @Test
    void testNullValueInMixedDict() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("alpha", TpValue.ofNull());
        data.put("beta", TpValue.ofUInt(42));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(TpValue.Type.NULL, result.get("alpha").getType());
        assertEquals(42, result.get("beta").uintValue());
    }

    @Test
    void testSubtreeMultiEntryGroups() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofUInt(1));
        data.put("aba", TpValue.ofUInt(2));
        data.put("abb", TpValue.ofUInt(3));
        data.put("ac", TpValue.ofUInt(4));
        data.put("b", TpValue.ofUInt(5));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.get("a").uintValue());
        assertEquals(2, result.get("aba").uintValue());
        assertEquals(3, result.get("abb").uintValue());
        assertEquals(4, result.get("ac").uintValue());
        assertEquals(5, result.get("b").uintValue());
    }

    @Test
    void testNonTerminalMultiEntryChildren() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("aba", TpValue.ofUInt(1));
        data.put("abb", TpValue.ofUInt(2));
        data.put("ac", TpValue.ofUInt(3));
        data.put("b", TpValue.ofUInt(4));
        byte[] buf = TriePack.encode(data);
        Map<String, TpValue> result = TriePack.decode(buf);
        assertEquals(1, result.get("aba").uintValue());
        assertEquals(2, result.get("abb").uintValue());
        assertEquals(3, result.get("ac").uintValue());
        assertEquals(4, result.get("b").uintValue());
    }

    @Test
    void testDecodeBadVersion() {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("a", TpValue.ofUInt(1));
        byte[] buf = TriePack.encode(data).clone();
        buf[4] = 2; // bad version
        // Fix CRC
        int crcDataLen = buf.length - 4;
        long newCrc = Crc32.crc32(buf, 0, crcDataLen);
        buf[crcDataLen] = (byte) ((newCrc >>> 24) & 0xFF);
        buf[crcDataLen + 1] = (byte) ((newCrc >>> 16) & 0xFF);
        buf[crcDataLen + 2] = (byte) ((newCrc >>> 8) & 0xFF);
        buf[crcDataLen + 3] = (byte) (newCrc & 0xFF);
        assertThrows(IllegalArgumentException.class, () -> TriePack.decode(buf));
    }
}
