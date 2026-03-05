/*
 * FixturesTest.kt
 *
 * Cross-language fixture tests -- mirrors Python test_fixtures.py.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

import java.io.File
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class FixturesTest {

    private fun fixtureDir(): String {
        // Navigate from bindings/kotlin/ up to tests/fixtures/
        val candidates = listOf(
            // Running from project root
            "tests/fixtures",
            // Running from bindings/kotlin
            "../../tests/fixtures",
            // Absolute path fallback
            System.getProperty("user.dir") + "/../../tests/fixtures"
        )
        for (c in candidates) {
            val f = File(c)
            if (f.exists() && f.isDirectory) return f.absolutePath
        }
        // Last resort: walk up from CWD
        var dir = File(System.getProperty("user.dir"))
        while (dir.parentFile != null) {
            val fixture = File(dir, "tests/fixtures")
            if (fixture.exists() && fixture.isDirectory) return fixture.absolutePath
            dir = dir.parentFile
        }
        throw RuntimeException("Cannot find tests/fixtures directory")
    }

    private fun readFixture(name: String): ByteArray {
        return File(fixtureDir(), name).readBytes()
    }

    // -- Decode C-generated fixtures --

    @Test
    fun testDecodeEmpty() {
        val buf = readFixture("empty.trp")
        val result = decode(buf)
        assertEquals(emptyMap(), result)
    }

    @Test
    fun testDecodeSingleNull() {
        val buf = readFixture("single_null.trp")
        val result = decode(buf)
        assertEquals(1, result.size)
        assertNull(result["hello"])
    }

    @Test
    fun testDecodeSingleInt() {
        val buf = readFixture("single_int.trp")
        val result = decode(buf)
        assertEquals(TpValue.UInt(42), result["key"])
    }

    @Test
    fun testDecodeMultiMixed() {
        val buf = readFixture("multi_mixed.trp")
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
    fun testDecodeSharedPrefix() {
        val buf = readFixture("shared_prefix.trp")
        val result = decode(buf)
        assertEquals(TpValue.UInt(10), result["abc"])
        assertEquals(TpValue.UInt(20), result["abd"])
        assertEquals(TpValue.UInt(30), result["xyz"])
    }

    @Test
    fun testDecodeLarge() {
        val buf = readFixture("large.trp")
        val result = decode(buf)
        assertEquals(100, result.size)
        for (i in 0 until 100) {
            val key = "key_%04d".format(i)
            assertEquals(TpValue.UInt(i.toLong()), result[key])
        }
    }

    @Test
    fun testDecodeKeysOnly() {
        val buf = readFixture("keys_only.trp")
        val result = decode(buf)
        assertEquals(3, result.size)
        assertNull(result["apple"])
        assertNull(result["banana"])
        assertNull(result["cherry"])
    }

    // -- Byte-for-byte binary reproducibility --

    @Test
    fun testEncodeEmptyMatchesFixture() {
        val ktBuf = encode(emptyMap())
        val cBuf = readFixture("empty.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeSingleNullMatchesFixture() {
        val ktBuf = encode(mapOf("hello" to null))
        val cBuf = readFixture("single_null.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeSingleIntMatchesFixture() {
        val ktBuf = encode(mapOf("key" to TpValue.UInt(42)))
        val cBuf = readFixture("single_int.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeMultiMixedMatchesFixture() {
        val ktBuf = encode(mapOf(
            "bool" to TpValue.Bool(true),
            "f64" to TpValue.Float64(3.14159),
            "int" to TpValue.Int(-100),
            "str" to TpValue.Str("hello"),
            "uint" to TpValue.UInt(200)
        ))
        val cBuf = readFixture("multi_mixed.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeSharedPrefixMatchesFixture() {
        val ktBuf = encode(mapOf(
            "abc" to TpValue.UInt(10),
            "abd" to TpValue.UInt(20),
            "xyz" to TpValue.UInt(30)
        ))
        val cBuf = readFixture("shared_prefix.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeLargeMatchesFixture() {
        val data = mutableMapOf<String, TpValue?>()
        for (i in 0 until 100) {
            data["key_%04d".format(i)] = TpValue.UInt(i.toLong())
        }
        val ktBuf = encode(data)
        val cBuf = readFixture("large.trp")
        assertContentEquals(cBuf, ktBuf)
    }

    @Test
    fun testEncodeKeysOnlyMatchesFixture() {
        val ktBuf = encode(mapOf(
            "apple" to null,
            "banana" to null,
            "cherry" to null
        ))
        val cBuf = readFixture("keys_only.trp")
        assertContentEquals(cBuf, ktBuf)
    }
}
