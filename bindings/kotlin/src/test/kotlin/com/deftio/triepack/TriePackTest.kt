/*
 * TriePackTest.kt
 *
 * Roundtrip tests -- mirrors Python test_triepack.py.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class TriePackTest {

    @Test
    fun testVersion() {
        assertEquals("0.1.0", VERSION)
    }

    @Test
    fun testEmptyObject() {
        val buf = encode(emptyMap())
        assertTrue(buf.size >= 36) // 32-byte header + 4-byte CRC
        val result = decode(buf)
        assertEquals(emptyMap(), result)
    }

    @Test
    fun testSingleKeyNull() {
        val data = mapOf<String, TpValue?>("hello" to null)
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(1, result.size)
        assertNull(result["hello"])
    }

    @Test
    fun testSingleKeyInteger() {
        val data = mapOf<String, TpValue?>("key" to TpValue.UInt(42))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.UInt(42), result["key"])
    }

    @Test
    fun testSingleKeyNegative() {
        val data = mapOf<String, TpValue?>("neg" to TpValue.Int(-100))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Int(-100), result["neg"])
    }

    @Test
    fun testSingleKeyBoolTrue() {
        val data = mapOf<String, TpValue?>("flag" to TpValue.Bool(true))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Bool(true), result["flag"])
    }

    @Test
    fun testSingleKeyBoolFalse() {
        val data = mapOf<String, TpValue?>("flag" to TpValue.Bool(false))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Bool(false), result["flag"])
    }

    @Test
    fun testSingleKeyFloat64() {
        val data = mapOf<String, TpValue?>("pi" to TpValue.Float64(3.14159))
        val buf = encode(data)
        val result = decode(buf)
        val v = result["pi"]
        assertNotNull(v)
        assertTrue(v is TpValue.Float64)
        assertTrue(abs(v.value - 3.14159) < 1e-10)
    }

    @Test
    fun testSingleKeyString() {
        val data = mapOf<String, TpValue?>("greeting" to TpValue.Str("hello world"))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Str("hello world"), result["greeting"])
    }

    @Test
    fun testSingleKeyBlob() {
        val blob = byteArrayOf(0xDE.toByte(), 0xAD.toByte(), 0xBE.toByte(), 0xEF.toByte())
        val data = mapOf<String, TpValue?>("binary" to TpValue.Blob(blob))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Blob(blob), result["binary"])
    }

    @Test
    fun testMultipleKeysMixed() {
        val data = mapOf<String, TpValue?>(
            "bool" to TpValue.Bool(true),
            "f64" to TpValue.Float64(3.14159),
            "int" to TpValue.Int(-100),
            "str" to TpValue.Str("hello"),
            "uint" to TpValue.UInt(200)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.Bool(true), result["bool"])
        val f64 = result["f64"]
        assertNotNull(f64)
        assertTrue(f64 is TpValue.Float64)
        assertTrue(abs(f64.value - 3.14159) < 1e-10)
        assertEquals(TpValue.Int(-100), result["int"])
        assertEquals(TpValue.Str("hello"), result["str"])
        assertEquals(TpValue.UInt(200), result["uint"])
    }

    @Test
    fun testSharedPrefix() {
        val data = mapOf<String, TpValue?>(
            "abc" to TpValue.UInt(10),
            "abd" to TpValue.UInt(20),
            "xyz" to TpValue.UInt(30)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.UInt(10), result["abc"])
        assertEquals(TpValue.UInt(20), result["abd"])
        assertEquals(TpValue.UInt(30), result["xyz"])
    }

    @Test
    fun testKeysOnlyNull() {
        val data = mapOf<String, TpValue?>(
            "apple" to null,
            "banana" to null,
            "cherry" to null
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(3, result.size)
        assertNull(result["apple"])
        assertNull(result["banana"])
        assertNull(result["cherry"])
    }

    @Test
    fun testLargeDictionary() {
        val data = mutableMapOf<String, TpValue?>()
        for (i in 0 until 100) {
            data["key_%04d".format(i)] = TpValue.UInt(i.toLong())
        }
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(100, result.size)
        for (i in 0 until 100) {
            assertEquals(TpValue.UInt(i.toLong()), result["key_%04d".format(i)])
        }
    }

    @Test
    fun testMagicBytes() {
        val buf = encode(mapOf("test" to TpValue.UInt(1)))
        assertEquals(0x54.toByte(), buf[0]) // T
        assertEquals(0x52.toByte(), buf[1]) // R
        assertEquals(0x50.toByte(), buf[2]) // P
        assertEquals(0x00.toByte(), buf[3])
    }

    @Test
    fun testVersionHeader() {
        val buf = encode(mapOf("test" to TpValue.UInt(1)))
        assertEquals(1.toByte(), buf[4]) // major
        assertEquals(0.toByte(), buf[5]) // minor
    }

    @Test
    fun testCrcCorruption() {
        val buf = encode(mapOf("test" to TpValue.UInt(1))).copyOf()
        buf[10] = (buf[10].toInt() xor 0x01).toByte()
        assertFailsWith<IllegalArgumentException>("CRC") {
            decode(buf)
        }
    }

    @Test
    fun testInvalidMagic() {
        val buf = ByteArray(40)
        buf[0] = 0xFF.toByte()
        assertFailsWith<IllegalArgumentException>("magic") {
            decode(buf)
        }
    }

    @Test
    fun testDecodeRejectsShort() {
        assertFailsWith<IllegalArgumentException> {
            decode(ByteArray(10))
        }
    }

    @Test
    fun testUtf8Keys() {
        val data = mapOf<String, TpValue?>(
            "caf\u00e9" to TpValue.UInt(1),
            "na\u00efve" to TpValue.UInt(2),
            "\u00fcber" to TpValue.UInt(3)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testSingleCharKeys() {
        val data = mapOf<String, TpValue?>(
            "a" to TpValue.UInt(1),
            "b" to TpValue.UInt(2),
            "c" to TpValue.UInt(3),
            "d" to TpValue.UInt(4),
            "e" to TpValue.UInt(5)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testEmptyStringKey() {
        val data = mapOf<String, TpValue?>("" to TpValue.UInt(99))
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(TpValue.UInt(99), result[""])
    }

    @Test
    fun testLongSharedPrefix() {
        val data = mapOf<String, TpValue?>(
            "aaaaaa1" to TpValue.UInt(1),
            "aaaaaa2" to TpValue.UInt(2),
            "aaaaaa3" to TpValue.UInt(3)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testDecodeBadVersion() {
        val buf = encode(mapOf("a" to TpValue.UInt(1))).copyOf()
        buf[4] = 2.toByte()
        // Recompute CRC
        val crcDataLen = buf.size - 4
        val newCrc = Crc32.compute(buf, 0, crcDataLen)
        buf[crcDataLen] = ((newCrc ushr 24) and 0xFFL).toByte()
        buf[crcDataLen + 1] = ((newCrc ushr 16) and 0xFFL).toByte()
        buf[crcDataLen + 2] = ((newCrc ushr 8) and 0xFFL).toByte()
        buf[crcDataLen + 3] = (newCrc and 0xFFL).toByte()
        assertFailsWith<IllegalArgumentException>("version") {
            decode(buf)
        }
    }

    @Test
    fun testPrefixKeyKeysOnly() {
        val data = mapOf<String, TpValue?>(
            "a" to null,
            "ab" to null,
            "ac" to null
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(3, result.size)
        assertTrue(result.containsKey("a"))
        assertTrue(result.containsKey("ab"))
        assertTrue(result.containsKey("ac"))
        for ((_, v) in result) {
            assertNull(v)
        }
    }

    @Test
    fun testPrefixKeyWithValues() {
        val data = mapOf<String, TpValue?>(
            "a" to TpValue.UInt(1),
            "ab" to TpValue.UInt(2),
            "ac" to TpValue.UInt(3)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testTerminalWithChildrenNonlast() {
        val data = mapOf<String, TpValue?>(
            "a" to TpValue.UInt(1),
            "ab" to TpValue.UInt(2),
            "b" to TpValue.UInt(3)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testTerminalWithChildrenNullValue() {
        val data = mapOf<String, TpValue?>(
            "a" to null,
            "ab" to TpValue.UInt(2),
            "b" to TpValue.UInt(3)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testNullValueInMixedDict() {
        val data = mapOf<String, TpValue?>(
            "alpha" to null,
            "beta" to TpValue.UInt(42)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testSubtreeMultiEntryGroups() {
        val data = mapOf<String, TpValue?>(
            "a" to TpValue.UInt(1),
            "aba" to TpValue.UInt(2),
            "abb" to TpValue.UInt(3),
            "ac" to TpValue.UInt(4),
            "b" to TpValue.UInt(5)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }

    @Test
    fun testNonTerminalMultiEntryChildren() {
        val data = mapOf<String, TpValue?>(
            "aba" to TpValue.UInt(1),
            "abb" to TpValue.UInt(2),
            "ac" to TpValue.UInt(3),
            "b" to TpValue.UInt(4)
        )
        val buf = encode(data)
        val result = decode(buf)
        assertEquals(data, result)
    }
}
