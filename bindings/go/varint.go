// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import "errors"

const varintMaxGroups = 10

// writeVarUint writes an unsigned LEB128 VarInt to the BitWriter.
//
// The value is a uint64 so the full unsigned range round-trips; an int would
// turn anything above MaxInt64 negative on the way in.
func writeVarUint(w *BitWriter, value uint64) {
	v := value
	for {
		b := uint8(v & 0x7F)
		v >>= 7
		if v > 0 {
			b |= 0x80
		}
		w.WriteU8(b)
		if v == 0 {
			break
		}
	}
}

// readVarUint reads an unsigned LEB128 VarInt from the BitReader.
func readVarUint(r *BitReader) (uint64, error) {
	val := uint64(0)
	shift := uint(0)
	for i := 0; i < varintMaxGroups; i++ {
		b, err := r.ReadU8()
		if err != nil {
			return 0, err
		}
		val |= uint64(b&0x7F) << shift
		if (b & 0x80) == 0 {
			return val, nil
		}
		shift += 7
	}
	return 0, errors.New("VarInt overflow")
}

// writeVarInt writes a signed zigzag VarInt to the BitWriter.
//
// Zigzag is computed on the bit pattern rather than by negating, which would
// overflow at MinInt64.
func writeVarInt(w *BitWriter, value int64) {
	raw := uint64(value<<1) ^ uint64(value>>63)
	writeVarUint(w, raw)
}

// readVarInt reads a signed zigzag VarInt from the BitReader.
func readVarInt(r *BitReader) (int64, error) {
	raw, err := readVarUint(r)
	if err != nil {
		return 0, err
	}
	return int64(raw>>1) ^ -int64(raw&1), nil
}

// varUintBits returns the number of bits needed to encode val as a VarUint.
func varUintBits(val uint64) int {
	bits := 0
	v := val
	for {
		bits += 8
		v >>= 7
		if v == 0 {
			break
		}
	}
	return bits
}
