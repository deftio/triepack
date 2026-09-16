// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"strings"
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

// Regression: issue #1 -- walk must stop at the end of the trie.
//
// The walker used to guess whether a BRANCH followed a terminal by peeking at
// the next bits. Past the last terminal those bits are the byte padding and
// the CRC, which for some key sets look exactly like a BRANCH code, sending
// the walker off the end of the buffer. The trie end comes from the header,
// so the walker must stop there.
//
// Each of these key sets produced a trailing BRANCH-looking code before the
// fix.
var trailingBranchKeySets = [][]string{
	{"eddb", "h"},
	{"aad", "ebg", "ec", "ehhbf", "h", "hebad"},
	{"aghed", "bae", "bfffa", "cad", "d", "ebbhb", "fa", "feb"},
	{"a", "aad", "bg", "bgc", "egba", "gahad", "ghehg", "hbdab", "hg"},
}

func TestWalkStopsAtTrieEndKeysOnly(t *testing.T) {
	for _, keys := range trailingBranchKeySets {
		data := make(map[string]interface{})
		for _, k := range keys {
			data[k] = nil
		}
		buf, err := Encode(data)
		if err != nil {
			t.Fatalf("Encode(%v) failed: %v", keys, err)
		}
		result, err := Decode(buf)
		if err != nil {
			t.Fatalf("Decode(%v) failed: %v", keys, err)
		}
		if len(result) != len(keys) {
			t.Fatalf("key set %v: got %d keys, want %d", keys, len(result), len(keys))
		}
		for _, k := range keys {
			if v, ok := result[k]; !ok || v != nil {
				t.Fatalf("key set %v: key %q = %v (present: %v), want nil", keys, k, v, ok)
			}
		}
	}
}

func TestWalkStopsAtTrieEndWithValues(t *testing.T) {
	for _, keys := range trailingBranchKeySets {
		data := make(map[string]interface{})
		for i, k := range keys {
			data[k] = uint64(i)
		}
		buf, err := Encode(data)
		if err != nil {
			t.Fatalf("Encode(%v) failed: %v", keys, err)
		}
		result, err := Decode(buf)
		if err != nil {
			t.Fatalf("Decode(%v) failed: %v", keys, err)
		}
		if len(result) != len(keys) {
			t.Fatalf("key set %v: got %d keys, want %d", keys, len(result), len(keys))
		}
		for i, k := range keys {
			assertIntValue(t, result, k, i)
		}
	}
}

func TestHeaderDeclaresExactlyTheBitsWritten(t *testing.T) {
	buf, err := Encode(map[string]interface{}{"eddb": nil, "h": nil})
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	u32 := func(off int) int {
		return int(buf[off])<<24 | int(buf[off+1])<<16 | int(buf[off+2])<<8 | int(buf[off+3])
	}
	valueStoreOffset := u32(16)
	totalDataBits := u32(24)
	// No values, so the trie is the whole data section, and the data section
	// plus its byte padding and the 4-byte CRC is the buffer.
	if valueStoreOffset != totalDataBits {
		t.Fatalf("value_store_offset %d != total_data_bits %d", valueStoreOffset, totalDataBits)
	}
	if want := 32 + (totalDataBits+7)/8 + 4; len(buf) != want {
		t.Fatalf("buffer is %d bytes, want %d", len(buf), want)
	}
}

// Go strings can hold arbitrary bytes, so unlike the UTF-8-only bindings this
// one can reach the format's alphabet ceiling. Going over it used to produce a
// buffer with a valid CRC that decoded to a single key.
func TestAlphabetLimit(t *testing.T) {
	build := func(n int) (map[string]interface{}, []byte, error) {
		data := make(map[string]interface{}, n)
		for i := 0; i < n; i++ {
			data[string([]byte{byte(i), 'X'})] = nil
		}
		buf, err := Encode(data)
		return data, buf, err
	}

	t.Run("at the limit", func(t *testing.T) {
		data, buf, err := build(MaxAlphabetSize)
		if err != nil {
			t.Fatalf("encode failed at the limit: %v", err)
		}
		result, err := Decode(buf)
		if err != nil {
			t.Fatalf("decode failed: %v", err)
		}
		if len(result) != len(data) {
			t.Fatalf("got %d keys, want %d", len(result), len(data))
		}
	})

	for _, n := range []int{MaxAlphabetSize + 1, 252, 256} {
		t.Run(fmt.Sprintf("over the limit/%d", n), func(t *testing.T) {
			_, _, err := build(n)
			if err == nil {
				t.Fatal("encoded an alphabet the format cannot address")
			}
			if !strings.Contains(err.Error(), "too many distinct byte values") {
				t.Fatalf("unexpected error: %v", err)
			}
		})
	}
}

// The version a build reports has to equal triepack-version.txt, the single
// source of truth. Reading the file rather than a copy of the string is the
// point: a stale constant fails.
func declaredVersion(t *testing.T) string {
	t.Helper()
	raw, err := os.ReadFile("../../triepack-version.txt")
	if err != nil {
		t.Fatalf("cannot read triepack-version.txt: %v", err)
	}
	return strings.TrimSpace(string(raw))
}

func TestVersionMatchesSourceOfTruth(t *testing.T) {
	want := declaredVersion(t)
	if Version != want {
		t.Fatalf("Version = %q, triepack-version.txt = %q", Version, want)
	}
	if got := VersionMetadata().Version; got != want {
		t.Fatalf("VersionMetadata().Version = %q, want %q", got, want)
	}
}

func TestVersionMetadataShape(t *testing.T) {
	want := declaredVersion(t)
	var major, minor, patch int
	fmt.Sscanf(want, "%d.%d.%d", &major, &minor, &patch)

	got := VersionMetadata()
	expected := VersionInfo{
		Name:               "triepack",
		Implementation:     "go",
		Version:            want,
		VersionMajor:       major,
		VersionMinor:       minor,
		VersionPatch:       patch,
		FormatVersionMajor: 1,
		FormatVersionMinor: 0,
		MaxAlphabetSize:    249,
	}
	if got != expected {
		t.Fatalf("VersionMetadata() = %+v, want %+v", got, expected)
	}
}
