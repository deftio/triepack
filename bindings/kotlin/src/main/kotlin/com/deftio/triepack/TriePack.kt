/*
 * TriePack.kt
 *
 * Native Kotlin implementation of the TriePack (.trp) binary format.
 * Public API: re-exports encode/decode and TpValue types.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack

const val VERSION = "1.2.0"

/**
 * Version of the on-disk .trp format this implementation writes. Distinct
 * from the library version: it changes only when the bytes change.
 */
const val FORMAT_VERSION_MAJOR: Int = 1

/** See [FORMAT_VERSION_MAJOR]. */
const val FORMAT_VERSION_MINOR: Int = 0

/**
 * Metadata about a triepack build. Every implementation reports the same
 * fields, so a polyglot system can ask each one what it is.
 */
data class VersionInfo(
    val name: String,
    val implementation: String,
    val version: String,
    val versionMajor: Int,
    val versionMinor: Int,
    val versionPatch: Int,
    val formatVersionMajor: Int,
    val formatVersionMinor: Int,
    val maxAlphabetSize: Int
)

/** Return metadata about this build. */
fun version(): VersionInfo {
    val parts = VERSION.split(".").map { it.toInt() }
    return VersionInfo(
        name = "triepack",
        implementation = "kotlin",
        version = VERSION,
        versionMajor = parts[0],
        versionMinor = parts[1],
        versionPatch = parts[2],
        formatVersionMajor = FORMAT_VERSION_MAJOR,
        formatVersionMinor = FORMAT_VERSION_MINOR,
        maxAlphabetSize = MAX_ALPHABET_SIZE
    )
}

// encode() and decode() are defined in Encoder.kt and Decoder.kt respectively.
// TpValue is defined in Values.kt.
// All are in the same package and thus accessible via this module.
