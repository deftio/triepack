// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

// Cross-language conformance suite.
//
// Reads tests/conformance/cases.txt -- the corpus every implementation shares
// -- and checks this binding against the C-generated fixtures: decoding a
// fixture must produce the corpus values, and encoding the corpus values must
// produce the fixture byte for byte.
//
// See tests/conformance/README.md.

package triepack

import (
	"bytes"
	"encoding/hex"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

const conformanceDir = "../../tests/conformance"

// -- Corpus parsing --------------------------------------------------------

// tokenBytes decodes a corpus token ("~" for empty, %XX escapes) into bytes.
func tokenBytes(t *testing.T, tok string) []byte {
	t.Helper()
	if tok == "~" {
		return []byte{}
	}
	out := make([]byte, 0, len(tok))
	for i := 0; i < len(tok); {
		if tok[i] == '%' {
			v, err := strconv.ParseUint(tok[i+1:i+3], 16, 8)
			if err != nil {
				t.Fatalf("bad escape in %q: %v", tok, err)
			}
			out = append(out, byte(v))
			i += 3
		} else {
			out = append(out, tok[i])
			i++
		}
	}
	return out
}

type confEntry struct {
	key string
	typ string
	arg string
}

type confCase struct {
	name           string
	encodeExpected bool
	entries        []confEntry
}

func (c *confCase) fixturePath() string {
	return filepath.Join(conformanceDir, "fixtures", c.name+".trp")
}

func (c *confCase) fixture(t *testing.T) []byte {
	t.Helper()
	buf, err := os.ReadFile(c.fixturePath())
	if err != nil {
		t.Fatalf("case %s: %v", c.name, err)
	}
	return buf
}

// value builds the Go value a corpus entry describes.
func (e confEntry) value(t *testing.T) interface{} {
	t.Helper()
	switch e.typ {
	case "null":
		return nil
	case "bool":
		return e.arg == "1"
	case "int":
		v, err := strconv.ParseInt(e.arg, 10, 64)
		if err != nil {
			t.Fatalf("bad int %q: %v", e.arg, err)
		}
		return v
	case "uint":
		v, err := strconv.ParseUint(e.arg, 10, 64)
		if err != nil {
			t.Fatalf("bad uint %q: %v", e.arg, err)
		}
		return v
	case "f64":
		bits, err := strconv.ParseUint(e.arg, 16, 64)
		if err != nil {
			t.Fatalf("bad f64 %q: %v", e.arg, err)
		}
		return math.Float64frombits(bits)
	case "f32":
		bits, err := strconv.ParseUint(e.arg, 16, 32)
		if err != nil {
			t.Fatalf("bad f32 %q: %v", e.arg, err)
		}
		return float64(math.Float32frombits(uint32(bits)))
	case "str":
		return string(tokenBytes(t, e.arg))
	case "blob":
		if e.arg == "~" {
			return []byte{}
		}
		raw, err := hex.DecodeString(e.arg)
		if err != nil {
			t.Fatalf("bad blob %q: %v", e.arg, err)
		}
		return raw
	}
	t.Fatalf("unknown corpus type %q", e.typ)
	return nil
}

func (c *confCase) data(t *testing.T) map[string]interface{} {
	t.Helper()
	m := make(map[string]interface{}, len(c.entries))
	for _, e := range c.entries {
		m[e.key] = e.value(t)
	}
	return m
}

func parseConformanceCases(t *testing.T) []*confCase {
	t.Helper()
	raw, err := os.ReadFile(filepath.Join(conformanceDir, "cases.txt"))
	if err != nil {
		t.Fatalf("cannot read corpus: %v", err)
	}
	var cases []*confCase
	var cur *confCase
	for _, line := range strings.Split(string(raw), "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.Split(line, " ")
		switch parts[0] {
		case "case":
			cur = &confCase{name: parts[1], encodeExpected: true}
			for _, p := range parts[2:] {
				if p == "encode=no" {
					cur.encodeExpected = false
				}
			}
			cases = append(cases, cur)
		case "key":
			e := confEntry{key: string(tokenBytes(t, parts[1])), typ: parts[2]}
			if len(parts) > 3 {
				e.arg = parts[3]
			}
			cur.entries = append(cur.entries, e)
		default:
			t.Fatalf("unknown corpus directive %q", parts[0])
		}
	}
	return cases
}

// valuesMatch compares a decoded value with the expected one. Doubles compare
// by bit pattern so -0.0 is distinguished, with all NaNs treated as equal.
func valuesMatch(got, want interface{}) bool {
	if want == nil {
		return got == nil
	}
	switch w := want.(type) {
	case bool:
		g, ok := got.(bool)
		return ok && g == w
	case int64:
		g, ok := got.(int64)
		return ok && g == w
	case uint64:
		g, ok := got.(uint64)
		return ok && g == w
	case float64:
		g, ok := got.(float64)
		if !ok {
			return false
		}
		if math.IsNaN(w) && math.IsNaN(g) {
			return true
		}
		return math.Float64bits(g) == math.Float64bits(w)
	case string:
		g, ok := got.(string)
		return ok && g == w
	case []byte:
		g, ok := got.([]byte)
		return ok && bytes.Equal(g, w)
	}
	return false
}

// -- The suite -------------------------------------------------------------

func TestConformanceCorpusPresent(t *testing.T) {
	cases := parseConformanceCases(t)
	if len(cases) == 0 {
		t.Fatal("corpus is empty")
	}
	for _, c := range cases {
		if _, err := os.Stat(c.fixturePath()); err != nil {
			t.Errorf("case %s: missing fixture: %v", c.name, err)
		}
	}
}

func TestConformanceDecode(t *testing.T) {
	for _, c := range parseConformanceCases(t) {
		c := c
		t.Run(c.name, func(t *testing.T) {
			result, err := Decode(c.fixture(t))
			if err != nil {
				t.Fatalf("decode failed: %v", err)
			}
			want := c.data(t)
			if len(result) != len(want) {
				t.Fatalf("got %d keys, want %d", len(result), len(want))
			}
			for k, v := range want {
				got, ok := result[k]
				if !ok {
					t.Fatalf("missing key %q", k)
				}
				if !valuesMatch(got, v) {
					t.Fatalf("key %q: got %#v, want %#v", k, got, v)
				}
			}
		})
	}
}

func TestConformanceEncode(t *testing.T) {
	for _, c := range parseConformanceCases(t) {
		c := c
		if !c.encodeExpected {
			// Cases marked encode=no describe values this binding cannot
			// reproduce exactly (float32, or doubles some languages read
			// back as integers).
			continue
		}
		t.Run(c.name, func(t *testing.T) {
			got, err := Encode(c.data(t))
			if err != nil {
				t.Fatalf("encode failed: %v", err)
			}
			if want := c.fixture(t); !bytes.Equal(got, want) {
				t.Fatalf("encoded %d bytes, C reference is %d; first difference at %d",
					len(got), len(want), firstDiff(got, want))
			}
		})
	}
}

func firstDiff(a, b []byte) int {
	n := len(a)
	if len(b) < n {
		n = len(b)
	}
	for i := 0; i < n; i++ {
		if a[i] != b[i] {
			return i
		}
	}
	return n
}

// -- Malformed inputs ------------------------------------------------------

// Each file is a valid fixture with one field damaged. Damage inside the data
// is re-sealed with a correct CRC, so the reader has to catch it rather than
// being handed a checksum failure.
func TestConformanceRejectsMalformed(t *testing.T) {
	dir := filepath.Join(conformanceDir, "malformed")
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatalf("cannot read malformed corpus: %v", err)
	}
	seen := 0
	for _, e := range entries {
		if !strings.HasSuffix(e.Name(), ".trp") {
			continue
		}
		seen++
		name := e.Name()
		t.Run(name, func(t *testing.T) {
			buf, err := os.ReadFile(filepath.Join(dir, name))
			if err != nil {
				t.Fatalf("%v", err)
			}
			if result, err := Decode(buf); err == nil {
				t.Fatalf("accepted malformed input, decoded %d keys", len(result))
			}
		})
	}
	if seen == 0 {
		t.Fatal("malformed corpus is empty")
	}
}
