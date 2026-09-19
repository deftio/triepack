'use strict';
/**
 * xjarchive -- reference encoder and decoder (JavaScript).
 *
 * Implements GRAMMAR.md exactly. Where this code and the grammar disagree,
 * the grammar is right and this is a bug.
 *
 * A node is either an Element, or a Uint8Array of text/binary. Everything is
 * bytes: a xjarchive document is a byte string and the decoder must not assume
 * UTF-8, because an encoding that only works for text is one that silently
 * corrupts binary.
 *
 * This is deliberately a second independent implementation rather than a
 * binding over the Python one. Two implementations that can disagree, checked
 * against shared vectors, is the only way to find out whether the grammar is
 * actually unambiguous.
 *
 * Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
 */

const IMPLICIT_TAG = Buffer.from('#text');
const MAX_DEPTH = 256;
const VARINT_MAX_GROUPS = 10;

class XjarchiveError extends Error {}

class Element {
    constructor(tag, attrs = [], children = []) {
        this.tag = Buffer.from(tag);
        this.attrs = attrs.map(([k, v]) => [Buffer.from(k), Buffer.from(v)]);
        this.children = children.map((c) => (c instanceof Element ? c : Buffer.from(c)));
    }
}

// --- bytes ---------------------------------------------------------------

const BSLASH = 0x5c;
const NUL = 0x00;

function isSet(set, byte) {
    return set.indexOf(byte) !== -1;
}

/** Escape exactly the reserved bytes and no others (grammar §5, §7 rule 4). */
function escapeBytes(raw, reserved, escapeLeading) {
    const out = [];
    for (let i = 0; i < raw.length; i++) {
        const b = raw[i];
        const lead = i === 0 && escapeLeading !== undefined && b === escapeLeading;
        if (b === BSLASH || isSet(reserved, b) || lead) out.push(BSLASH);
        out.push(b);
    }
    return Buffer.from(out);
}

function varint(n) {
    const out = [];
    for (;;) {
        const b = n & 0x7f;
        n = Math.floor(n / 128);
        out.push(n ? b | 0x80 : b);
        if (!n) return Buffer.from(out);
    }
}

function readVarint(buf, i) {
    let n = 0;
    let shift = 1;
    let groups = 0;
    for (;;) {
        if (i >= buf.length) throw new XjarchiveError('truncated varint');
        if (++groups > VARINT_MAX_GROUPS) throw new XjarchiveError('varint longer than 10 groups');
        const b = buf[i++];
        n += (b & 0x7f) * shift;
        shift *= 128;
        if (!(b & 0x80)) return [n, i];
    }
}

// --- which binary form (grammar §5.1) ------------------------------------

function sizeInline(b) {
    let n = b.length;
    for (const x of b) if (x === 0x7b || x === 0x7d || x === BSLASH) n++;
    return n;
}

function sizeTerminated(b) {
    let n = 2 + b.length + 1;
    for (const x of b) if (x === NUL || x === BSLASH) n++;
    return n;
}

function sizeCounted(b) {
    return 2 + varint(b.length).length + b.length;
}

/**
 * Shortest wins; ties break counted, terminated, inline. Mandatory rather
 * than advisory -- three legal encodings of one payload would otherwise mean
 * three legal files for one document.
 */
function chooseForm(payload, leadingBracket = false) {
    const c = sizeCounted(payload);
    const t = sizeTerminated(payload);
    const i =
        sizeInline(payload) + (leadingBracket && payload.length && payload[0] === 0x5b ? 1 : 0);
    const best = Math.min(c, t, i);
    if (c === best) return 'counted';
    if (t === best) return 'terminated';
    return 'inline';
}

function encodeBinary(payload, form) {
    if (form === 'counted') {
        return Buffer.concat([Buffer.from('\\B'), varint(payload.length), payload]);
    }
    const body = [];
    for (const b of payload) {
        if (b === BSLASH) body.push(BSLASH, BSLASH);
        else if (b === NUL) body.push(BSLASH, 0x30); // \0
        else body.push(b);
    }
    return Buffer.concat([Buffer.from('\\b'), Buffer.from(body), Buffer.from([NUL])]);
}

// --- encoding ------------------------------------------------------------

/** §7 rule 5: nothing separates adjacent text on the wire. */
function mergeText(children) {
    const out = [];
    for (const c of children) {
        if (!(c instanceof Element) && out.length && !(out[out.length - 1] instanceof Element)) {
            out[out.length - 1] = Buffer.concat([out[out.length - 1], c]);
        } else {
            out.push(c);
        }
    }
    return out.filter((c) => c instanceof Element || c.length > 0);
}

const R_TAG = [0x2c, 0x7b, 0x7d];
const R_KEY = [0x3a, 0x2c, 0x5d];
const R_VAL = [0x2c, 0x5d];
const R_TEXT = [0x7b, 0x7d];

function encode(node, depth = 0) {
    if (depth > MAX_DEPTH) throw new XjarchiveError(`nesting deeper than ${MAX_DEPTH}`);

    if (!(node instanceof Element)) {
        return Buffer.concat([
            Buffer.from('{'),
            escapeBytes(node, R_TEXT, 0x5b),
            Buffer.from('}'),
        ]);
    }

    const parts = [Buffer.from('{'), escapeBytes(node.tag, R_TAG)];

    if (node.attrs.length) {
        const pairs = node.attrs.map(([k, v]) =>
            Buffer.concat([escapeBytes(k, R_KEY), Buffer.from(':'), escapeBytes(v, R_VAL)])
        );
        parts.push(Buffer.from(',['));
        pairs.forEach((p, i) => {
            if (i) parts.push(Buffer.from(','));
            parts.push(p);
        });
        parts.push(Buffer.from(']'));
    }

    const children = mergeText(node.children);
    if (children.length) {
        parts.push(Buffer.from(','));
        children.forEach((child, i) => {
            if (child instanceof Element) {
                parts.push(encode(child, depth + 1));
            } else {
                const atStart = i === 0 && !node.attrs.length;
                const form = chooseForm(child, atStart);
                parts.push(
                    form === 'inline'
                        ? escapeBytes(child, R_TEXT, atStart ? 0x5b : undefined)
                        : encodeBinary(child, form)
                );
            }
        });
    } else if (!node.tag.equals(IMPLICIT_TAG)) {
        parts.push(Buffer.from(','));
    }

    parts.push(Buffer.from('}'));
    return Buffer.concat(parts);
}

function encodeDocument(nodes) {
    return Buffer.concat(nodes.map((n) => encode(n)));
}

// --- decoding ------------------------------------------------------------

const VALID_ESCAPES = [0x7b, 0x7d, 0x5b, 0x5d, 0x2c, 0x3a, BSLASH];

class Reader {
    constructor(buf) {
        this.buf = buf;
        this.i = 0;
    }
    eof() {
        return this.i >= this.buf.length;
    }
    peek() {
        if (this.eof()) throw new XjarchiveError('unexpected end of input');
        return this.buf[this.i];
    }
    take() {
        const b = this.peek();
        this.i++;
        return b;
    }
    expect(byte) {
        const got = this.take();
        if (got !== byte) throw new XjarchiveError(`expected ${byte} at ${this.i - 1}, got ${got}`);
    }
}

function readEscaped(r, stop) {
    const out = [];
    for (;;) {
        if (r.eof()) throw new XjarchiveError('unterminated element');
        const ch = r.peek();
        if (ch === BSLASH) {
            r.take();
            const nxt = r.take();
            if (!isSet(VALID_ESCAPES, nxt)) {
                throw new XjarchiveError(`invalid escape \\${String.fromCharCode(nxt)}`);
            }
            out.push(nxt);
            continue;
        }
        if (isSet(stop, ch)) return Buffer.from(out);
        out.push(r.take());
    }
}

function readBinary(r) {
    r.expect(BSLASH);
    const kind = r.take();

    if (kind === 0x42) {
        // 'B'
        const [n, i] = readVarint(r.buf, r.i);
        // §6: validate the length against what remains BEFORE allocating.
        if (n > r.buf.length - i) {
            throw new XjarchiveError(`binary run claims ${n} bytes, ${r.buf.length - i} remain`);
        }
        r.i = i + n;
        return r.buf.subarray(i, i + n);
    }

    const out = [];
    for (;;) {
        if (r.eof()) throw new XjarchiveError('unterminated binary run');
        const ch = r.take();
        if (ch === NUL) return Buffer.from(out);
        if (ch === BSLASH) {
            const nxt = r.take();
            if (nxt === 0x30) out.push(NUL);
            else if (nxt === BSLASH) out.push(BSLASH);
            else throw new XjarchiveError(`invalid escape in binary run: \\${String.fromCharCode(nxt)}`);
            continue;
        }
        out.push(ch);
    }
}

function decodeElement(r, depth) {
    if (depth > MAX_DEPTH) throw new XjarchiveError(`nesting deeper than ${MAX_DEPTH}`);
    r.expect(0x7b); // {

    const head = readEscaped(r, [0x2c, 0x7b, 0x7d]);
    const ch = r.peek();

    if (ch === 0x7d) {
        r.take();
        return new Element(IMPLICIT_TAG, [], head.length ? [head] : []);
    }
    if (ch === 0x7b) {
        throw new XjarchiveError('element with children must have a comma after the tag');
    }

    r.expect(0x2c); // ,
    const el = new Element(head);

    if (!r.eof() && r.peek() === 0x5b) {
        r.take();
        if (r.peek() !== 0x5d) {
            for (;;) {
                const key = readEscaped(r, [0x3a, 0x2c, 0x5d]);
                if (r.peek() !== 0x3a) throw new XjarchiveError("attribute pair without ':'");
                r.take();
                const val = readEscaped(r, [0x2c, 0x5d]);
                el.attrs.push([key, val]);
                if (r.peek() === 0x2c) {
                    r.take();
                    if (r.peek() === 0x5d) break;
                    continue;
                }
                break;
            }
        }
        r.expect(0x5d);
        if (r.peek() === 0x2c) r.take();
        else if (r.peek() === 0x7d) {
            r.take();
            return el;
        }
    }

    for (;;) {
        if (r.eof()) throw new XjarchiveError('unterminated element');
        const c = r.peek();
        if (c === 0x7d) {
            r.take();
            return el;
        }
        if (c === 0x7b) {
            el.children.push(decodeElement(r, depth + 1));
            continue;
        }
        if (c === BSLASH && (r.buf[r.i + 1] === 0x62 || r.buf[r.i + 1] === 0x42)) {
            // §5.1: a letter after the escape byte introduces a command. The
            // reserved bytes are all punctuation, so the two cannot collide.
            el.children.push(readBinary(r));
            continue;
        }
        const text = readEscaped(r, [0x7b, 0x7d]);
        if (text.length) el.children.push(text);
    }
}

function decode(buf) {
    const r = new Reader(Buffer.from(buf));
    const el = decodeElement(r, 0);
    if (!r.eof()) throw new XjarchiveError(`trailing bytes at offset ${r.i}`);
    return el;
}

function decodeDocument(buf) {
    const r = new Reader(Buffer.from(buf));
    const out = [];
    while (!r.eof()) out.push(decodeElement(r, 0));
    return out;
}

module.exports = {
    Element,
    XjarchiveError,
    IMPLICIT_TAG,
    MAX_DEPTH,
    encode,
    decode,
    encodeDocument,
    decodeDocument,
    chooseForm,
};
