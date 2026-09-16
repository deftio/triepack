// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

// Package triepack provides a native Go implementation of the Triepack .trp binary format.
package triepack

import "fmt"

// Encode encodes a map of string keys into the .trp binary format.
func Encode(data map[string]interface{}) ([]byte, error) {
	return encodeData(data)
}

// Decode decodes a .trp binary buffer into a map of string keys.
func Decode(buffer []byte) (map[string]interface{}, error) {
	return decodeData(buffer)
}

// Version is kept in step with triepack-version.txt by
// scripts/sync_version.sh.
const Version = "1.2.0"

// Version of the on-disk .trp format this implementation writes. Distinct
// from the library version: it changes only when the bytes change.
const (
	FormatVersionMajor = 1
	FormatVersionMinor = 0
)

// VersionInfo describes a triepack build. Every implementation reports the
// same fields, so a polyglot system can ask each one what it is.
type VersionInfo struct {
	Name               string `json:"name"`
	Implementation     string `json:"implementation"`
	Version            string `json:"version"`
	VersionMajor       int    `json:"version_major"`
	VersionMinor       int    `json:"version_minor"`
	VersionPatch       int    `json:"version_patch"`
	FormatVersionMajor int    `json:"format_version_major"`
	FormatVersionMinor int    `json:"format_version_minor"`
	MaxAlphabetSize    int    `json:"max_alphabet_size"`
}

// VersionMetadata returns metadata about this build.
func VersionMetadata() VersionInfo {
	var major, minor, patch int
	fmt.Sscanf(Version, "%d.%d.%d", &major, &minor, &patch)
	return VersionInfo{
		Name:               "triepack",
		Implementation:     "go",
		Version:            Version,
		VersionMajor:       major,
		VersionMinor:       minor,
		VersionPatch:       patch,
		FormatVersionMajor: FormatVersionMajor,
		FormatVersionMinor: FormatVersionMinor,
		MaxAlphabetSize:    MaxAlphabetSize,
	}
}
