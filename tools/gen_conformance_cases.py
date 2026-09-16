#!/usr/bin/env python3
# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""Generate tests/conformance/cases.txt, the cross-language conformance corpus.

The corpus is the single source of truth for what every implementation must
encode and decode identically. tools/generate_conformance.c turns it into .trp
fixtures using the C reference encoder; each binding has a harness that reads
the same file and checks itself against those fixtures.

Run from the repository root:

    python3 tools/gen_conformance_cases.py

See tests/conformance/README.md for the file format.
"""

import os
import struct

SAFE = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")


def enc(s):
    """Escape a str (or bytes) as a corpus token: ~ for empty, %XX otherwise."""
    raw = s.encode("utf-8") if isinstance(s, str) else s
    if not raw:
        return "~"
    out = []
    for b in raw:
        c = chr(b)
        out.append(c if c in SAFE else "%%%02X" % b)
    return "".join(out)


def f64bits(x):
    return "%016X" % struct.unpack(">Q", struct.pack(">d", x))[0]


def f32bits(x):
    return "%08X" % struct.unpack(">I", struct.pack(">f", x))[0]


class Case:
    def __init__(self, name, encode=True, note=None, requires=None):
        self.name = name
        self.encode = encode
        self.note = note
        # "int64": values need more than 53 bits, so implementations that hold
        # integers in a double (JavaScript) skip the case.
        self.requires = requires
        self.keys = []

    def null(self, k):
        self.keys.append((k, "null", None))
        return self

    def boolean(self, k, v):
        self.keys.append((k, "bool", "1" if v else "0"))
        return self

    def sint(self, k, v):
        # JS cannot tell a non-negative int from a uint, so the corpus only
        # ever uses the signed tag for negative values.
        assert v < 0, "use uint for non-negative integers"
        self.keys.append((k, "int", str(v)))
        return self

    def f64_is_integral(self, v):
        return float(v).is_integer()

    def uint(self, k, v):
        assert v >= 0
        self.keys.append((k, "uint", str(v)))
        return self

    def f64(self, k, v):
        # An integral double is indistinguishable from an integer in JS, which
        # would encode it with the uint tag; keep those in a decode-only case.
        assert not self.encode or not float(v).is_integer(), (
            "integral double %r needs a decode-only case" % v
        )
        self.keys.append((k, "f64", f64bits(v)))
        return self

    def f64raw(self, k, bits):
        self.keys.append((k, "f64", "%016X" % bits))
        return self

    def f32(self, k, v):
        self.keys.append((k, "f32", f32bits(v)))
        return self

    def text(self, k, v):
        self.keys.append((k, "str", enc(v)))
        return self

    def blob(self, k, v):
        self.keys.append((k, "blob", v.hex().upper() if v else "~"))
        return self

    def render(self):
        lines = []
        if self.note:
            lines.append("# %s" % self.note)
        head = "case %s" % self.name
        if not self.encode:
            head += " encode=no"
        if self.requires:
            head += " requires=%s" % self.requires
        lines.append(head)
        for k, t, arg in self.keys:
            line = "key %s %s" % (enc(k), t)
            if arg is not None:
                line += " " + arg
            lines.append(line)
        return "\n".join(lines)


def build():
    cases = []

    # -- The original tests/fixtures set, so the two stay in step ----------
    cases.append(Case("empty", note="Original fixture set (tests/fixtures/*.trp)"))
    cases.append(Case("single_null").null("hello"))
    cases.append(Case("single_int").uint("key", 42))
    c = Case("multi_mixed")
    c.boolean("bool", True).f64("f64", 3.14159).sint("int", -100)
    c.text("str", "hello").uint("uint", 200)
    cases.append(c)
    cases.append(Case("shared_prefix").uint("abc", 10).uint("abd", 20).uint("xyz", 30))
    c = Case("large")
    for i in range(100):
        c.uint("key_%04d" % i, i)
    cases.append(c)
    cases.append(Case("keys_only").null("apple").null("banana").null("cherry"))

    # -- Trie shapes -------------------------------------------------------
    cases.append(Case("empty_key", note="Trie shapes").null(""))
    cases.append(Case("empty_key_with_siblings").null("").null("a").null("ab"))
    c = Case("single_char_keys")
    for ch in "abcdefghijklmnopqrstuvwxyz":
        c.uint(ch, ord(ch))
    cases.append(c)
    cases.append(Case("terminal_with_children").null("a").null("ab").null("abc"))
    cases.append(
        Case("terminal_with_children_valued").uint("a", 1).uint("ab", 2).uint("abc", 3)
    )
    c = Case("prefix_chain")
    for n in range(1, 21):
        c.uint("a" * n, n)
    cases.append(c)
    cases.append(Case("deep_chain").uint("z" * 200, 1))
    cases.append(Case("one_branch_two_leaves").null("ax").null("ay"))
    cases.append(Case("no_common_prefix").null("aaa").null("bbb").null("ccc"))
    c = Case("wide_branch")
    for i in range(64):
        c.uint("k%02d" % i, i)
    cases.append(c)

    # Issue #1: trailing bits that look like a BRANCH code.
    trailing = [
        ["eddb", "h"],
        ["aad", "ebg", "ec", "ehhbf", "h", "hebad"],
        ["aghed", "bae", "bfffa", "cad", "d", "ebbhb", "fa", "feb"],
        ["a", "aad", "bg", "bgc", "egba", "gahad", "ghehg", "hbdab", "hg"],
    ]
    for i, keys in enumerate(trailing):
        note = None
        if i == 0:
            note = "Issue #1: padding/CRC after the last terminal can look like BRANCH"
        c = Case("trailing_branch_%d" % i, note=note)
        for k in keys:
            c.null(k)
        cases.append(c)
        c = Case("trailing_branch_%d_valued" % i)
        for j, k in enumerate(keys):
            c.uint(k, j)
        cases.append(c)

    # -- bits_per_symbol boundaries ----------------------------------------
    # bps is ceil(log2(alphabet + 6)); step the alphabet across each power.
    pool = (
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!#$%&()*+,-./:;<=>?@[]^_{|}~"
    )
    for size, bps in [(2, 3), (10, 4), (26, 5), (58, 6)]:
        letters = pool[:size]
        assert len(letters) == size
        c = Case(
            "bps%d_alphabet%d" % (bps, size),
            note="bits_per_symbol boundary: %d distinct key bytes -> bps %d"
            % (size, bps),
        )
        for i, ch in enumerate(letters):
            c.uint(ch, i)
        cases.append(c)

    # Wider alphabets need multi-byte characters; walk code points until the
    # set of distinct UTF-8 bytes reaches the target.
    def wide_alphabet_case(name, target_bytes, note=None):
        c = Case(name, note=note)
        seen = set()
        i = 0
        cp = 0x21
        while len(seen) < target_bytes and cp < 0x30000:
            ch = chr(cp)
            cp += 1
            if not ch.isprintable():
                continue
            b = set(ch.encode("utf-8"))
            if b <= seen:
                continue
            if len(seen | b) > target_bytes:
                continue
            seen |= b
            c.uint("k%03d%s" % (i, ch), i)
            i += 1
        # "k", the digits and the code points together define the alphabet;
        # record what we actually reached in the note.
        return c, len(seen)

    c, n = wide_alphabet_case("bps7_wide", 120)
    c.note = "bits_per_symbol boundary: %d distinct key bytes -> bps 7" % n
    cases.append(c)
    c, n = wide_alphabet_case("bps8_wide", 200)
    c.note = "bits_per_symbol boundary: %d distinct key bytes -> bps 8" % n
    cases.append(c)
    c, n = wide_alphabet_case("alphabet_max", 243)
    c.note = (
        "Widest alphabet reachable from UTF-8 keys (%d bytes); the format's "
        "ceiling is 249" % n
    )
    cases.append(c)

    # -- Keys --------------------------------------------------------------
    cases.append(
        Case("unicode_keys", note="Keys")
        .uint("café", 1)
        .uint("日本語", 2)
        .uint("emoji\U0001F389", 3)
        .uint("Ωmega", 4)
        .uint("naïve", 5)
    )
    cases.append(
        Case("utf8_boundaries")
        .uint("", 1)
        .uint("߿", 2)
        .uint("ࠀ", 3)
        .uint("�", 4)
        .uint("\U00010000", 5)
        .uint("\U0010FFFF", 6)
    )
    cases.append(Case("long_key").uint("k" * 1000, 1))
    cases.append(
        Case("keys_differing_at_end")
        .uint("prefix_aaaa", 1)
        .uint("prefix_aaab", 2)
        .uint("prefix_aaac", 3)
    )

    # -- Values ------------------------------------------------------------
    cases.append(Case("all_null_values", note="Values").null("a").null("b").null("c"))
    cases.append(Case("bools").boolean("f", False).boolean("t", True))
    cases.append(
        Case("uint_boundaries",
             note="Varint group boundaries, within the range a double holds exactly")
        .uint("zero", 0)
        .uint("one", 1)
        .uint("b7", 127)
        .uint("b8", 128)
        .uint("b14", 16383)
        .uint("b15", 16384)
        .uint("b21", 2097151)
        .uint("b22", 2097152)
        .uint("u32max", 4294967295)
        .uint("u53", 9007199254740991)
    )
    cases.append(
        Case("int_boundaries")
        .sint("neg1", -1)
        .sint("neg2", -2)
        .sint("neg64", -64)
        .sint("neg65", -65)
        .sint("neg8192", -8192)
        .sint("i32min", -2147483648)
        # zigzag doubles the magnitude, so -2**52 is the largest negative a
        # double-backed implementation can still encode exactly.
        .sint("i52", -(2 ** 52))
    )
    cases.append(
        Case("int64_extremes", requires="int64",
             note="Full 64-bit range: skipped where integers live in a double")
        .sint("i64min", -(2 ** 63))
        .sint("i64min_plus1", -(2 ** 63) + 1)
        .sint("below_i53", -9007199254740993)
        .uint("u63", 2 ** 63)
        .uint("u64max", 2 ** 64 - 1)
        .uint("above_u53", 9007199254740993)
    )
    cases.append(
        Case("floats")
        .f64("pi", 3.141592653589793)
        .f64("e", 2.718281828459045)
        .f64("tiny", 5e-324)
        .f64("small", 2.2250738585072014e-308)
        .f64("neg", -0.5)
        .f64("frac", 0.1)
    )
    cases.append(
        Case(
            "float_specials",
            encode=False,
            note="Decode-only: JS cannot tell an integral double from an int",
        )
        .f64raw("inf", 0x7FF0000000000000)
        .f64raw("neg_inf", 0xFFF0000000000000)
        .f64raw("nan", 0x7FF8000000000000)
        .f64raw("neg_zero", 0x8000000000000000)
        .f64raw("zero", 0x0000000000000000)
        .f64raw("three", 0x4008000000000000)
        .f64raw("huge", 0x7FEFFFFFFFFFFFFF)
    )
    cases.append(
        Case(
            "float32",
            encode=False,
            note="Decode-only: most bindings widen float32 to double",
        )
        .f32("half", 0.5)
        .f32("quarter", -0.25)
        .f32("big", 1234.5)
    )
    cases.append(
        Case("strings")
        .text("empty", "")
        .text("ascii", "hello world")
        .text("unicode", "héllo 世界 \U0001F389")
        .text("controls", "line1\nline2\ttabbed")
        .text("long", "x" * 500)
        .text("quotes", "he said \"hi\" and 'bye'")
    )
    cases.append(
        Case("blobs")
        .blob("empty", b"")
        .blob("one", b"\x00")
        .blob("ff", b"\xff")
        .blob("all", bytes(range(256)))
        .blob("mixed", b"not really text \x00\x01\x02")
    )
    c = Case("mixed_types_many")
    c.null("a_null").boolean("b_bool", True).sint("c_int", -42).uint("d_uint", 42)
    c.f64("e_f64", 1.5).text("f_str", "str").blob("g_blob", b"\xde\xad\xbe\xef")
    cases.append(c)

    # -- Scale -------------------------------------------------------------
    c = Case("many_keys_500", note="Scale")
    for i in range(500):
        c.uint("item_%05d" % i, i)
    cases.append(c)
    c = Case("many_keys_shared_prefix")
    for i in range(200):
        c.uint("com.example.package.module.symbol_%03d" % i, i)
    cases.append(c)
    c = Case("sparse_keys")
    for i in range(0, 1000, 37):
        c.uint("s%d" % i, i)
    cases.append(c)

    return cases


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "tests", "conformance", "cases.txt")
    cases = build()

    names = [c.name for c in cases]
    assert len(names) == len(set(names)), "duplicate case name"

    body = "\n\n".join(c.render() for c in cases)
    header = (
        "# Triepack cross-language conformance corpus.\n"
        "#\n"
        "# GENERATED by tools/gen_conformance_cases.py -- do not edit by hand.\n"
        "# The format is documented in tests/conformance/README.md.\n"
        "#\n"
        "# %d cases\n" % len(cases)
    )
    with open(out, "w") as f:
        f.write(header + "\n" + body + "\n")
    print(
        "wrote %s (%d cases, %d keys)"
        % (out, len(cases), sum(len(c.keys) for c in cases))
    )


if __name__ == "__main__":
    main()
