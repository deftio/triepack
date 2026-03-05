// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"fmt"
	"math"
)

// Value type tags (4-bit, matches tp_value_type enum).
const (
	tpNull    = 0
	tpBool    = 1
	tpInt     = 2
	tpUint    = 3
	tpFloat32 = 4
	tpFloat64 = 5
	tpString  = 6
	tpBlob    = 7
)

// encodeValue writes a Go value into the bitstream.
func encodeValue(w *BitWriter, val interface{}) error {
	if val == nil {
		w.WriteBits(tpNull, 4)
		return nil
	}

	switch v := val.(type) {
	case bool:
		w.WriteBits(tpBool, 4)
		if v {
			w.WriteBit(1)
		} else {
			w.WriteBit(0)
		}
		return nil

	case int:
		if v < 0 {
			w.WriteBits(tpInt, 4)
			writeVarInt(w, v)
		} else {
			w.WriteBits(tpUint, 4)
			writeVarUint(w, v)
		}
		return nil

	case int64:
		if v < 0 {
			w.WriteBits(tpInt, 4)
			writeVarInt(w, int(v))
		} else {
			w.WriteBits(tpUint, 4)
			writeVarUint(w, int(v))
		}
		return nil

	case uint64:
		w.WriteBits(tpUint, 4)
		writeVarUint(w, int(v))
		return nil

	case float32:
		w.WriteBits(tpFloat32, 4)
		bits := math.Float32bits(v)
		w.WriteU32(bits)
		return nil

	case float64:
		w.WriteBits(tpFloat64, 4)
		bits := math.Float64bits(v)
		hi := uint32(bits >> 32)
		lo := uint32(bits & 0xFFFFFFFF)
		w.WriteU64(hi, lo)
		return nil

	case string:
		encoded := []byte(v)
		w.WriteBits(tpString, 4)
		writeVarUint(w, len(encoded))
		w.AlignToByte()
		w.WriteBytes(encoded)
		return nil

	case []byte:
		w.WriteBits(tpBlob, 4)
		writeVarUint(w, len(v))
		w.AlignToByte()
		w.WriteBytes(v)
		return nil

	default:
		return fmt.Errorf("unsupported value type: %T", val)
	}
}

// decodeValue reads a value from the bitstream.
func decodeValue(r *BitReader) (interface{}, error) {
	tagBits, err := r.ReadBits(4)
	if err != nil {
		return nil, err
	}
	tag := int(tagBits)

	switch tag {
	case tpNull:
		return nil, nil

	case tpBool:
		bit, err := r.ReadBit()
		if err != nil {
			return nil, err
		}
		return bit != 0, nil

	case tpInt:
		v, err := readVarInt(r)
		if err != nil {
			return nil, err
		}
		return v, nil

	case tpUint:
		v, err := readVarUint(r)
		if err != nil {
			return nil, err
		}
		return v, nil

	case tpFloat32:
		bits, err := r.ReadU32()
		if err != nil {
			return nil, err
		}
		return float64(math.Float32frombits(bits)), nil

	case tpFloat64:
		hi, err := r.ReadU32()
		if err != nil {
			return nil, err
		}
		lo, err := r.ReadU32()
		if err != nil {
			return nil, err
		}
		bits := uint64(hi)<<32 | uint64(lo)
		return math.Float64frombits(bits), nil

	case tpString:
		slen, err := readVarUint(r)
		if err != nil {
			return nil, err
		}
		r.AlignToByte()
		raw, err := r.ReadBytes(slen)
		if err != nil {
			return nil, err
		}
		return string(raw), nil

	case tpBlob:
		blen, err := readVarUint(r)
		if err != nil {
			return nil, err
		}
		r.AlignToByte()
		data, err := r.ReadBytes(blen)
		if err != nil {
			return nil, err
		}
		return data, nil

	default:
		return nil, fmt.Errorf("unknown value type tag: %d", tag)
	}
}
