/*
 * ConformanceTest.kt
 *
 * Cross-language conformance suite.
 *
 * Reads tests/conformance/cases.txt -- the corpus every implementation shares
 * -- and checks this binding against the C-generated fixtures: decoding a
 * fixture must produce the corpus values, and encoding the corpus values must
 * produce the fixture byte for byte.
 *
 * See tests/conformance/README.md.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

import java.io.File
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ConformanceTest {

    // ── Corpus location ───────────────────────────────────────────────

    private val conformanceDir: File by lazy {
        // Tests run from bindings/kotlin; walk up to the repository root.
        var dir: File? = File("").absoluteFile
        while (dir != null) {
            val candidate = File(File(dir, "tests"), "conformance")
            if (candidate.isDirectory) return@lazy candidate
            dir = dir.parentFile
        }
        error("cannot locate tests/conformance")
    }

    private fun fixtureFile(name: String) = File(File(conformanceDir, "fixtures"), "$name.trp")

    // ── Corpus parsing ────────────────────────────────────────────────

    /** Decode a corpus token ("~" for empty, %XX escapes) into bytes. */
    private fun tokenBytes(tok: String): ByteArray {
        if (tok == "~") return ByteArray(0)
        val out = ArrayList<Byte>(tok.length)
        var i = 0
        while (i < tok.length) {
            if (tok[i] == '%') {
                out.add(tok.substring(i + 1, i + 3).toInt(16).toByte())
                i += 3
            } else {
                out.add(tok[i].code.toByte())
                i += 1
            }
        }
        return out.toByteArray()
    }

    private fun tokenString(tok: String) = String(tokenBytes(tok), Charsets.UTF_8)

    private fun hexBytes(tok: String): ByteArray {
        if (tok == "~") return ByteArray(0)
        return ByteArray(tok.length / 2) { tok.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
    }

    private data class Entry(val key: String, val type: String, val arg: String?)

    private data class Case(
        val name: String,
        val encodeExpected: Boolean,
        val entries: MutableList<Entry> = mutableListOf()
    )

    private fun parseCases(): List<Case> {
        val cases = mutableListOf<Case>()
        File(conformanceDir, "cases.txt").forEachLine { raw ->
            val line = raw.trim()
            if (line.isNotEmpty() && !line.startsWith("#")) {
                val parts = line.split(" ")
                when (parts[0]) {
                    "case" -> cases.add(Case(parts[1], !parts.contains("encode=no")))
                    "key" -> cases.last().entries.add(
                        Entry(tokenString(parts[1]), parts[2], parts.getOrNull(3))
                    )
                    else -> error("unknown corpus directive: ${parts[0]}")
                }
            }
        }
        return cases
    }

    /** Build the Kotlin value a corpus entry describes. */
    private fun corpusValue(type: String, arg: String?): TpValue? = when (type) {
        "null" -> null
        "bool" -> TpValue.Bool(arg == "1")
        "int" -> TpValue.Int(arg!!.toLong())
        // uint64 values above Long.MAX_VALUE arrive as the matching negative
        // bit pattern, which is how TpValue carries them.
        "uint" -> TpValue.UInt(java.lang.Long.parseUnsignedLong(arg!!))
        "f64" -> TpValue.Float64(
            java.lang.Double.longBitsToDouble(java.lang.Long.parseUnsignedLong(arg!!, 16))
        )
        "f32" -> TpValue.Float64(
            java.lang.Float.intBitsToFloat(java.lang.Long.parseUnsignedLong(arg!!, 16).toInt())
                .toDouble()
        )
        "str" -> TpValue.Str(tokenString(arg!!))
        "blob" -> TpValue.Blob(hexBytes(arg!!))
        else -> error("unknown corpus type: $type")
    }

    private fun caseData(c: Case): Map<String, TpValue?> =
        c.entries.associate { it.key to corpusValue(it.type, it.arg) }

    /**
     * Doubles compare by bit pattern so -0.0 is distinguished, with all NaNs
     * treated as equal.
     */
    private fun valuesMatch(got: TpValue?, want: TpValue?): Boolean {
        if (want is TpValue.Float64 && got is TpValue.Float64) {
            if (got.value.isNaN() && want.value.isNaN()) return true
            return got.value.toRawBits() == want.value.toRawBits()
        }
        return got == want
    }

    // ── The suite ─────────────────────────────────────────────────────

    @Test
    fun testCorpusAndFixturesPresent() {
        val cases = parseCases()
        assertTrue(cases.isNotEmpty(), "corpus is empty")
        for (c in cases) {
            assertTrue(fixtureFile(c.name).exists(), "case ${c.name}: missing fixture")
        }
    }

    @Test
    fun testDecodesCFixtures() {
        for (c in parseCases()) {
            val result = decode(fixtureFile(c.name).readBytes())
            val want = caseData(c)
            assertEquals(want.size, result.size, "case ${c.name}: key count")
            for ((k, v) in want) {
                assertTrue(
                    valuesMatch(result[k], v),
                    "case ${c.name}: key $k: got ${result[k]}, want $v"
                )
            }
        }
    }

    /**
     * Each file is a valid fixture with one field damaged. Damage inside the
     * data is re-sealed with a correct CRC, so the reader has to catch it
     * rather than being handed a checksum failure.
     */
    @Test
    fun testRejectsMalformed() {
        val dir = File(conformanceDir, "malformed")
        val files = dir.listFiles { f: File -> f.name.endsWith(".trp") }?.sortedBy { it.name }
            ?: emptyList()
        assertTrue(files.isNotEmpty(), "malformed corpus is empty")
        for (f in files) {
            val bytes = f.readBytes()
            var rejected = false
            try {
                decode(bytes)
            } catch (e: Exception) {
                rejected = true
            }
            assertTrue(rejected, "${f.name}: accepted malformed input")
        }
    }

    @Test
    fun testEncodesLikeCReference() {
        for (c in parseCases()) {
            // Cases marked encode=no describe values this binding cannot
            // reproduce exactly (float32, or doubles some languages read back
            // as integers).
            if (!c.encodeExpected) continue
            assertContentEquals(
                fixtureFile(c.name).readBytes(),
                encode(caseData(c)),
                "case ${c.name}"
            )
        }
    }
}
