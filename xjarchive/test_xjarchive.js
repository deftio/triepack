'use strict';
/**
 * xjarchive conformance tests (JavaScript).
 *
 * Two things are checked. The shared vectors in vectors.json, which the
 * Python implementation produced and this one must reproduce byte for byte --
 * that is what makes the grammar testable rather than merely written down.
 * And a randomised round-trip, which is what finds the case nobody thought
 * of.
 *
 * No test framework: node test_xjarchive.js
 *
 * Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
 */
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const T = require('./xjarchive');

let pass = 0;
function check(name, fn) {
    try {
        fn();
        pass++;
    } catch (e) {
        console.error(`FAIL  ${name}\n      ${e.message}`);
        process.exitCode = 1;
    }
}

// --- shared vectors ------------------------------------------------------

function fromJson(n) {
    if (n.text !== undefined) return Buffer.from(n.text, 'hex');
    return new T.Element(
        Buffer.from(n.tag, 'hex'),
        n.attrs.map(([k, v]) => [Buffer.from(k, 'hex'), Buffer.from(v, 'hex')]),
        n.children.map(fromJson)
    );
}

function same(a, b) {
    if (!(a instanceof T.Element) || !(b instanceof T.Element)) {
        return Buffer.from(a).equals(Buffer.from(b));
    }
    return (
        a.tag.equals(b.tag) &&
        a.attrs.length === b.attrs.length &&
        a.attrs.every(([k, v], i) => k.equals(b.attrs[i][0]) && v.equals(b.attrs[i][1])) &&
        a.children.length === b.children.length &&
        a.children.every((c, i) => same(c, b.children[i]))
    );
}

const vectors = JSON.parse(fs.readFileSync(path.join(__dirname, 'vectors.json'), 'utf8'));
for (const c of vectors.cases) {
    check(`vector encode: ${c.name}`, () => {
        const got = T.encode(fromJson(c.tree)).toString('hex');
        assert.strictEqual(got, c.wire, `\n  expected ${c.wire}\n  got      ${got}`);
    });
    check(`vector decode: ${c.name}`, () => {
        const back = T.decode(Buffer.from(c.wire, 'hex'));
        assert.ok(same(fromJson(c.tree), back), 'decoded tree differs');
    });
}

// --- rejection (GRAMMAR §6) ---------------------------------------------

for (const [bad, why] of [
    ['{p,unterminated', 'truncation'],
    ['{p,bad\\escape}', 'unknown escape'],
    ['{a,[nokeyvalue],x}', 'attribute without colon'],
    ['{p,ok}trailing', 'trailing bytes'],
    ['', 'empty input'],
    ['{', 'bare brace'],
]) {
    check(`reject: ${why}`, () => {
        assert.throws(() => T.decode(Buffer.from(bad, 'binary')), T.XjarchiveError);
    });
}

check('reject: \\B length past end of input', () => {
    // \B with a varint length far larger than what follows -- the shape that
    // must be checked before allocating.
    const wire = Buffer.concat([Buffer.from('{d,\\B'), Buffer.from([0xff, 0xff, 0x7f]), Buffer.from('ab}')]);
    assert.throws(() => T.decode(wire), T.XjarchiveError);
});

check('reject: unterminated \\b run', () => {
    assert.throws(() => T.decode(Buffer.from('{d,\\babc')), T.XjarchiveError);
});

// --- randomised round-trip ----------------------------------------------

function mulberry(seed) {
    return function () {
        seed |= 0;
        seed = (seed + 0x6d2b79f5) | 0;
        let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

const ADVERSARIAL = [
    '', 'a', '{', '}', '[', ']', ',', ':', '\\', '{{', '}}', '[[', ']]',
    '\\\\', ',,', '::', '[lead', 'trail]', '\x00', '\xff', '\x00\xff',
    'see [[link]]', 'use {{tmpl}}', 'a,b:c]d',
].map((s) => Buffer.from(s, 'binary'));

function randomTree(rnd, depth = 0) {
    if (depth >= 4 || rnd() < 0.3) return ADVERSARIAL[Math.floor(rnd() * ADVERSARIAL.length)];
    const tags = ['a', 'p', 'div', 'x\x00y', 't,ag', 't{g'];
    const tag = Buffer.from(tags[Math.floor(rnd() * tags.length)], 'binary');
    const attrs = [];
    for (let i = 0; i < Math.floor(rnd() * 4); i++) {
        const k = ADVERSARIAL[Math.floor(rnd() * ADVERSARIAL.length)];
        attrs.push([k.length ? k : Buffer.from('k'), ADVERSARIAL[Math.floor(rnd() * ADVERSARIAL.length)]]);
    }
    const kids = [];
    for (let i = 0; i < Math.floor(rnd() * 5); i++) kids.push(randomTree(rnd, depth + 1));
    return new T.Element(tag, attrs, kids);
}

function canonical(n) {
    if (!(n instanceof T.Element)) return Buffer.from(n);
    const merged = [];
    for (const c of n.children.map(canonical)) {
        if (!(c instanceof T.Element) && merged.length && !(merged[merged.length - 1] instanceof T.Element)) {
            merged[merged.length - 1] = Buffer.concat([merged[merged.length - 1], c]);
        } else merged.push(c);
    }
    return new T.Element(n.tag, n.attrs, merged.filter((c) => c instanceof T.Element || c.length));
}

check('randomised round-trip (3000 trees)', () => {
    const rnd = mulberry(20260918);
    for (let i = 0; i < 3000; i++) {
        let tree = randomTree(rnd);
        if (!(tree instanceof T.Element)) tree = new T.Element(Buffer.from('root'), [], [tree]);
        const wire = T.encode(tree);
        const back = T.decode(wire);
        assert.ok(same(canonical(tree), back), `round-trip failed: ${wire.toString('binary')}`);
        assert.ok(T.encode(back).equals(wire), `not canonical: ${wire.toString('binary')}`);
    }
});

check('truncation at every offset is rejected', () => {
    const el = new T.Element(Buffer.from('a'), [[Buffer.from('k'), Buffer.from('v')]],
        [Buffer.from('text'), new T.Element(Buffer.from('b'), [], [Buffer.from('deep')])]);
    const wire = T.encode(el);
    for (let cut = 1; cut < wire.length; cut++) {
        assert.throws(() => T.decode(wire.subarray(0, cut)), T.XjarchiveError,
            `truncation at ${cut} decoded as whole`);
    }
});

console.log(`${pass} checks passed`);
