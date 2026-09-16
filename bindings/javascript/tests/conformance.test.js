'use strict';

/**
 * Cross-language conformance suite.
 *
 * Reads tests/conformance/cases.txt — the corpus every implementation shares —
 * and checks this binding against the C-generated fixtures: decoding a fixture
 * must produce the corpus values, and encoding the corpus values must produce
 * the fixture byte for byte.
 *
 * See tests/conformance/README.md.
 */

const fs = require('fs');
const path = require('path');
const { encode, decode } = require('../src/index');

const CONFORMANCE_DIR = path.resolve(__dirname, '../../../tests/conformance');
const CASES_FILE = path.join(CONFORMANCE_DIR, 'cases.txt');
const FIXTURE_DIR = path.join(CONFORMANCE_DIR, 'fixtures');
const MALFORMED_DIR = path.join(CONFORMANCE_DIR, 'malformed');

// ── Corpus parsing ───────────────────────────────────────────────────────

/** Decode a corpus token ("~" for empty, %XX escapes) into a Buffer. */
function tokenBytes(tok) {
    if (tok === '~') return Buffer.alloc(0);
    const out = [];
    for (let i = 0; i < tok.length;) {
        if (tok[i] === '%') {
            out.push(parseInt(tok.substr(i + 1, 2), 16));
            i += 3;
        } else {
            out.push(tok.charCodeAt(i));
            i += 1;
        }
    }
    return Buffer.from(out);
}

function tokenString(tok) {
    return tokenBytes(tok).toString('utf8');
}

function hexBytes(tok) {
    if (tok === '~') return Buffer.alloc(0);
    return Buffer.from(tok, 'hex');
}

function f64FromHex(tok) {
    const b = Buffer.from(tok, 'hex');
    return b.readDoubleBE(0);
}

function f32FromHex(tok) {
    const b = Buffer.from(tok, 'hex');
    return b.readFloatBE(0);
}

function parseCases(text) {
    const cases = [];
    let cur = null;
    for (const raw of text.split('\n')) {
        const line = raw.trim();
        if (!line || line.startsWith('#')) continue;
        const parts = line.split(' ');
        if (parts[0] === 'case') {
            cur = {
                name: parts[1],
                encodeExpected: !parts.includes('encode=no'),
                // JavaScript holds integers in a double, so cases that need
                // the full 64-bit range are not this binding's to check.
                skip: parts.includes('requires=int64'),
                keys: [],
            };
            cases.push(cur);
        } else if (parts[0] === 'key') {
            cur.keys.push({ key: tokenString(parts[1]), type: parts[2], arg: parts[3] });
        } else {
            throw new Error('unknown corpus directive: ' + parts[0]);
        }
    }
    return cases;
}

/** Build the JS value a corpus entry describes. */
function caseValue(entry) {
    switch (entry.type) {
        case 'null': return null;
        case 'bool': return entry.arg === '1';
        case 'int': return Number(entry.arg);
        case 'uint': return Number(entry.arg);
        case 'f64': return f64FromHex(entry.arg);
        case 'f32': return f32FromHex(entry.arg);
        case 'str': return tokenString(entry.arg);
        case 'blob': return hexBytes(entry.arg);
        default: throw new Error('unknown corpus type: ' + entry.type);
    }
}

function caseData(c) {
    const data = {};
    for (const entry of c.keys) data[entry.key] = caseValue(entry);
    return data;
}

/**
 * Compare a decoded value with the expected one. Doubles compare by bit
 * pattern so -0.0 and NaN are handled, with all NaNs treated as equal.
 */
function valuesMatch(got, want) {
    if (want === null) return got === null;
    if (typeof want === 'boolean') return got === want;
    if (typeof want === 'number') {
        if (typeof got !== 'number') return false;
        if (Number.isNaN(want) && Number.isNaN(got)) return true;
        return Object.is(got, want);
    }
    if (typeof want === 'string') return got === want;
    if (want instanceof Uint8Array) {
        return got instanceof Uint8Array && Buffer.from(got).equals(Buffer.from(want));
    }
    return false;
}

// ── The suite ────────────────────────────────────────────────────────────

const cases = parseCases(fs.readFileSync(CASES_FILE, 'utf8'));

describe('cross-language conformance corpus', () => {
    test('corpus and fixtures are present', () => {
        expect(cases.length).toBeGreaterThan(0);
        for (const c of cases) {
            expect(fs.existsSync(path.join(FIXTURE_DIR, c.name + '.trp'))).toBe(true);
        }
    });

    describe.each(cases.map(c => [c.name, c]))('%s', (_name, c) => {
        const fixture = () => new Uint8Array(fs.readFileSync(path.join(FIXTURE_DIR, c.name + '.trp')));

        const decodeTest = c.skip ? test.skip : test;
        decodeTest('decodes the C fixture to the corpus values', () => {
            const result = decode(fixture());
            const want = caseData(c);
            expect(Object.keys(result).sort()).toEqual(Object.keys(want).sort());
            for (const k of Object.keys(want)) {
                expect(valuesMatch(result[k], want[k])).toBe(true);
            }
        });

        // Cases marked encode=no describe values this binding cannot
        // reproduce exactly (float32, or doubles JS reads back as integers).
        const encodeTest = c.encodeExpected && !c.skip ? test : test.skip;
        encodeTest('encodes byte-for-byte like the C reference', () => {
            const got = Buffer.from(encode(caseData(c)));
            expect(got.equals(Buffer.from(fixture()))).toBe(true);
        });
    });
});

// Every buffer here is a valid fixture with one field damaged. Damage inside
// the data is re-sealed with a correct CRC, so the reader has to catch it
// rather than being handed a checksum failure.
describe('malformed inputs are rejected', () => {
    const files = fs.readdirSync(MALFORMED_DIR).filter(f => f.endsWith('.trp')).sort();

    test('the malformed corpus is present', () => {
        expect(files.length).toBeGreaterThan(0);
    });

    test.each(files)('%s', name => {
        const buf = new Uint8Array(fs.readFileSync(path.join(MALFORMED_DIR, name)));
        expect(() => decode(buf)).toThrow();
    });
});
