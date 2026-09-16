// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — Native JavaScript implementation of the Triepack .trp binary format.
 */

'use strict';

const { encode, MAX_ALPHABET_SIZE } = require('./encoder');
const { decode } = require('./decoder');

// Kept in step with triepack-version.txt by scripts/sync_version.sh.
const VERSION = '1.3.2';

// Version of the on-disk .trp format this implementation writes. Distinct
// from the library version: it changes only when the bytes change.
const FORMAT_VERSION_MAJOR = 1;
const FORMAT_VERSION_MINOR = 0;

/**
 * Metadata about this build. Every triepack implementation answers with the
 * same shape, so a polyglot system can ask each one what it is.
 *
 * @returns {{name: string, implementation: string, version: string,
 *   versionMajor: number, versionMinor: number, versionPatch: number,
 *   formatVersionMajor: number, formatVersionMinor: number,
 *   maxAlphabetSize: number}}
 */
function version() {
    const [major, minor, patch] = VERSION.split('.').map(Number);
    return {
        name: 'triepack',
        implementation: 'javascript',
        version: VERSION,
        versionMajor: major,
        versionMinor: minor,
        versionPatch: patch,
        formatVersionMajor: FORMAT_VERSION_MAJOR,
        formatVersionMinor: FORMAT_VERSION_MINOR,
        maxAlphabetSize: MAX_ALPHABET_SIZE,
    };
}

module.exports = { encode, decode, version, VERSION, MAX_ALPHABET_SIZE };
