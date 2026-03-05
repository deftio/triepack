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

const val VERSION = "0.1.0"

// encode() and decode() are defined in Encoder.kt and Decoder.kt respectively.
// TpValue is defined in Values.kt.
// All are in the same package and thus accessible via this module.
