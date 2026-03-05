// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"bytes"
	"fmt"
	"math"
	"testing"
)

func TestEmptyObject(t *testing.T) {
	buf, err := Encode(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	if len(buf) < 36 {
		t.Fatalf("expected at least 36 bytes (32 header + 4 CRC), got %d", len(buf))
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if len(result) != 0 {
		t.Fatalf("expected empty map, got %d entries", len(result))
	}
}

func TestSingleKeyNull(t *testing.T) {
	data := map[string]interface{}{"hello": nil}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
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

func TestSingleKeyInteger(t *testing.T) {
	data := map[string]interface{}{"key": 42}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "key", 42)
}

func TestSingleKeyNegative(t *testing.T) {
	data := map[string]interface{}{"neg": -100}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "neg", -100)
}

func TestSingleKeyBoolTrue(t *testing.T) {
	data := map[string]interface{}{"flag": true}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	v, ok := result["flag"]
	if !ok {
		t.Fatal("missing key 'flag'")
	}
	if v != true {
		t.Fatalf("expected true, got %v", v)
	}
}

func TestSingleKeyBoolFalse(t *testing.T) {
	data := map[string]interface{}{"flag": false}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	v, ok := result["flag"]
	if !ok {
		t.Fatal("missing key 'flag'")
	}
	if v != false {
		t.Fatalf("expected false, got %v", v)
	}
}

func TestSingleKeyFloat64(t *testing.T) {
	data := map[string]interface{}{"pi": 3.14159}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	v, ok := result["pi"]
	if !ok {
		t.Fatal("missing key 'pi'")
	}
	f, ok := v.(float64)
	if !ok {
		t.Fatalf("expected float64, got %T", v)
	}
	if math.Abs(f-3.14159) > 1e-10 {
		t.Fatalf("expected ~3.14159, got %v", f)
	}
}

func TestSingleKeyString(t *testing.T) {
	data := map[string]interface{}{"greeting": "hello world"}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	v, ok := result["greeting"]
	if !ok {
		t.Fatal("missing key 'greeting'")
	}
	s, ok := v.(string)
	if !ok {
		t.Fatalf("expected string, got %T", v)
	}
	if s != "hello world" {
		t.Fatalf("expected 'hello world', got '%s'", s)
	}
}

func TestSingleKeyBlob(t *testing.T) {
	blob := []byte{0xDE, 0xAD, 0xBE, 0xEF}
	data := map[string]interface{}{"binary": blob}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	v, ok := result["binary"]
	if !ok {
		t.Fatal("missing key 'binary'")
	}
	b, ok := v.([]byte)
	if !ok {
		t.Fatalf("expected []byte, got %T", v)
	}
	if !bytes.Equal(b, blob) {
		t.Fatalf("expected %v, got %v", blob, b)
	}
}

func TestMultipleKeysMixed(t *testing.T) {
	data := map[string]interface{}{
		"bool": true,
		"f64":  3.14159,
		"int":  -100,
		"str":  "hello",
		"uint": 200,
	}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
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

func TestSharedPrefix(t *testing.T) {
	data := map[string]interface{}{"abc": 10, "abd": 20, "xyz": 30}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "abc", 10)
	assertIntValue(t, result, "abd", 20)
	assertIntValue(t, result, "xyz", 30)
}

func TestKeysOnlyNull(t *testing.T) {
	data := map[string]interface{}{"apple": nil, "banana": nil, "cherry": nil}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
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

func TestLargeDictionary(t *testing.T) {
	data := make(map[string]interface{})
	for i := 0; i < 100; i++ {
		data[fmt.Sprintf("key_%04d", i)] = i
	}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
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

func TestMagicBytes(t *testing.T) {
	buf, err := Encode(map[string]interface{}{"test": 1})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	if buf[0] != 0x54 || buf[1] != 0x52 || buf[2] != 0x50 || buf[3] != 0x00 {
		t.Fatalf("expected magic TRP\\0, got %x %x %x %x", buf[0], buf[1], buf[2], buf[3])
	}
}

func TestVersionHeader(t *testing.T) {
	buf, err := Encode(map[string]interface{}{"test": 1})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	if buf[4] != 1 {
		t.Fatalf("expected version major=1, got %d", buf[4])
	}
	if buf[5] != 0 {
		t.Fatalf("expected version minor=0, got %d", buf[5])
	}
}

func TestCRCCorruption(t *testing.T) {
	buf, err := Encode(map[string]interface{}{"test": 1})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	corrupted := make([]byte, len(buf))
	copy(corrupted, buf)
	corrupted[10] ^= 0x01
	_, err = Decode(corrupted)
	if err == nil {
		t.Fatal("expected error from CRC corruption")
	}
}

func TestInvalidMagic(t *testing.T) {
	buf := make([]byte, 40)
	buf[0] = 0xFF
	_, err := Decode(buf)
	if err == nil {
		t.Fatal("expected error from invalid magic")
	}
}

func TestDecodeRejectsShort(t *testing.T) {
	_, err := Decode(make([]byte, 10))
	if err == nil {
		t.Fatal("expected error from short buffer")
	}
}

func TestUTF8Keys(t *testing.T) {
	data := map[string]interface{}{
		"caf\u00e9":  1,
		"na\u00efve": 2,
		"\u00fcber":  3,
	}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "caf\u00e9", 1)
	assertIntValue(t, result, "na\u00efve", 2)
	assertIntValue(t, result, "\u00fcber", 3)
}

func TestSingleCharKeys(t *testing.T) {
	data := map[string]interface{}{"a": 1, "b": 2, "c": 3, "d": 4, "e": 5}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	for k, expected := range data {
		assertIntValue(t, result, k, expected.(int))
	}
}

func TestEmptyStringKey(t *testing.T) {
	data := map[string]interface{}{"": 99}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "", 99)
}

func TestLongSharedPrefix(t *testing.T) {
	data := map[string]interface{}{"aaaaaa1": 1, "aaaaaa2": 2, "aaaaaa3": 3}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "aaaaaa1", 1)
	assertIntValue(t, result, "aaaaaa2", 2)
	assertIntValue(t, result, "aaaaaa3", 3)
}

func TestPrefixKeyKeysOnly(t *testing.T) {
	data := map[string]interface{}{"a": nil, "ab": nil, "ac": nil}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	for _, key := range []string{"a", "ab", "ac"} {
		if _, ok := result[key]; !ok {
			t.Fatalf("missing key '%s'", key)
		}
		if result[key] != nil {
			t.Fatalf("expected nil for '%s', got %v", key, result[key])
		}
	}
}

func TestPrefixKeyWithValues(t *testing.T) {
	data := map[string]interface{}{"a": 1, "ab": 2, "ac": 3}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "a", 1)
	assertIntValue(t, result, "ab", 2)
	assertIntValue(t, result, "ac", 3)
}

func TestTerminalWithChildrenNonlast(t *testing.T) {
	data := map[string]interface{}{"a": 1, "ab": 2, "b": 3}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "a", 1)
	assertIntValue(t, result, "ab", 2)
	assertIntValue(t, result, "b", 3)
}

func TestTerminalWithChildrenNullValue(t *testing.T) {
	data := map[string]interface{}{"a": nil, "ab": 2, "b": 3}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if result["a"] != nil {
		t.Fatalf("expected nil for 'a', got %v", result["a"])
	}
	assertIntValue(t, result, "ab", 2)
	assertIntValue(t, result, "b", 3)
}

func TestNullValueInMixedDict(t *testing.T) {
	data := map[string]interface{}{"alpha": nil, "beta": 42}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if result["alpha"] != nil {
		t.Fatalf("expected nil for 'alpha', got %v", result["alpha"])
	}
	assertIntValue(t, result, "beta", 42)
}

func TestSubtreeMultiEntryGroups(t *testing.T) {
	data := map[string]interface{}{"a": 1, "aba": 2, "abb": 3, "ac": 4, "b": 5}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "a", 1)
	assertIntValue(t, result, "aba", 2)
	assertIntValue(t, result, "abb", 3)
	assertIntValue(t, result, "ac", 4)
	assertIntValue(t, result, "b", 5)
}

func TestNonTerminalMultiEntryChildren(t *testing.T) {
	data := map[string]interface{}{"aba": 1, "abb": 2, "ac": 3, "b": 4}
	buf, err := Encode(data)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	result, err := Decode(buf)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	assertIntValue(t, result, "aba", 1)
	assertIntValue(t, result, "abb", 2)
	assertIntValue(t, result, "ac", 3)
	assertIntValue(t, result, "b", 4)
}

// assertIntValue checks that a map key has an integer value.
func assertIntValue(t *testing.T, result map[string]interface{}, key string, expected int) {
	t.Helper()
	v, ok := result[key]
	if !ok {
		t.Fatalf("missing key '%s'", key)
	}
	switch iv := v.(type) {
	case int:
		if iv != expected {
			t.Fatalf("key '%s': expected %d, got %d", key, expected, iv)
		}
	case int64:
		if int(iv) != expected {
			t.Fatalf("key '%s': expected %d, got %d", key, expected, iv)
		}
	case uint64:
		if int(iv) != expected {
			t.Fatalf("key '%s': expected %d, got %d", key, expected, iv)
		}
	default:
		t.Fatalf("key '%s': expected int, got %T (%v)", key, v, v)
	}
}
