// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"fmt"
	"sort"
)

// decodeData decodes a .trp binary buffer into a map.
func decodeData(buffer []byte) (map[string]interface{}, error) {
	if len(buffer) < tpHeaderSize+4 {
		return nil, fmt.Errorf("data too short for .trp format")
	}

	buf := buffer

	// Validate magic
	for i := 0; i < 4; i++ {
		if buf[i] != tpMagic[i] {
			return nil, fmt.Errorf("invalid magic bytes - not a .trp file")
		}
	}

	// Version
	versionMajor := buf[4]
	if versionMajor != 1 {
		return nil, fmt.Errorf("unsupported format version: %d", versionMajor)
	}

	// Flags
	flags := uint16(buf[6])<<8 | uint16(buf[7])
	hasValues := (flags & tpFlagHasValues) != 0

	// num_keys
	numKeys := int(uint32(buf[8])<<24 | uint32(buf[9])<<16 | uint32(buf[10])<<8 | uint32(buf[11]))

	// Offsets
	trieDataOffset := int(uint32(buf[12])<<24 | uint32(buf[13])<<16 | uint32(buf[14])<<8 | uint32(buf[15]))
	valueStoreOffset := int(uint32(buf[16])<<24 | uint32(buf[17])<<16 | uint32(buf[18])<<8 | uint32(buf[19]))

	// CRC verification
	crcDataLen := len(buf) - 4
	expectedCRC := uint32(buf[crcDataLen])<<24 | uint32(buf[crcDataLen+1])<<16 | uint32(buf[crcDataLen+2])<<8 | uint32(buf[crcDataLen+3])
	actualCRC := computeCRC32(buf[:crcDataLen])
	if actualCRC != expectedCRC {
		return nil, fmt.Errorf("CRC-32 integrity check failed")
	}

	if numKeys == 0 {
		return make(map[string]interface{}), nil
	}

	// Parse trie config
	reader := NewBitReader(buf)
	dataStart := tpHeaderSize * 8

	reader.Seek(dataStart)
	bpsBits, err := reader.ReadBits(4)
	if err != nil {
		return nil, err
	}
	bps := int(bpsBits)

	symCountBits, err := reader.ReadBits(8)
	if err != nil {
		return nil, err
	}
	symbolCount := int(symCountBits)

	// Read control codes
	var ctrlCodes [numControlCodes]int
	for c := 0; c < numControlCodes; c++ {
		v, err := reader.ReadBits(bps)
		if err != nil {
			return nil, err
		}
		ctrlCodes[c] = int(v)
	}

	// Read symbol table
	var reverseMap [256]int
	for cd := numControlCodes; cd < symbolCount; cd++ {
		cp, err := readVarUint(reader)
		if err != nil {
			return nil, err
		}
		if cd < 256 && cp < 256 {
			reverseMap[cd] = cp
		}
	}

	trieStart := dataStart + trieDataOffset
	valueStart := dataStart + valueStoreOffset

	// DFS iteration
	result := make(map[string]interface{})
	keyStack := make([]byte, 0, 64)

	var dfsWalk func(r *BitReader) error
	var walkBranch func(r *BitReader, childCount int) error

	dfsWalk = func(r *BitReader) error {
		for {
			if r.Position() >= r.bitLen {
				return nil
			}
			sym, err := r.ReadBits(bps)
			if err != nil {
				return nil
			}
			s := int(sym)

			if s == ctrlCodes[ctrlEnd] {
				keyStr := string(keyStack)
				result[keyStr] = nil

				if r.Position()+bps <= r.bitLen {
					nextSym, err := r.PeekBits(bps)
					if err == nil && int(nextSym) == ctrlCodes[ctrlBranch] {
						r.ReadBits(bps) //nolint: errcheck
						cc, err := readVarUint(r)
						if err != nil {
							return err
						}
						err = walkBranch(r, cc)
						if err != nil {
							return err
						}
					}
				}
				return nil
			}

			if s == ctrlCodes[ctrlEndVal] {
				_, err := readVarUint(r) // value index
				if err != nil {
					return err
				}
				keyStr := string(keyStack)
				result[keyStr] = nil

				if r.Position()+bps <= r.bitLen {
					nextSym, err := r.PeekBits(bps)
					if err == nil && int(nextSym) == ctrlCodes[ctrlBranch] {
						r.ReadBits(bps) //nolint: errcheck
						cc, err := readVarUint(r)
						if err != nil {
							return err
						}
						err = walkBranch(r, cc)
						if err != nil {
							return err
						}
					}
				}
				return nil
			}

			if s == ctrlCodes[ctrlBranch] {
				cc, err := readVarUint(r)
				if err != nil {
					return err
				}
				return walkBranch(r, cc)
			}

			// Regular symbol
			byteVal := byte(0)
			if s < 256 {
				byteVal = byte(reverseMap[s])
			}
			keyStack = append(keyStack, byteVal)
		}
	}

	walkBranch = func(r *BitReader, childCount int) error {
		savedKeyLen := len(keyStack)
		for ci := 0; ci < childCount; ci++ {
			hasSkip := ci < childCount-1
			skipDist := 0

			if hasSkip {
				_, err := r.ReadBits(bps) // SKIP control code
				if err != nil {
					return err
				}
				skipDist, err = readVarUint(r)
				if err != nil {
					return err
				}
			}

			childStartPos := r.Position()
			keyStack = keyStack[:savedKeyLen]
			if err := dfsWalk(r); err != nil {
				return err
			}

			if hasSkip {
				r.Seek(childStartPos + skipDist)
			}
		}
		keyStack = keyStack[:savedKeyLen]
		return nil
	}

	// Walk the trie
	reader.Seek(trieStart)
	if numKeys > 0 {
		if err := dfsWalk(reader); err != nil {
			return nil, err
		}
	}

	// Decode values
	if hasValues {
		reader.Seek(valueStart)
		sortedKeys := make([]string, 0, len(result))
		for k := range result {
			sortedKeys = append(sortedKeys, k)
		}
		sort.Slice(sortedKeys, func(i, j int) bool {
			return sortedKeys[i] < sortedKeys[j]
		})
		for _, key := range sortedKeys {
			v, err := decodeValue(reader)
			if err != nil {
				return nil, err
			}
			result[key] = v
		}
	}

	return result, nil
}
