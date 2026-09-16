/*
 * ConformanceTest.java
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

package com.deftio.triepack;

import static org.junit.jupiter.api.Assertions.*;

import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;

class ConformanceTest {

    // ── Corpus location ───────────────────────────────────────────────

    private static Path conformanceDir() {
        // Tests run from bindings/java; walk up to the repository root.
        Path here = Paths.get("").toAbsolutePath();
        for (Path p = here; p != null; p = p.getParent()) {
            Path candidate = p.resolve("tests").resolve("conformance");
            if (Files.isDirectory(candidate)) {
                return candidate;
            }
        }
        throw new IllegalStateException("cannot locate tests/conformance from " + here);
    }

    private static Path fixturePath(String name) {
        return conformanceDir().resolve("fixtures").resolve(name + ".trp");
    }

    // ── Corpus parsing ────────────────────────────────────────────────

    /** Decode a corpus token ("~" for empty, %XX escapes) into bytes. */
    private static byte[] tokenBytes(String tok) {
        if (tok.equals("~")) {
            return new byte[0];
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (int i = 0; i < tok.length(); ) {
            if (tok.charAt(i) == '%') {
                out.write(Integer.parseInt(tok.substring(i + 1, i + 3), 16));
                i += 3;
            } else {
                out.write(tok.charAt(i));
                i += 1;
            }
        }
        return out.toByteArray();
    }

    private static String tokenString(String tok) {
        return new String(tokenBytes(tok), StandardCharsets.UTF_8);
    }

    private static byte[] hexBytes(String tok) {
        if (tok.equals("~")) {
            return new byte[0];
        }
        byte[] out = new byte[tok.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(tok.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }

    static final class Entry {
        final String key;
        final String type;
        final String arg;

        Entry(String key, String type, String arg) {
            this.key = key;
            this.type = type;
            this.arg = arg;
        }
    }

    static final class Case {
        final String name;
        final boolean encodeExpected;
        final List<Entry> entries = new ArrayList<>();

        Case(String name, boolean encodeExpected) {
            this.name = name;
            this.encodeExpected = encodeExpected;
        }

        Map<String, TpValue> data() {
            Map<String, TpValue> m = new LinkedHashMap<>();
            for (Entry e : entries) {
                m.put(e.key, corpusValue(e.type, e.arg));
            }
            return m;
        }

        byte[] fixture() throws IOException {
            return Files.readAllBytes(fixturePath(name));
        }

        @Override
        public String toString() {
            return name;
        }
    }

    /** Build the Java value a corpus entry describes. */
    private static TpValue corpusValue(String type, String arg) {
        switch (type) {
            case "null": return TpValue.ofNull();
            case "bool": return TpValue.ofBool("1".equals(arg));
            case "int": return TpValue.ofInt(Long.parseLong(arg));
            // uint64 values above Long.MAX_VALUE arrive as the matching
            // negative bit pattern, which is how TpValue carries them.
            case "uint": return TpValue.ofUInt(Long.parseUnsignedLong(arg));
            case "f64": return TpValue.ofFloat64(
                Double.longBitsToDouble(Long.parseUnsignedLong(arg, 16)));
            case "f32": return TpValue.ofFloat64(
                Float.intBitsToFloat((int) Long.parseUnsignedLong(arg, 16)));
            case "str": return TpValue.ofString(tokenString(arg));
            case "blob": return TpValue.ofBlob(hexBytes(arg));
            default: throw new IllegalArgumentException("unknown corpus type: " + type);
        }
    }

    static List<Case> parseCases() throws IOException {
        List<Case> cases = new ArrayList<>();
        Case cur = null;
        for (String raw : Files.readAllLines(conformanceDir().resolve("cases.txt"),
                                             StandardCharsets.UTF_8)) {
            String line = raw.trim();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] parts = line.split(" ");
            if (parts[0].equals("case")) {
                boolean encodeExpected = !Arrays.asList(parts).contains("encode=no");
                cur = new Case(parts[1], encodeExpected);
                cases.add(cur);
            } else if (parts[0].equals("key")) {
                cur.entries.add(new Entry(tokenString(parts[1]), parts[2],
                                          parts.length > 3 ? parts[3] : null));
            } else {
                throw new IllegalArgumentException("unknown corpus directive: " + parts[0]);
            }
        }
        return cases;
    }

    static Stream<Case> cases() throws IOException {
        return parseCases().stream();
    }

    static Stream<Case> encodableCases() throws IOException {
        // Cases marked encode=no describe values this binding cannot reproduce
        // exactly (float32, or doubles some languages read back as integers).
        return parseCases().stream().filter(c -> c.encodeExpected);
    }

    /**
     * Doubles compare by bit pattern so -0.0 is distinguished, with all NaNs
     * treated as equal.
     */
    private static boolean valuesMatch(TpValue got, TpValue want) {
        if (got == null) {
            return false;
        }
        if (want.getType() == TpValue.Type.FLOAT64 && got.getType() == TpValue.Type.FLOAT64) {
            double g = got.float64Value();
            double w = want.float64Value();
            if (Double.isNaN(g) && Double.isNaN(w)) {
                return true;
            }
            return Double.doubleToRawLongBits(g) == Double.doubleToRawLongBits(w);
        }
        return got.equals(want);
    }

    // ── The suite ─────────────────────────────────────────────────────

    @Test
    void testCorpusAndFixturesPresent() throws IOException {
        List<Case> cases = parseCases();
        assertFalse(cases.isEmpty(), "corpus is empty");
        for (Case c : cases) {
            assertTrue(Files.exists(fixturePath(c.name)), "case " + c.name + ": missing fixture");
        }
    }

    @ParameterizedTest(name = "{0}")
    @MethodSource("cases")
    void testDecodesCFixture(Case c) throws IOException {
        Map<String, TpValue> result = TriePack.decode(c.fixture());
        Map<String, TpValue> want = c.data();
        assertEquals(want.size(), result.size(), "case " + c.name + ": key count");
        for (Map.Entry<String, TpValue> e : want.entrySet()) {
            assertTrue(valuesMatch(result.get(e.getKey()), e.getValue()),
                       "case " + c.name + ": key " + e.getKey()
                       + ": got " + result.get(e.getKey()) + ", want " + e.getValue());
        }
    }

    @ParameterizedTest(name = "{0}")
    @MethodSource("encodableCases")
    void testEncodesLikeCReference(Case c) throws IOException {
        assertArrayEquals(c.fixture(), TriePack.encode(c.data()), "case " + c.name);
    }

    // ── Malformed inputs ──────────────────────────────────────────────

    static Stream<String> malformedFixtures() throws IOException {
        try (Stream<Path> files = Files.list(conformanceDir().resolve("malformed"))) {
            return files.map(p -> p.getFileName().toString())
                        .filter(n -> n.endsWith(".trp"))
                        .sorted()
                        .collect(java.util.stream.Collectors.toList())
                        .stream();
        }
    }

    @Test
    void testMalformedCorpusPresent() throws IOException {
        assertTrue(malformedFixtures().findAny().isPresent(), "malformed corpus is empty");
    }

    /**
     * Each file is a valid fixture with one field damaged. Damage inside the
     * data is re-sealed with a correct CRC, so the reader has to catch it
     * rather than being handed a checksum failure.
     */
    @ParameterizedTest(name = "{0}")
    @MethodSource("malformedFixtures")
    void testMalformedInputRejected(String name) throws IOException {
        byte[] buf = Files.readAllBytes(conformanceDir().resolve("malformed").resolve(name));
        assertThrows(RuntimeException.class, () -> TriePack.decode(buf), name);
    }
}
