// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

// Package triepack provides a native Go implementation of the Triepack .trp binary format.
package triepack

import "errors"

// ErrNotImplemented is returned by stub functions that are not yet implemented.
var ErrNotImplemented = errors.New("not implemented")

// Encode encodes a map of string keys into the .trp binary format.
func Encode(data map[string]interface{}) ([]byte, error) {
	return nil, ErrNotImplemented
}

// Decode decodes a .trp binary buffer into a map of string keys.
func Decode(buffer []byte) (map[string]interface{}, error) {
	return nil, ErrNotImplemented
}
