// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import "errors"

const varintMaxGroups = 10

// writeVarUint writes an unsigned LEB128 VarInt to the BitWriter.
func writeVarUint(w *BitWriter, value int) {
	if value < 0 {
		panic("writeVarUint: negative value")
	}
	v := uint64(value)
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
func readVarUint(r *BitReader) (int, error) {
	val := uint64(0)
	shift := uint(0)
	for i := 0; i < varintMaxGroups; i++ {
		b, err := r.ReadU8()
		if err != nil {
			return 0, err
		}
		val |= uint64(b&0x7F) << shift
		if (b & 0x80) == 0 {
			return int(val), nil
		}
		shift += 7
	}
	return 0, errors.New("VarInt overflow")
}

// writeVarInt writes a signed zigzag VarInt to the BitWriter.
func writeVarInt(w *BitWriter, value int) {
	var raw int
	if value >= 0 {
		raw = value * 2
	} else {
		raw = (-value)*2 - 1
	}
	writeVarUint(w, raw)
}

// readVarInt reads a signed zigzag VarInt from the BitReader.
func readVarInt(r *BitReader) (int, error) {
	raw, err := readVarUint(r)
	if err != nil {
		return 0, err
	}
	if raw&1 != 0 {
		return -(raw >> 1) - 1, nil
	}
	return raw >> 1, nil
}

// varUintBits returns the number of bits needed to encode val as a VarUint.
func varUintBits(val int) int {
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
