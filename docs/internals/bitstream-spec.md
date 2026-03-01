---
layout: default
title: Bitstream Specification
---

# Bitstream Specification

<!-- Copyright (c) 2026 M. A. Chatterjee -->

The `triepack_bitstream` library provides arbitrary-width bit field I/O over
a contiguous byte buffer. It is the foundation layer of the TriePack stack --
the trie codec and JSON library are built entirely on top of it.

The library descends from `dio_BitFile` (2001), which provided
`PutNBitQty(stream, value, n)` and `GetNBitQty(stream, &val, n, sign_extend)`
to pack/unpack integers of arbitrary bit width. Everything in this spec --
byte reads, VarInts, symbols, UTF-8 -- is built on that core primitive.

---

## 1. Overview

The bitstream library bridges between **packed bit fields** (storage format)
and **native integers** (compute format):

```
Bit stream (storage):   [3 bits][7 bits][12 bits][5 bits]...
Read as integers:         → 5     → 97     → 3041    → 18

Write from integers:    5,3  →  97,7  →  3041,12  →  18,5  →  packed bits
```

Key properties:

- **MSB-first bit ordering** (default, matching network byte order)
- **Separate reader and writer types** -- reader is zero-copy, ROM-safe
- **Stateless ROM functions** -- read from raw `const uint8_t *` at any bit
  offset with no object allocation
- **Growable writer** with configurable buffer growth policy
- **64-bit cursor** -- can address up to 2^64 bits (~2 exabytes)

### API Prefix

All C functions use the `tp_bs_` prefix. Types: `tp_bitstream_reader`,
`tp_bitstream_writer`.

---

## 2. Bit Numbering and Byte Layout

The library uses **MSB-first** (big-endian) bit ordering within each byte.
Bit 0 is the most significant bit:

```
Byte value 0xA5 = 1010 0101 (binary)

Bit index:        0 1 2 3  4 5 6 7
Bit value:        1 0 1 0  0 1 0 1
                  ↑ MSB           ↑ LSB
```

### Cross-Byte Field Extraction

Fields are not constrained to byte boundaries. A field can start at any
bit position and span multiple bytes.

**Example: reading a 5-bit field at bit offset 5**

```
Byte 0: [b0 b1 b2 b3 b4 | b5 b6 b7]     Byte 1: [b8 b9 | b10 ...]
                           ─────────               ──────
                           ↑ 3 bits from byte 0    ↑ 2 bits from byte 1
                           └──── 5-bit field ──────┘
```

The library extracts bits left-to-right across byte boundaries by reading
one bit at a time (MSB-first), shifting each into the result:

```c
uint64_t val = 0;
for (i = 0; i < n; i++) {
    val = (val << 1) | read_bit_msb(buf, bit_pos + i);
}
```

### Two Consecutive Writes

Writing `tp_bs_write_bits(w, 0x15, 5)` followed by `tp_bs_write_bits(w, 0x03, 3)` produces:

```
Write 0x15 (10101) in 5 bits, then 0x03 (011) in 3 bits:

Byte 0: [1 0 1 0 1 | 0 1 1]
         ─────────   ─────
         5 bits       3 bits
       = 0xAB (10101011)
```

---

## 3. Fixed-Width Unsigned Integers

```c
tp_result tp_bs_write_bits(tp_bitstream_writer *w, uint64_t value, unsigned int n);
tp_result tp_bs_read_bits(tp_bitstream_reader *r, unsigned int n, uint64_t *out);
```

Write the low `n` bits of `value` (MSB-first) and advance the cursor by `n`.
Read `n` bits into a `uint64_t`, zero-extended. Valid range: `n` in 1..64.

### Worked Examples

**3-bit write/read (value = 5)**

```
Binary: 101
Stream: [1 0 1 . . . . .]   (3 bits consumed in byte 0)
Read:   0b101 = 5
```

**5-bit write/read (value = 19)**

```
Binary: 10011
Stream: [1 0 0 1 1 . . .]   (5 bits consumed in byte 0)
Read:   0b10011 = 19
```

**12-bit write/read (value = 3041)**

```
Binary: 1011 1110 0001
Byte 0: [1 0 1 1  1 1 1 0]   = 0xBE
Byte 1: [0 0 0 1  . . . .]   (4 bits into byte 1)
Read:   0b101111100001 = 3041
```

**37-bit write/read (value = 0x1234567890 & ((1<<37)-1))**

This field spans 5 bytes. The library handles it identically on 32-bit
and 64-bit platforms -- see Section 7.

```
37 bits = 4 full bytes (32 bits) + 5 bits in byte 5
Bytes:  [b0] [b1] [b2] [b3] [b4(5 bits) + padding]
```

**64-bit write/read (maximum width)**

```
Writes all 64 bits of a uint64_t as 8 contiguous bytes (MSB-first).
```

### Fast 32-bit Read

```c
tp_result tp_bs_read_bits32(tp_bitstream_reader *r, unsigned int n, uint32_t *out);
```

Convenience function for fields known to be ≤32 bits. Returns a `uint32_t`
directly, avoiding the `uint64_t` intermediate. Recommended for 32-bit
embedded targets.

---

## 4. Fixed-Width Signed Integers

```c
tp_result tp_bs_write_bits_signed(tp_bitstream_writer *w, int64_t value, unsigned int n);
tp_result tp_bs_read_bits_signed(tp_bitstream_reader *r, unsigned int n, int64_t *out);
tp_result tp_bs_read_bits_signed_at(const uint8_t *buf, uint64_t bit_pos,
                                     unsigned int n, int64_t *out);
```

Writes a signed value truncated to `n` bits (two's complement).  Reads `n`
bits and **sign-extends** the MSB to recover the original `int64_t` value.
The valid range for an `n`-bit signed field is -(2^(n-1)) to 2^(n-1)-1.

### Sign Extension Algorithm

Given an `n`-bit unsigned value `raw`:

```
if n < 64 AND bit (n-1) of raw is set:       // MSB (sign bit) is 1
    raw |= ~(((uint64_t)1 << n) - 1)          // set all upper bits to 1
result = (int64_t)raw
```

This extends the sign bit of the n-bit field to fill all 64 bits.

### Worked Examples

**5-bit signed value -3**

```
-3 in 5-bit two's complement = 11101 (binary)

Write: tp_bs_write_bits_signed(w, -3, 5)    // truncates to 0b11101

Read:  raw = 0b11101 = 29 (unsigned)
       n = 5, bit 4 is set (sign bit)
       raw |= ~((1 << 5) - 1) = ~0x1F = 0xFFFFFFFFFFFFFFE0
       raw = 0xFFFFFFFFFFFFFFFD
       (int64_t)raw = -3  ✓
```

**3-bit signed value -1**

```
-1 in 3-bit two's complement = 111

Write: tp_bs_write_bits_signed(w, -1, 3)    // truncates to 0b111

Read:  raw = 0b111 = 7
       bit 2 is set → sign extend
       raw |= ~((1 << 3) - 1) = ~0x07 = 0xFFFFFFFFFFFFFFF8
       raw = 0xFFFFFFFFFFFFFFFF
       (int64_t)raw = -1  ✓
```

**1-bit signed**

```
n = 1: only two values are possible
  0b0 → sign bit clear → 0
  0b1 → sign bit set   → -1
```

**64-bit signed (no extension needed)**

```
n = 64: all 64 bits are read, no sign extension required
(the n < 64 check in the algorithm ensures this)
```

**Note:** `tp_bs_write_bits_signed()` is a convenience that truncates an
`int64_t` to n bits automatically.  You can also write signed values
using `tp_bs_write_bits()` if you mask manually -- the bit patterns are
identical.  The sign extension happens only on read, so the same stream
bytes can be interpreted as signed or unsigned.

---

## 5. VarInt -- Unsigned (LEB128)

```c
tp_result tp_bs_write_varint_u(tp_bitstream_writer *w, uint64_t value);
tp_result tp_bs_read_varint_u(tp_bitstream_reader *r, uint64_t *out);
```

Variable-length encoding: each byte uses 7 data bits and 1 continuation
bit (MSB). Continuation bit = 1 means more bytes follow; 0 means last byte.
Least-significant group first.

### Format Diagram

```
Byte:    [C d6 d5 d4 d3 d2 d1 d0]
          ↑ continuation bit
            ↑──────────────────↑ 7 data bits (least significant first)
```

### Worked Examples

**Value 0**

```
0 = 0b0000000 → one byte: 0x00
  [0 0000000]    C=0 (last byte), data=0
```

**Value 127**

```
127 = 0b1111111 → one byte: 0x7F
  [0 1111111]    C=0, data=127
```

**Value 128**

```
128 = 0b10000000
  Group 0: 128 & 0x7F = 0x00, remaining = 128 >> 7 = 1, more → C=1
  Group 1: 1 & 0x7F = 0x01, remaining = 0, last → C=0

  Bytes: [1 0000000] [0 0000001]  = 0x80 0x01
```

**Value 300**

```
300 = 0b100101100
  Group 0: 300 & 0x7F = 0x2C, remaining = 300 >> 7 = 2, more → C=1
  Group 1: 2 & 0x7F = 0x02, remaining = 0, last → C=0

  Bytes: [1 0101100] [0 0000010]  = 0xAC 0x02
```

**Value 16384**

```
16384 = 0b100000000000000
  Group 0: 0x00, remaining = 128, more → C=1
  Group 1: 0x00, remaining = 1, more → C=1
  Group 2: 0x01, remaining = 0, last → C=0

  Bytes: 0x80 0x80 0x01
```

### Byte Count Table

| Value range            | Bytes |
|------------------------|-------|
| 0 -- 127              | 1     |
| 128 -- 16,383         | 2     |
| 16,384 -- 2,097,151   | 3     |
| 2,097,152 -- 2^28-1   | 4     |
| 2^28 -- 2^35-1        | 5     |
| 2^35 -- 2^42-1        | 6     |
| 2^42 -- 2^49-1        | 7     |
| 2^49 -- 2^56-1        | 8     |
| 2^56 -- 2^63-1        | 9     |
| 2^63 -- 2^64-1        | 10    |

Maximum: 10 bytes for `uint64_t` (defined as `TP_VARINT_MAX_GROUPS`).

---

## 6. VarInt -- Signed (Zigzag + LEB128)

```c
tp_result tp_bs_write_varint_s(tp_bitstream_writer *w, int64_t value);
tp_result tp_bs_read_varint_s(tp_bitstream_reader *r, int64_t *out);
```

Signed integers are **zigzag-encoded** before LEB128 encoding. Zigzag
maps signed values to unsigned values so that small magnitudes (positive
or negative) use few bytes.

### Zigzag Transform

```
Encode: zigzag(n) = (n << 1) ^ (n >> 63)
Decode: if (raw & 1) val = -(raw >> 1) - 1
        else          val =  (raw >> 1)
```

### Mapping Table

| Signed value | Zigzag (unsigned) | LEB128 bytes |
|--------------|-------------------|--------------|
| 0            | 0                 | 0x00         |
| -1           | 1                 | 0x01         |
| 1            | 2                 | 0x02         |
| -2           | 3                 | 0x03         |
| 2            | 4                 | 0x04         |
| -64          | 127               | 0x7F         |
| 64           | 128               | 0x80 0x01    |
| -65          | 129               | 0x81 0x01    |

### Why Zigzag?

Without zigzag, -1 as a signed 64-bit integer is `0xFFFFFFFFFFFFFFFF`,
which requires 10 LEB128 bytes. With zigzag, -1 maps to unsigned 1,
which requires 1 byte. Small negative numbers are just as cheap as small
positive numbers.

---

## 7. 32-Bit Platform Behavior

A key design goal is that TriePack works identically on 32-bit and 64-bit
platforms with **no `#ifdef` required**.

### uint64_t Portability

C99 makes `uint64_t` technically optional (only required if the platform
has a native 64-bit integer type).  In practice, every modern 32-bit
compiler defines it because `unsigned long long` (guaranteed >=64 bits
by C99) is exactly 64 bits on all real architectures.

For strict portability, `triepack_common.h` provides a fallback:

```c
#if !defined(UINT64_MAX)
typedef unsigned long long uint64_t;
typedef long long int64_t;
#endif
```

On 32-bit hardware, the compiler implements 64-bit operations using
register pairs (two 32-bit registers). The library's `uint64_t` types
and `int64_t` types work correctly on both architectures.

### 37-Bit Fields Work on 32-Bit Hardware

`tp_bs_write_bits(w, value, 37)` works identically on 32-bit and 64-bit.
The implementation uses a bit-by-bit loop with shifts and masks:

```c
for (i = 0; i < n; i++) {
    bit = (uint8_t)((value >> (n - 1 - i)) & 1);
    write_bit_msb(w->buf, w->pos, bit);
    w->pos++;
}
```

The `value >> k` and `& 1` operations on `uint64_t` are correctly
implemented by the compiler using two-register operations on 32-bit
targets. No special handling is needed.

### Fast 32-Bit Path

For fields known to be ≤32 bits, `tp_bs_read_bits32()` returns a
`uint32_t` directly. This avoids the `uint64_t` intermediate entirely,
which is slightly faster on 32-bit hardware:

```c
tp_result tp_bs_read_bits32(tp_bitstream_reader *r, unsigned int n, uint32_t *out);
// n must be 1..32
```

### Width Parameter Type

All bit-width parameters (`n`, `bps`) use `unsigned int` rather than
`uint8_t`.  On 32-bit targets, `uint8_t` parameters require an extra
masking instruction at each call site (e.g., `AND 0xFF` on i386,
`UXTB` on ARM) because the C ABI promotes them to `int` for passing.
Using `unsigned int` eliminates this -- the value arrives in a register
as-is.  The function validates `n` internally (must be 1..64), so the
wider type adds no risk.

### Performance Considerations

| Operation         | 64-bit CPU | 32-bit CPU      |
|-------------------|------------|-----------------|
| `uint64_t` shift  | 1 cycle    | ~2-4 cycles     |
| `uint64_t` add    | 1 cycle    | ~2 cycles       |
| VarInt decode     | fast       | slightly slower  |
| Bit-level I/O     | same       | same (byte ops)  |

Rule of thumb: 64-bit arithmetic is ~2-4x slower on 32-bit hardware.
For performance-critical paths where the field width is known ≤32 bits,
use `tp_bs_read_bits32()` and 32-bit temporaries.

### CI Validation

The project builds and passes all tests under `-m32` (32-bit mode) in CI.
This is an Ubuntu GCC 32-bit job that compiles the entire library and
test suite as 32-bit code.

---

## 8. Symbol Encoding

### Fixed-Width Symbols

```c
tp_result tp_bs_write_symbol(tp_bitstream_writer *w, uint32_t val, unsigned int bps);
tp_result tp_bs_read_symbol(tp_bitstream_reader *r, unsigned int bps, uint32_t *out);
```

A "symbol" is a fixed-width field of `bps` bits (1-32). Typically used for
trie symbols where `bps` is determined by the alphabet size:

| bps | Max symbols | Typical use                     |
|-----|-------------|---------------------------------|
| 5   | 32          | English lowercase + 6 controls  |
| 6   | 64          | Alphanumeric + controls         |
| 7   | 128         | Full ASCII                      |
| 8   | 256         | Binary keys                     |

### UTF-8

```c
tp_result tp_bs_write_utf8(tp_bitstream_writer *w, uint32_t codepoint);
tp_result tp_bs_read_utf8(tp_bitstream_reader *r, uint32_t *out);
```

Reads/writes Unicode code points in standard UTF-8 encoding (1-4 bytes).
The stream must be byte-aligned for UTF-8 operations. Validates:

- Rejects overlong encodings
- Rejects surrogate pairs (U+D800..U+DFFF)
- Supports full Unicode range U+0000..U+10FFFF

---

## 9. ROM-Safe Stateless Functions

For zero-overhead access to data in ROM or flash, the library provides
stateless functions that operate on raw byte pointers:

```c
tp_result tp_bs_read_bits_at(const uint8_t *buf, uint64_t bit_pos,
                              unsigned int n, uint64_t *out);

tp_result tp_bs_read_bits_signed_at(const uint8_t *buf, uint64_t bit_pos,
                                     unsigned int n, int64_t *out);

tp_result tp_bs_read_varint_u_at(const uint8_t *buf, uint64_t bit_pos,
                                  uint64_t *out, unsigned int *bits_read);
```

These functions:

- Take no reader/writer object -- just a buffer pointer and bit position
- Allocate no memory
- Have no state (no cursor to manage)
- Are safe to call from interrupt handlers or bare-metal environments

This mirrors the original `ReadAsBitFileNBitQtyAt()` function from
`dio_BitFile`, which was designed specifically for reading from ROM
without allocating a BitFile object.

---

## 10. Buffer Management

### Writer Growth

The writer allocates a growable buffer. Growth policy is set at creation:

```c
tp_bs_writer_create(&w, initial_cap, growth);
```

- `growth > 0`: grow by `growth` bytes each time
- `growth = 0`: double the buffer each time (default)

New memory is zero-initialized (important for correct bit writes to
uninitialized bytes).

### Detach

```c
tp_result tp_bs_writer_detach_buffer(tp_bitstream_writer *w,
                                      uint8_t **buf, size_t *byte_len,
                                      uint64_t *bit_len);
```

Transfers buffer ownership to the caller. The writer becomes empty.
The caller is responsible for `free()`ing the returned buffer.

### Writer-to-Reader

```c
tp_result tp_bs_writer_to_reader(tp_bitstream_writer *w,
                                  tp_bitstream_reader **reader);
```

Creates a reader over the writer's buffer (copies the data). Useful for
round-trip testing.

### Zero-Copy Reader

```c
tp_result tp_bs_reader_create(tp_bitstream_reader **out,
                               const uint8_t *buf, uint64_t bit_len);
```

Wraps an existing buffer with no copy. The buffer must remain valid for
the reader's lifetime. This is the ROM-safe path: point the reader at
a `const` buffer in flash and read directly.

```c
tp_result tp_bs_reader_create_copy(tp_bitstream_reader **out,
                                    const uint8_t *buf, uint64_t bit_len);
```

Copies the buffer into reader-owned memory. Use when the original buffer
may be freed before the reader is done.

### Direct Pointer Access

```c
tp_result tp_bs_reader_direct_ptr(tp_bitstream_reader *r,
                                   const uint8_t **ptr, size_t n);
```

Returns a pointer directly into the reader's buffer (byte-aligned only).
Used for zero-copy access to string and blob values.

---

## 11. Error Handling

All functions return `tp_result`. The bitstream-specific errors are:

| Code                     | Value | Meaning                             |
|--------------------------|-------|-------------------------------------|
| `TP_OK`                  | 0     | Success                             |
| `TP_ERR_EOF`             | -1    | Read past end of stream             |
| `TP_ERR_ALLOC`           | -2    | Memory allocation failed            |
| `TP_ERR_INVALID_PARAM`   | -3    | NULL pointer or n=0 or n>64         |
| `TP_ERR_INVALID_POSITION`| -4    | Seek beyond buffer bounds           |
| `TP_ERR_NOT_ALIGNED`     | -5    | Byte operation on non-aligned cursor|
| `TP_ERR_OVERFLOW`        | -6    | VarInt exceeds 10 groups            |
| `TP_ERR_INVALID_UTF8`    | -7    | Malformed UTF-8 sequence            |
