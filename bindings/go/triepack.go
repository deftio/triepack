// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

// Package triepack provides a native Go implementation of the Triepack .trp binary format.
package triepack

// Encode encodes a map of string keys into the .trp binary format.
func Encode(data map[string]interface{}) ([]byte, error) {
	return encodeData(data)
}

// Decode decodes a .trp binary buffer into a map of string keys.
func Decode(buffer []byte) (map[string]interface{}, error) {
	return decodeData(buffer)
}
