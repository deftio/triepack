// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import (
	"errors"
	"io"
)

// BitWriter writes arbitrary-width bit fields into a growable buffer, MSB-first.
type BitWriter struct {
	buf []byte
	pos int // current bit position
}

// NewBitWriter creates a new BitWriter with the given initial capacity in bytes.
func NewBitWriter(initialCap int) *BitWriter {
	if initialCap <= 0 {
		initialCap = 256
	}
	return &BitWriter{
		buf: make([]byte, initialCap),
		pos: 0,
	}
}

// Position returns the current bit position.
func (w *BitWriter) Position() int {
	return w.pos
}

func (w *BitWriter) ensure(nBits int) {
	needed := (w.pos + nBits + 7) >> 3
	if needed <= len(w.buf) {
		return
	}
	newCap := len(w.buf) * 2
	for newCap < needed {
		newCap *= 2
	}
	newBuf := make([]byte, newCap)
	copy(newBuf, w.buf)
	w.buf = newBuf
}

// WriteBits writes n bits (1..64) from value, MSB-first.
func (w *BitWriter) WriteBits(value uint64, n int) {
	if n <= 0 || n > 64 {
		panic("WriteBits: n must be 1..64")
	}
	w.ensure(n)
	for i := 0; i < n; i++ {
		bit := (value >> uint(n-1-i)) & 1
		byteIdx := w.pos >> 3
		bitIdx := uint(7 - (w.pos & 7))
		if bit != 0 {
			w.buf[byteIdx] |= 1 << bitIdx
		} else {
			w.buf[byteIdx] &^= 1 << bitIdx
		}
		w.pos++
	}
}

// WriteBit writes a single bit.
func (w *BitWriter) WriteBit(val int) {
	w.ensure(1)
	byteIdx := w.pos >> 3
	bitIdx := uint(7 - (w.pos & 7))
	if val != 0 {
		w.buf[byteIdx] |= 1 << bitIdx
	} else {
		w.buf[byteIdx] &^= 1 << bitIdx
	}
	w.pos++
}

// WriteU8 writes an 8-bit value.
func (w *BitWriter) WriteU8(val uint8) {
	w.WriteBits(uint64(val), 8)
}

// WriteU16 writes a 16-bit big-endian value.
func (w *BitWriter) WriteU16(val uint16) {
	w.WriteBits(uint64(val), 16)
}

// WriteU32 writes a 32-bit big-endian value.
func (w *BitWriter) WriteU32(val uint32) {
	w.WriteBits(uint64(val), 32)
}

// WriteU64 writes a 64-bit value as two 32-bit halves (hi, lo).
func (w *BitWriter) WriteU64(hi, lo uint32) {
	w.WriteBits(uint64(hi), 32)
	w.WriteBits(uint64(lo), 32)
}

// WriteBytes writes a byte slice.
func (w *BitWriter) WriteBytes(data []byte) {
	for _, b := range data {
		w.WriteU8(b)
	}
}

// AlignToByte pads to the next byte boundary with zeros.
func (w *BitWriter) AlignToByte() {
	rem := w.pos & 7
	if rem != 0 {
		pad := 8 - rem
		w.ensure(pad)
		w.pos += pad
	}
}

// ToBytes returns the buffer truncated to the current bit position.
func (w *BitWriter) ToBytes() []byte {
	length := (w.pos + 7) >> 3
	result := make([]byte, length)
	copy(result, w.buf[:length])
	return result
}

// GetBuffer returns the underlying buffer (may be longer than used).
func (w *BitWriter) GetBuffer() []byte {
	return w.buf
}

// BitReader reads arbitrary-width bit fields from a buffer, MSB-first.
type BitReader struct {
	buf    []byte
	bitLen int
	pos    int
}

// NewBitReader creates a BitReader from the given buffer.
func NewBitReader(buf []byte) *BitReader {
	return &BitReader{
		buf:    buf,
		bitLen: len(buf) * 8,
		pos:    0,
	}
}

// Position returns the current bit position.
func (r *BitReader) Position() int {
	return r.pos
}

// Remaining returns the number of bits left.
func (r *BitReader) Remaining() int {
	return r.bitLen - r.pos
}

// Seek sets the bit position.
func (r *BitReader) Seek(bitPos int) {
	r.pos = bitPos
}

// Advance moves the bit position forward by n bits.
func (r *BitReader) Advance(nBits int) {
	r.pos += nBits
}

// ReadBits reads n bits (1..64), MSB-first.
func (r *BitReader) ReadBits(n int) (uint64, error) {
	if n <= 0 || n > 64 {
		return 0, errors.New("ReadBits: n must be 1..64")
	}
	if r.pos+n > r.bitLen {
		return 0, io.EOF
	}
	var val uint64
	for i := 0; i < n; i++ {
		byteIdx := r.pos >> 3
		bitIdx := uint(7 - (r.pos & 7))
		val = (val << 1) | uint64((r.buf[byteIdx]>>bitIdx)&1)
		r.pos++
	}
	return val, nil
}

// ReadBit reads a single bit.
func (r *BitReader) ReadBit() (int, error) {
	if r.pos >= r.bitLen {
		return 0, io.EOF
	}
	byteIdx := r.pos >> 3
	bitIdx := uint(7 - (r.pos & 7))
	bit := int((r.buf[byteIdx] >> bitIdx) & 1)
	r.pos++
	return bit, nil
}

// ReadU8 reads an 8-bit value.
func (r *BitReader) ReadU8() (uint8, error) {
	v, err := r.ReadBits(8)
	return uint8(v), err
}

// ReadU16 reads a 16-bit value.
func (r *BitReader) ReadU16() (uint16, error) {
	v, err := r.ReadBits(16)
	return uint16(v), err
}

// ReadU32 reads a 32-bit value.
func (r *BitReader) ReadU32() (uint32, error) {
	v, err := r.ReadBits(32)
	return uint32(v), err
}

// ReadU64 reads a 64-bit value (hi32 * 0x100000000 + lo32).
func (r *BitReader) ReadU64() (uint64, error) {
	hi, err := r.ReadU32()
	if err != nil {
		return 0, err
	}
	lo, err := r.ReadU32()
	if err != nil {
		return 0, err
	}
	return uint64(hi)*0x100000000 + uint64(lo), nil
}

// ReadBytes reads n bytes.
func (r *BitReader) ReadBytes(n int) ([]byte, error) {
	out := make([]byte, n)
	for i := 0; i < n; i++ {
		b, err := r.ReadU8()
		if err != nil {
			return nil, err
		}
		out[i] = b
	}
	return out, nil
}

// PeekBits reads n bits without advancing the position.
func (r *BitReader) PeekBits(n int) (uint64, error) {
	saved := r.pos
	val, err := r.ReadBits(n)
	r.pos = saved
	return val, err
}

// AlignToByte advances to the next byte boundary.
func (r *BitReader) AlignToByte() {
	rem := r.pos & 7
	if rem != 0 {
		r.pos += 8 - rem
	}
}
