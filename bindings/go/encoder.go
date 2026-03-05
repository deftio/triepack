// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

package triepack

import "sort"

// Format constants (match core_internal.h).
var tpMagic = []byte{0x54, 0x52, 0x50, 0x00} // "TRP\0"

const (
	tpVersionMajor  = 1
	tpVersionMinor  = 0
	tpHeaderSize    = 32
	tpFlagHasValues = 1
)

// Control code indices.
const (
	ctrlEnd    = 0
	ctrlEndVal = 1
	ctrlSkip   = 2
	// ctrlSuffix = 3
	// ctrlEscape = 4
	ctrlBranch      = 5
	numControlCodes = 6
)

// trieEntry holds a key (as bytes) and its associated value.
type trieEntry struct {
	key []byte
	val interface{}
}

// encodeCtx holds shared context for the trie encoder.
type encodeCtx struct {
	entries   []trieEntry
	bps       int
	symbolMap [256]int
	ctrlCodes [numControlCodes]int
	hasValues bool
}

// encodeData encodes a map into the .trp binary format.
func encodeData(data map[string]interface{}) ([]byte, error) {
	// Collect entries
	entries := make([]trieEntry, 0, len(data))
	for k, v := range data {
		entries = append(entries, trieEntry{key: []byte(k), val: v})
	}

	// Sort lexicographically by key bytes
	sort.Slice(entries, func(i, j int) bool {
		a, b := entries[i].key, entries[j].key
		minLen := len(a)
		if len(b) < minLen {
			minLen = len(b)
		}
		for idx := 0; idx < minLen; idx++ {
			if a[idx] != b[idx] {
				return a[idx] < b[idx]
			}
		}
		return len(a) < len(b)
	})

	// Determine if any entries have non-null values
	hasValues := false
	for _, e := range entries {
		if e.val != nil {
			hasValues = true
			break
		}
	}

	// Symbol analysis: find unique bytes in all keys
	var used [256]bool
	for _, e := range entries {
		for _, b := range e.key {
			used[b] = true
		}
	}

	alphabetSize := 0
	for _, u := range used {
		if u {
			alphabetSize++
		}
	}
	totalSymbols := alphabetSize + numControlCodes
	bps := 1
	for (1 << uint(bps)) < totalSymbols {
		bps++
	}

	// Control codes: 0..5
	var ctrlCodes [numControlCodes]int
	for i := 0; i < numControlCodes; i++ {
		ctrlCodes[i] = i
	}

	// Symbol map: byte value -> N-bit code
	var symbolMap [256]int
	var reverseMap [256]int
	code := numControlCodes
	for i := 0; i < 256; i++ {
		if used[i] {
			symbolMap[i] = code
			if code < 256 {
				reverseMap[code] = i
			}
			code++
		}
	}

	// Build the bitstream
	w := NewBitWriter(256)

	// Write 32-byte header placeholder
	for _, b := range tpMagic {
		w.WriteU8(b)
	}
	w.WriteU8(tpVersionMajor)
	w.WriteU8(tpVersionMinor)
	flags := uint16(0)
	if hasValues {
		flags = tpFlagHasValues
	}
	w.WriteU16(flags)
	w.WriteU32(uint32(len(entries))) // num_keys
	w.WriteU32(0)                    // trie_data_offset placeholder
	w.WriteU32(0)                    // value_store_offset placeholder
	w.WriteU32(0)                    // suffix_table_offset placeholder
	w.WriteU32(0)                    // total_data_bits placeholder
	w.WriteU32(0)                    // reserved

	dataStart := w.Position() // should be 256 (32 bytes * 8)

	// Trie config: bits_per_symbol (4 bits) + symbol_count (8 bits)
	w.WriteBits(uint64(bps), 4)
	w.WriteBits(uint64(totalSymbols), 8)

	// Control code mappings
	for c := 0; c < numControlCodes; c++ {
		w.WriteBits(uint64(ctrlCodes[c]), bps)
	}

	// Symbol table
	for cd := numControlCodes; cd < totalSymbols; cd++ {
		byteVal := 0
		if cd < 256 {
			byteVal = reverseMap[cd]
		}
		writeVarUint(w, byteVal)
	}

	trieDataOffset := w.Position() - dataStart

	// Write prefix trie
	ctx := &encodeCtx{
		entries:   entries,
		bps:       bps,
		symbolMap: symbolMap,
		ctrlCodes: ctrlCodes,
		hasValues: hasValues,
	}
	valueIdx := []int{0}
	if len(entries) > 0 {
		trieWrite(ctx, w, 0, len(entries), 0, valueIdx)
	}

	valueStoreOffset := w.Position() - dataStart

	// Write value store
	if hasValues {
		for _, e := range entries {
			if err := encodeValue(w, e.val); err != nil {
				return nil, err
			}
		}
	}

	totalDataBits := w.Position() - dataStart

	w.AlignToByte()

	// Get pre-CRC buffer
	preCRCBuf := w.ToBytes()
	crcVal := computeCRC32(preCRCBuf)

	// Append CRC
	w.WriteU32(crcVal)

	outBuf := w.ToBytes()

	// Patch header offsets
	patchU32(outBuf, 12, uint32(trieDataOffset))
	patchU32(outBuf, 16, uint32(valueStoreOffset))
	patchU32(outBuf, 20, 0) // suffix_table_offset
	patchU32(outBuf, 24, uint32(totalDataBits))

	// Recompute CRC over patched data
	crcDataLen := len(outBuf) - 4
	crcVal = computeCRC32(outBuf[:crcDataLen])
	patchU32(outBuf, crcDataLen, crcVal)

	return outBuf, nil
}

// patchU32 writes a big-endian uint32 at the given byte offset.
func patchU32(buf []byte, offset int, value uint32) {
	buf[offset] = byte((value >> 24) & 0xFF)
	buf[offset+1] = byte((value >> 16) & 0xFF)
	buf[offset+2] = byte((value >> 8) & 0xFF)
	buf[offset+3] = byte(value & 0xFF)
}

// trieSubtreeSize computes the subtree size in bits (dry run).
func trieSubtreeSize(ctx *encodeCtx, start, end, prefixLen int, valueIdx []int) int {
	entries := ctx.entries
	bps := ctx.bps
	hasValues := ctx.hasValues
	bits := 0

	// Find common prefix beyond prefixLen
	common := prefixLen
	for {
		allHave := true
		ch := byte(0)
		for i := start; i < end; i++ {
			if len(entries[i].key) <= common {
				allHave = false
				break
			}
			if i == start {
				ch = entries[i].key[common]
			} else if entries[i].key[common] != ch {
				allHave = false
				break
			}
		}
		if !allHave {
			break
		}
		common++
	}

	bits += (common - prefixLen) * bps

	hasTerminal := len(entries[start].key) == common
	childrenStart := start
	if hasTerminal {
		childrenStart = start + 1
	}

	// Count children
	childCount := 0
	cs := childrenStart
	for cs < end {
		childCount++
		ch := entries[cs].key[common]
		ce := cs + 1
		for ce < end && entries[ce].key[common] == ch {
			ce++
		}
		cs = ce
	}

	if hasTerminal && childCount == 0 {
		if hasValues && entries[start].val != nil {
			bits += bps
			bits += varUintBits(valueIdx[0])
		} else {
			bits += bps
		}
		valueIdx[0]++
	} else if hasTerminal && childCount > 0 {
		if hasValues && entries[start].val != nil {
			bits += bps
			bits += varUintBits(valueIdx[0])
		} else {
			bits += bps
		}
		valueIdx[0]++
		bits += bps
		bits += varUintBits(childCount)

		cs = childrenStart
		childI := 0
		for cs < end {
			ch := entries[cs].key[common]
			ce := cs + 1
			for ce < end && entries[ce].key[common] == ch {
				ce++
			}
			if childI < childCount-1 {
				childSz := trieSubtreeSize(ctx, cs, ce, common, valueIdx)
				bits += bps
				bits += varUintBits(childSz)
				bits += childSz
			} else {
				bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx)
			}
			childI++
			cs = ce
		}
	} else { // not hasTerminal, childCount > 0
		bits += bps
		bits += varUintBits(childCount)

		cs = childrenStart
		childI := 0
		for cs < end {
			ch := entries[cs].key[common]
			ce := cs + 1
			for ce < end && entries[ce].key[common] == ch {
				ce++
			}
			if childI < childCount-1 {
				childSz := trieSubtreeSize(ctx, cs, ce, common, valueIdx)
				bits += bps
				bits += varUintBits(childSz)
				bits += childSz
			} else {
				bits += trieSubtreeSize(ctx, cs, ce, common, valueIdx)
			}
			childI++
			cs = ce
		}
	}

	return bits
}

// trieWrite writes the trie subtree to the BitWriter.
func trieWrite(ctx *encodeCtx, w *BitWriter, start, end, prefixLen int, valueIdx []int) {
	entries := ctx.entries
	bps := ctx.bps
	symbolMap := ctx.symbolMap
	ctrlCodes := ctx.ctrlCodes
	hasValues := ctx.hasValues

	// Find common prefix beyond prefixLen
	common := prefixLen
	for {
		allHave := true
		ch := byte(0)
		for i := start; i < end; i++ {
			if len(entries[i].key) <= common {
				allHave = false
				break
			}
			if i == start {
				ch = entries[i].key[common]
			} else if entries[i].key[common] != ch {
				allHave = false
				break
			}
		}
		if !allHave {
			break
		}
		common++
	}

	// Write common prefix symbols
	for i := prefixLen; i < common; i++ {
		ch := entries[start].key[i]
		w.WriteBits(uint64(symbolMap[ch]), bps)
	}

	hasTerminal := len(entries[start].key) == common
	childrenStart := start
	if hasTerminal {
		childrenStart = start + 1
	}

	// Count children
	childCount := 0
	cs := childrenStart
	for cs < end {
		childCount++
		ch := entries[cs].key[common]
		ce := cs + 1
		for ce < end && entries[ce].key[common] == ch {
			ce++
		}
		cs = ce
	}

	// Write terminal
	if hasTerminal {
		if hasValues && entries[start].val != nil {
			w.WriteBits(uint64(ctrlCodes[ctrlEndVal]), bps)
			writeVarUint(w, valueIdx[0])
		} else {
			w.WriteBits(uint64(ctrlCodes[ctrlEnd]), bps)
		}
		valueIdx[0]++
	}

	if childCount == 0 {
		return
	}

	// BRANCH
	w.WriteBits(uint64(ctrlCodes[ctrlBranch]), bps)
	writeVarUint(w, childCount)

	cs = childrenStart
	childI := 0
	for cs < end {
		ch := entries[cs].key[common]
		ce := cs + 1
		for ce < end && entries[ce].key[common] == ch {
			ce++
		}

		if childI < childCount-1 {
			viCopy := []int{valueIdx[0]}
			childSz := trieSubtreeSize(ctx, cs, ce, common, viCopy)
			w.WriteBits(uint64(ctrlCodes[ctrlSkip]), bps)
			writeVarUint(w, childSz)
		}

		trieWrite(ctx, w, cs, ce, common, valueIdx)

		childI++
		cs = ce
	}
}
