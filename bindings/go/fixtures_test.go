// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

func fixtureDir() string {
	_, filename, _, _ := runtime.Caller(0)
	return filepath.Join(filepath.Dir(filename), "..", "..", "tests", "fixtures")
}

func readFixture(t *testing.T, name string) []byte {
	t.Helper()
	path := filepath.Join(fixtureDir(), name)
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("failed to read fixture %s: %v", name, err)
	}
	return data
}

// ── Decode C-generated fixtures ─────────────────────────────────────

func TestDecodeFixtureEmpty(t *testing.T) {
	buf := readFixture(t, "empty.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if len(result) != 0 {
		t.Fatalf("expected empty map, got %d entries", len(result))
	}
}

func TestDecodeFixtureSingleNull(t *testing.T) {
	buf := readFixture(t, "single_null.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if len(result) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(result))
	}
	v, ok := result["hello"]
	if !ok {
		t.Fatal("missing key 'hello'")
	}
	if v != nil {
		t.Fatalf("expected nil, got %v", v)
	}
}

func TestDecodeFixtureSingleInt(t *testing.T) {
	buf := readFixture(t, "single_int.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "key", 42)
}

func TestDecodeFixtureMultiMixed(t *testing.T) {
	buf := readFixture(t, "multi_mixed.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if result["bool"] != true {
		t.Fatalf("expected bool=true, got %v", result["bool"])
	}
	f, ok := result["f64"].(float64)
	if !ok || math.Abs(f-3.14159) > 1e-10 {
		t.Fatalf("expected f64~3.14159, got %v", result["f64"])
	}
	assertIntValue(t, result, "int", -100)
	if result["str"] != "hello" {
		t.Fatalf("expected str='hello', got %v", result["str"])
	}
	assertIntValue(t, result, "uint", 200)
}

func TestDecodeFixtureSharedPrefix(t *testing.T) {
	buf := readFixture(t, "shared_prefix.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "abc", 10)
	assertIntValue(t, result, "abd", 20)
	assertIntValue(t, result, "xyz", 30)
}

func TestDecodeFixtureLarge(t *testing.T) {
	buf := readFixture(t, "large.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if len(result) != 100 {
		t.Fatalf("expected 100 entries, got %d", len(result))
	}
	for i := 0; i < 100; i++ {
		key := fmt.Sprintf("key_%04d", i)
		assertIntValue(t, result, key, i)
	}
}

func TestDecodeFixtureKeysOnly(t *testing.T) {
	buf := readFixture(t, "keys_only.trp")
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	for _, key := range []string{"apple", "banana", "cherry"} {
		v, ok := result[key]
		if !ok {
			t.Fatalf("missing key '%s'", key)
		}
		if v != nil {
			t.Fatalf("expected nil for '%s', got %v", key, v)
		}
	}
}

// ── Byte-for-byte binary reproducibility ────────────────────────────

func TestEncodeEmptyMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "empty.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeSingleNullMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{"hello": nil})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "single_null.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeSingleIntMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{"key": 42})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "single_int.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeMultiMixedMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{
		"bool": true,
		"f64":  3.14159,
		"int":  -100,
		"str":  "hello",
		"uint": 200,
	})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "multi_mixed.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeSharedPrefixMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{"abc": 10, "abd": 20, "xyz": 30})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "shared_prefix.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeLargeMatchesFixture(t *testing.T) {
	data := make(map[string]interface{})
	for i := 0; i < 100; i++ {
		data[fmt.Sprintf("key_%04d", i)] = i
	}
	goBuf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "large.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}

func TestEncodeKeysOnlyMatchesFixture(t *testing.T) {
	goBuf, err := Encode(map[string]interface{}{"apple": nil, "banana": nil, "cherry": nil})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	cBuf := readFixture(t, "keys_only.trp")
	if !bytes.Equal(goBuf, cBuf) {
		t.Fatalf("Go output (%d bytes) != C fixture (%d bytes)", len(goBuf), len(cBuf))
	}
}
