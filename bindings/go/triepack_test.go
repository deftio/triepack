// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"testing"
)

func TestEncodeReturnsError(t *testing.T) {
	data := map[string]interface{}{"hello": "world"}
	_, err := Encode(data)
	if err == nil {
		t.Fatal("expected error from Encode, got nil")
	}
	if err != ErrNotImplemented {
		t.Fatalf("expected ErrNotImplemented, got %v", err)
	}
}
