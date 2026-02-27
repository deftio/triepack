# TXZ Stream Library Specification

Foundation library for TXZ: a read/write stream over a byte buffer with bit-level cursor tracking and multiple addressing modes.

---

## 1. Purpose

### 1.0 Core Concept: Bit Fields to Integers

The fundamental job of this library is to **bridge between packed bit fields (storage format) and native integers (compute format)**. You store a 3-bit value, a 7-bit value, a 12-bit value packed adjacently in a bit stream with no byte alignment. When you read them back, they come out as ordinary integers you can do arithmetic on. The bit stream is the wire/storage representation; integers are the compute representation.

```
Bit stream (storage):    [3 bits][7 bits][12 bits][5 bits]...
                          ^^^     ^^^^^^^  ^^^^^^^^^^^^  ^^^^^
Read as integers:         → 5     → 97     → 3041        → 18
                          (u8)    (u8)     (u16)         (u8)

Write from integers:     5,3  →  97,7  →  3041,12  →  18,5  →  packed bit stream
```

This is what the original `dio_BitFile` provided: `PutNBitQty(stream, 3041, 12)` packs the integer 3041 into 12 bits at the current cursor position, and `GetNBitQty(stream, &val, 12, sign_extend)` reads those 12 bits back out as a native integer -- optionally sign-extended for signed values.

Everything else in this spec (byte reads, symbol reads, VarInts, UTF-8) is built on top of this core primitive.

### 1.1 Design Goals

TXZ needs to read and write data at three granularities -- bit, byte, and symbol (including variable-width UTF-8 code points) -- often within the same file. Rather than building this into the trie codec directly, we factor it out as a standalone stream library that:

- **Packs arbitrary-width integers into a bit stream and extracts them back** -- the core operation
- Wraps a contiguous byte buffer (may be ROM-mapped, heap-allocated, or memory-mapped file)
- Tracks position as a **bit offset** internally, regardless of addressing mode
- Exposes typed read/write operations at bit, byte, and symbol granularity
- Handles VarInt encoding/decoding at any alignment
- Is suitable for both encoding (write path) and decoding (read path)
- Has no allocator dependency for the read path (ROM-safe)
- Provides **stateless static read functions** for zero-overhead ROM access (no object needed)

### 1.1 Historical Reference: `dio_BitFile`

The original C implementation (`dev/bitfile.h`, authored by M. A. Chatterjee) provides the direct ancestor of this library. Key design patterns carried forward:

| `dio_BitFile` pattern | Modern equivalent | Notes |
|----------------------|-------------------|-------|
| Opaque `dio_BitFile` struct, Construct/Destruct via double-pointer | Reader/Writer types | C: keep opaque struct pattern |
| Single bit cursor for both read and write (`SetPosition`/`GetPosition`) | Single cursor per stream instance | Unchanged |
| `PutNBitQty(npThis, nNum, nNumBits)` | `write_bits(value, n)` | Core write primitive |
| `GetNBitQty(npThis, npNum, nNumBits, nbSignExtend)` | `read_bits_signed(n)` / `read_bits(n)` | **Sign extension on read** -- the original supported this as a parameter; modern version provides both signed and unsigned reads |
| `ReadAsBitFileNBitQtyAt(npBytes, npNum, nNumBits, nBitPos, nbSignExtend)` | `read_bits_at(buf, bit_pos, n)` | **Stateless static function** -- reads from a raw byte pointer at a bit offset without allocating a BitFile object. Critical for ROM: no struct, no cursor, no allocator. |
| `nbMakeCopy` flag in constructor | `from_buffer()` (no copy) vs `from_buffer_copy()` (copies) | When false: wraps existing buffer (ROM-safe read-only). When true: copies into owned buffer (mutable). |
| `nAllocIncSize` growth increment | Configurable growth policy | Original allowed caller to control allocation granularity |
| `AddBuffer()` / `GetBuffer()` | `append_buffer()` / `get_buffer()` | Import/export raw bytes |
| `dio_BITFILE_BITORDER_BE` / `_LE` compile-time defines | Configurable bit ordering | Original supported both MSB-first and LSB-first within each byte |
| `dio_RESULT` return codes | Result/error returns | Every operation returns a status |
| `u32` bit positions (max ~512MB) | `u64` bit positions | Extended for modern use |

---

## 2. Core Concepts

### 2.1 Buffer and Cursor

```
Buffer:  [byte 0] [byte 1] [byte 2] [byte 3] ...
Bits:    0      7 8     15 16    23 24    31 ...

Cursor:  a single uint64 bit-position into the buffer
```

The cursor is always a **bit offset** from the start of the buffer. All operations advance the cursor by the number of bits consumed/produced.

- `cursor = 0` means bit 0 of byte 0 (MSB-first by default)
- `cursor = 13` means bit 5 of byte 1 (0-indexed from MSB)

### 2.2 Bit Ordering

The original `dio_BitFile` supported both bit orderings via compile-time defines (`dio_BITFILE_BITORDER_BE` / `_LE`). Modern TXZ makes this a runtime-configurable property of each stream instance, defaulting to MSB-first.

**MSB-first (big-endian bit order, default):**

```
Byte value 0xA5 = 1010_0101
                  ^         ^
               bit 0     bit 7
              (MSB)      (LSB)
```

Bits are numbered left-to-right within a byte: bit 0 is the most significant bit. This matches the original `dio_BITFILE_BITORDER_BE` (76543210) mode and is the natural order for network protocols and the TXZ VarInt convention.

**LSB-first (little-endian bit order):**

```
Byte value 0xA5 = 1010_0101
                  ^         ^
               bit 7     bit 0
              (MSB)      (LSB)
```

Bits are numbered right-to-left: bit 0 is the least significant bit. This matches `dio_BITFILE_BITORDER_LE` (01234567) and is natural for hardware register access on little-endian processors.

The TXZ file format specifies MSB-first. LSB-first is available for other uses of the stream library.

### 2.3 Addressing Modes

The stream itself is always bit-addressed internally. Addressing modes are a **policy layer** on top that constrains how the cursor advances:

| Mode | Read/write unit | Cursor advancement | Alignment |
|------|----------------|-------------------|-----------|
| **bit** | 1-64 bits | exact bit count | none |
| **byte** | 8 bits | 8 bits | cursor must be byte-aligned (multiple of 8) before byte-mode operations |
| **symbol-fixed** | N bits (declared width) | N bits | none (packed) or byte-aligned (configurable) |
| **symbol-utf8** | 1-4 bytes (8-32 bits) | 8, 16, 24, or 32 bits per code point | byte-aligned |

Mode is a property of the caller, not the stream. The stream exposes all operations always; the TXZ codec enforces mode discipline.

---

## 3. API

### 3.1 Construction

Following the original `dio_BitFile` pattern, construction distinguishes between wrapping an existing buffer (read-only, ROM-safe) and allocating a new one (read-write, growable).

```
BitStream.from_buffer(buffer: *const u8, bit_length: u64) -> BitStreamReader
    -- Wrap an existing buffer for reading. No copy, no allocation. ROM-safe.
       The buffer is treated as immutable.
       (Original: dio_BitFile_Construct with nbMakeCopy=false)

BitStream.from_buffer_copy(buffer: *const u8, bit_length: u64) -> BitStream
    -- Copy buffer into owned memory. Supports both read and write.
       (Original: dio_BitFile_Construct with nbMakeCopy=true)

BitStream.new_writer(initial_capacity_bytes: usize, growth_increment_bytes: usize) -> BitStreamWriter
    -- Allocate a growable write buffer with configurable growth.
       growth_increment_bytes: how much to grow when buffer is full (0 = double).
       (Original: nAllocIncSize parameter)

BitStream.from_writer(writer: BitStreamWriter) -> BitStreamReader
    -- Finalize a writer into a reader (for verification / round-trip testing).
       (Original: dio_BitFile_GetBuffer then dio_BitFile_Construct)
```

### 3.2 Cursor Operations

```
.bit_position() -> u64
    -- Current cursor position in bits.

.byte_position() -> u64
    -- Current cursor position in bytes (floor division, for byte-aligned contexts).

.seek_bits(pos: u64)
    -- Set cursor to absolute bit position.

.seek_bytes(pos: u64)
    -- Set cursor to absolute byte position (pos * 8).

.advance_bits(n: u64)
    -- Move cursor forward by n bits.

.advance_bytes(n: u64)
    -- Move cursor forward by n bytes (n * 8 bits).

.remaining_bits() -> u64
    -- Bits remaining from cursor to end.

.align_to_byte()
    -- Advance cursor to next byte boundary (0-7 bit advance). On write, pads with zero bits.
```

### 3.3 Bit-Level Read/Write

```
.read_bits(n: u8) -> u64          // n in 1..=64
    -- Read n bits at current position, return as u64 (MSB-first, zero-extended).
       Advance cursor by n.
       (Original: dio_BitFile_GetNBitQty with nbSignExtend=false)

.read_bits_signed(n: u8) -> i64   // n in 1..=64
    -- Read n bits at current position with sign extension. The MSB of the n-bit
       field is treated as a sign bit and extended to fill i64.
       Advance cursor by n.
       (Original: dio_BitFile_GetNBitQty with nbSignExtend=true)

.write_bits(value: u64, n: u8)    // n in 1..=64
    -- Write low n bits of value at current position (MSB-first). Advance cursor by n.
       (Original: dio_BitFile_PutNBitQty)

.read_bit() -> bool
    -- Read 1 bit. Shorthand for read_bits(1).
       (Original: dio_BitFile_GetBit)

.write_bit(value: bool)
    -- Write 1 bit. Shorthand for write_bits(value, 1).
       (Original: dio_BitFile_PutBit)

.peek_bits(n: u8) -> u64
    -- Read n bits without advancing cursor.
```

### 3.3.1 Stateless Static Read (ROM Path)

These functions operate on a raw byte pointer with no stream object, no cursor, and no allocation. They are the lowest-overhead way to read from ROM-mapped data.

```
BitStream.read_bits_at(buffer: *const u8, bit_position: u64, n: u8) -> u64
    -- Read n bits starting at bit_position from buffer. No state, no side effects.
       (Original: dio_BitFile_ReadAsBitFileNBitQtyAt with nbSignExtend=false)

BitStream.read_bits_signed_at(buffer: *const u8, bit_position: u64, n: u8) -> i64
    -- Same, with sign extension.
       (Original: dio_BitFile_ReadAsBitFileNBitQtyAt with nbSignExtend=true)
```

These are pure functions -- they take a pointer, a position, and a width, and return a value. In C, they are standalone functions (not methods on a struct). They enable ROM lookups without instantiating any object.

### 3.4 Byte-Level Read/Write

```
.read_u8() -> u8
    -- Read 8 bits as unsigned byte. Works at any bit alignment.

.write_u8(value: u8)
    -- Write 8 bits. Works at any bit alignment.

.read_bytes(n: usize) -> &[u8]    // or copies into caller buffer
    -- Read n bytes. If cursor is byte-aligned, this can be a zero-copy slice of the buffer.

.write_bytes(data: &[u8])
    -- Write raw bytes.

.read_u16() / .read_u32() / .read_u64()
    -- Read fixed-width integers (big-endian or little-endian, configurable per stream).

.write_u16(v) / .write_u32(v) / .write_u64(v)
    -- Write fixed-width integers.
```

### 3.5 VarInt Read/Write

```
.read_varint_u() -> u64
    -- Read unsigned VarInt (8-bit groups, MSB continuation protocol).
       Works at any bit alignment: reads 8 bits at current position,
       checks continuation bit, repeats.

.write_varint_u(value: u64)
    -- Write unsigned VarInt.

.read_varint_s() -> i64
    -- Read signed VarInt (zig-zag decoded).

.write_varint_s(value: i64)
    -- Write signed VarInt (zig-zag encoded).
```

### 3.6 Symbol Read/Write

These operations are parameterized by the current symbol configuration.

```
.read_symbol(bits_per_symbol: u8) -> u32
    -- Read one fixed-width symbol (bits_per_symbol bits, returned as u32).

.write_symbol(value: u32, bits_per_symbol: u8)
    -- Write one fixed-width symbol.

.read_utf8_codepoint() -> u32
    -- Read one UTF-8 encoded code point (1-4 bytes) from current position.
       Cursor must be byte-aligned (or at a bit position where the next
       bits form valid UTF-8 bytes).

.write_utf8_codepoint(cp: u32)
    -- Write one UTF-8 encoded code point.
```

### 3.7 Buffer Import/Export

```
.append_buffer(buffer: *const u8, bit_length: u64)
    -- Append raw bits from an external buffer to the write stream.
       (Original: dio_BitFile_AddBuffer)

.get_buffer() -> (*const u8, u64)
    -- Return pointer to the underlying byte buffer and its total length in bits.
       For readers, this is the original buffer. For writers, this is the accumulated output.
       (Original: dio_BitFile_GetBuffer)

.get_size_bits() -> u64
    -- Total size of the stream in bits.
       (Original: dio_BitFile_GetBitFileSize)

.get_size_bytes() -> u64
    -- Total size in bytes (ceiling: (bits + 7) / 8).
       (Original: (NumBitsInStream >> 3) + ((NumBitsInStream&7)?1:0) )
```

### 3.8 Bulk / Convenience

```
.read_symbol_string(bits_per_symbol: u8, count: usize) -> Vec<u32>
    -- Read `count` fixed-width symbols.

.read_utf8_string(byte_len: usize) -> String
    -- Read `byte_len` UTF-8 bytes as a string.

.copy_bits_to(dest: &mut BitStreamWriter, n_bits: u64)
    -- Copy n bits from reader to writer (for sub-stream extraction).
```

---

## 4. Implementation Notes

### 4.1 Bit Extraction

The core operation is extracting N bits starting at an arbitrary bit offset from a byte array. Efficient implementation:

```
fn read_bits(buf: &[u8], bit_pos: u64, n: u8) -> u64:
    // Calculate byte index and bit offset within that byte
    byte_idx = bit_pos / 8
    bit_off  = bit_pos % 8    // 0 = MSB of byte

    // Read enough bytes to cover the request (at most 9 bytes for 64 bits + offset)
    // Shift and mask to extract the desired bits
    // Return MSB-first
```

For ROM-safety, this must work with simple pointer arithmetic and shifts -- no allocation, no copies beyond a register-sized working variable.

### 4.2 Write Buffer Growth

The writer maintains a byte buffer that grows as needed. Following the original `nAllocIncSize` pattern, the caller controls growth:

- Initial capacity and growth increment provided at construction
- If growth increment > 0: grow by that many bytes each time (linear, predictable memory use)
- If growth increment == 0: double the buffer (amortized O(1), fewer allocations for large streams)
- `finalize()` trims to exact bit length, returns buffer + bit length
- Trailing bits in the last byte (beyond `total_bit_length`) are zero-padded

### 4.3 Zero-Copy Read Path

When the cursor is byte-aligned and the caller requests N bytes, `read_bytes()` can return a slice directly into the underlying buffer with no copy. This is critical for ROM-mapped operation.

When the cursor is not byte-aligned, `read_bytes()` must shift-copy into a caller-provided buffer.

### 4.4 Endianness

- **Bit order within bytes**: MSB-first (big-endian), matching original TXZ
- **Multi-byte integers** (u16, u32, u64 in the file header): configurable, default little-endian (matching modern platforms)
- **VarInt byte order**: always big-endian (MSB group first), matching original flag-enc-num
- **IEEE 754 floats**: stored in the byte order declared by the stream's endianness setting

### 4.5 Error Handling

Following the original `dio_RESULT` pattern, every operation in the C implementation returns a result code. Other languages may use idiomatic error handling (Result types, exceptions, etc.) but the C API always returns status.

```c
// C error model (following dio_RESULT pattern)
typedef enum {
    TXZ_OK = 0,
    TXZ_ERR_END_OF_STREAM,    // read past end of buffer
    TXZ_ERR_ALLOC_FAILED,     // write buffer growth failed
    TXZ_ERR_INVALID_UTF8,     // UTF-8 decode error
    TXZ_ERR_VARINT_OVERFLOW,  // VarInt exceeds max groups (>10 for u64, >5 for u32)
    TXZ_ERR_INVALID_POSITION, // seek to position beyond stream bounds
} txz_result;
```

Read operations fail if:
- Reading past end of buffer
- UTF-8 decode error in `read_utf8_codepoint()`
- VarInt exceeds maximum group count (>10 for 64-bit, >5 for 32-bit)
- Seek to invalid position

Write operations fail if:
- Write buffer allocation fails (writer only)

### 4.6 Performance

The original `dio_BitFile` was written for correctness, not speed. It works, but the read/write paths do more work per call than necessary (e.g. bounds-checking and byte-index arithmetic on every single-bit read). For a modern implementation:

**Phase 1 -- Correct (initial implementation):**

Get the API right. Straightforward bit extraction using byte-index + shift + mask. Match `dio_BitFile` behavior exactly. This is sufficient for encoding (write path, runs once) and for small datasets.

**Phase 2 -- Fast (future optimization):**

The read path is performance-critical because TXZ lookups happen at query time, potentially in tight loops. Optimization targets:

| Technique | Description |
|-----------|-------------|
| **Word-at-a-time reads** | Load a 32-bit or 64-bit word from the buffer, shift/mask to extract. Avoids per-byte loads for multi-bit reads. Requires care with unaligned access on some architectures. |
| **Cursor caching** | Keep the current word in a register with a "bits remaining" count. Only reload from the buffer when exhausted. Amortizes memory access across multiple reads. |
| **32-bit fast path** | For reads of <= 32 bits (the overwhelming common case in TXZ), use 32-bit arithmetic only. Avoid 64-bit shifts on 32-bit processors. |
| **Byte-aligned fast path** | When the cursor happens to be byte-aligned, use direct byte/word loads instead of shift/mask. |
| **Branch elimination** | The inner read loop for trie traversal should be branch-light. Avoid branching on alignment within the hot path. |
| **Platform-specific intrinsics** | `__builtin_bswap32/64`, unaligned load intrinsics, etc. -- behind a clean abstraction layer. |

**Non-goals for speed:**
- The write path does not need to be as fast as the read path (encoding is offline/batch).
- The static ROM-read functions (`read_bits_at`) are inherently stateless and can't benefit from cursor caching, but they're used for one-off reads, not tight loops.

---

## 5. Language and Platform Considerations

### 5.1 Implementation Language

The stream library is implemented in **C** with **C++ wrappers**, matching the TXZ library itself.

| Component | Language | Notes |
|-----------|----------|-------|
| **txz-bitstream (core)** | C (C99+) | Primary implementation. Opaque struct + function API, following the `dio_BitFile` pattern. No allocator needed for reader. `stdint.h` types throughout. |
| **txz-bitstream (C++ wrapper)** | C++ | Thin RAII wrapper. Constructor/destructor for ownership. Method syntax over the C API. No additional logic. |
| **Test harness** | C (or C++ for convenience) | 100% code coverage required. Every function, every branch, every error path. This is an embedded library -- it must be solid. |

Future bindings for other languages (Python, JS/TS, Rust) may wrap the C library via FFI. The C implementation is the single source of truth.

### 5.2 32-bit and 64-bit Portability

The library must work on both 32-bit and 64-bit processors. Pragmatic approach:

- **Bit positions**: Use `uint64_t` for bit positions and stream sizes. On 32-bit targets this limits nothing practical (the original used `u32`, capping at ~512MB; `uint64_t` is available on all modern 32-bit compilers via `<stdint.h>`).
- **Read/write values**: `read_bits()` returns up to 64 bits in a `uint64_t`. On 32-bit targets, 64-bit operations are emulated but the compiler handles this -- no special code needed for values <= 32 bits (the common case).
- **Buffer sizes**: Use `size_t` for byte buffer sizes (naturally 32 or 64 bit). Bit lengths use `uint64_t`.
- **Avoid pointer-width assumptions**: Never cast between pointers and integers for bit arithmetic. Keep bit positions in dedicated integer variables.
- **Performance**: On 32-bit targets, the hot path (reading N <= 32 bits) should use 32-bit arithmetic where possible. The C implementation can provide `read_bits32()` / `write_bits32()` fast paths that avoid 64-bit operations.
- **No `#ifdef` explosion**: The goal is clean portable C, not a maze of platform conditionals. Use `stdint.h` types, let the compiler handle the rest.

---

## 6. Testing Strategy

**100% test coverage is mandatory.** Every function, every branch, every error path. This is foundation code for an embedded library -- untested paths are unshippable paths.

Coverage is measured with `gcov` / `lcov` (or equivalent). CI enforces the coverage threshold.

### 6.1 Round-Trip Tests

For every operation, write a value and read it back:
- `write_bits(v, n)` then `read_bits(n)` == v, for all n in 1..64, for boundary values
- VarInt round-trip for edge cases: 0, 127, 128, 16383, 16384, u64::MAX
- UTF-8 round-trip for 1-byte, 2-byte, 3-byte, 4-byte code points

### 6.2 Alignment Tests

- Read/write at every bit offset 0-7 within a byte, verify correct extraction
- Mixed operations: write 3 bits, write a VarInt, write 5 bits, then read back in same order
- Byte-mode read after non-byte-aligned bit writes (with explicit `align_to_byte()`)

### 6.3 Cross-Mode Tests

- Write a sequence in bit mode, read back in byte mode (after alignment)
- Write symbols at 5-bit width, intersperse VarInts, read back and verify
- UTF-8 code point write followed by bit-level read of the same bytes

### 6.4 ROM-Safety Tests

- Reader constructed from a `const` buffer (no mutation)
- Verify no heap allocations during read path
- Verify correct behavior when buffer is at an arbitrary base address (not page-aligned)

### 6.5 Compatibility Tests

- Write with one language implementation, read with another
- Verify bit-identical output from all implementations for the same input sequence
