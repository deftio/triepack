---
layout: default
title: Bitstream Guide
---

# Understanding Arbitrary-Width Bit Fields

<!-- Copyright (c) 2026 M. A. Chatterjee -->

Most programming works with bytes (8 bits), words (16/32/64 bits), and
the standard integer types built on them. TriePack's bitstream library
operates at a lower level: it reads and writes integers of **any width
from 1 to 64 bits**, packed tightly with no wasted space between fields.

This guide explains the concepts from first principles.  If you've
worked with hardware registers, binary protocols, or compression formats,
some of this will be familiar.  If not, read on -- the ideas are
straightforward once you see them.

---

## 1. Why Sub-Byte Integers?

Consider storing the letters a-z in a trie.  With 26 letters + 6 control
codes = 32 symbols, each symbol needs exactly 5 bits (because 2^5 = 32).
Using a full byte per symbol wastes 3 bits -- a 37.5% overhead.

For 10,000 keys averaging 8 characters each, that's:

```
Full bytes:  10,000 x 8 x 8 = 640,000 bits
5-bit packed: 10,000 x 8 x 5 = 400,000 bits  (37.5% smaller)
```

The bitstream library lets you write `tp_bs_write_bits(w, symbol, 5)` and
read `tp_bs_read_bits(r, 5, &val)` -- packing each symbol into exactly
5 bits, no padding, no wasted space.

---

## 2. Bit Numbering: MSB First

Within each byte, bits are numbered from the **most significant bit**
(MSB, leftmost) to the **least significant bit** (LSB, rightmost):

```
Byte value: 0xA5 = 165 = 10100101 in binary

Bit index:   0  1  2  3  4  5  6  7
Bit value:   1  0  1  0  0  1  0  1
             ^                    ^
             MSB (bit 0)         LSB (bit 7)
             weight = 128        weight = 1
```

This is called **MSB-first** or **big-endian bit ordering**, and it
matches network byte order.  The first bit written is the most
significant bit of the value.

---

## 3. Writing Packed Bit Fields

When you write a value with fewer than 8 bits, the bits are packed
into the output buffer starting from the MSB of the current byte,
continuing into the next byte when the current byte is full.

### Example: Two Fields, One Byte

```c
tp_bs_write_bits(w, 21, 5);   // 21 = 10101 in binary (5 bits)
tp_bs_write_bits(w,  3, 3);   //  3 = 011   in binary (3 bits)
```

The resulting byte in memory:

```
Bit index:   0  1  2  3  4  5  6  7
Written by:  ── first call ──  ─ second ─
Bit value:   1  0  1  0  1  0  1  1
             └──── 21 ────┘  └─ 3 ─┘

Byte = 10101011 = 0xAB
```

Two values, two different widths, packed into a single byte with zero
wasted bits.

### Example: Crossing a Byte Boundary

```c
tp_bs_write_bits(w, 21, 5);   // 10101 → bits 0-4 of byte 0
tp_bs_write_bits(w, 97, 7);   // 1100001 → bits 5-7 of byte 0 + bits 0-3 of byte 1
```

```
Byte 0:      0  1  2  3  4  5  6  7
             1  0  1  0  1  1  1  0   = 0xAE
             └──── 21 ────┘  └─ first 3 bits of 97

Byte 1:      0  1  2  3  4  5  6  7
             0  0  0  1  .  .  .  .
             ─ last 4 bits of 97 ─┘

97 in binary: 1100001 (7 bits)
Written across the boundary: byte 0 gets 110, byte 1 gets 0001
```

The library handles boundary crossing automatically.  You never need to
think about which byte you're in.

---

## 4. Reading Packed Bit Fields

Reading is the reverse.  You specify how many bits to read, and the
library extracts them from the current position in the stream, advancing
the cursor.

```c
uint64_t val;
tp_bs_read_bits(r, 5, &val);   // reads 5 bits → val = 21
tp_bs_read_bits(r, 7, &val);   // reads 7 bits → val = 97
```

The reader maintains an internal cursor (in bits, not bytes).  After
reading 5 bits, the cursor is at bit 5.  After reading 7 more, it's at
bit 12.  The cursor can be at any bit position -- it is not constrained
to byte boundaries.

### Reading from ROM (Stateless)

For embedded systems where data is in flash/ROM, stateless functions
read from a raw buffer with no reader object:

```c
static const uint8_t rom_data[] = { /* .trp file content */ };

uint64_t val;
tp_bs_read_bits_at(rom_data, 40, 5, &val);   // read 5 bits at bit 40
```

No object, no cursor, no heap allocation.  Safe for interrupt handlers.

---

## 5. Unsigned vs. Signed: What Changes

### Unsigned (the default)

When you write `tp_bs_write_bits(w, 21, 5)`, the 5-bit unsigned value 21
is stored.  On read, the 5 bits are returned as-is in a `uint64_t`,
zero-extended to 64 bits.  The range of a 5-bit unsigned field is
0 to 31 (2^5 - 1).

```
Width    Unsigned range
─────    ──────────────
1 bit    0 to 1
3 bits   0 to 7
5 bits   0 to 31
8 bits   0 to 255
13 bits  0 to 8191
32 bits  0 to 4,294,967,295
64 bits  0 to 18,446,744,073,709,551,615
```

### Signed (two's complement with sign extension)

A signed n-bit integer uses **two's complement** representation, just
like the C `int` type but at an arbitrary width.  The most significant
bit (MSB) is the **sign bit**: 0 for non-negative, 1 for negative.

```
Width    Signed range
─────    ────────────
1 bit    -1 to 0
3 bits   -4 to 3
5 bits   -16 to 15
8 bits   -128 to 127
13 bits  -4096 to 4095
32 bits  -2,147,483,648 to 2,147,483,647
64 bits  -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
```

The formula: an n-bit signed field holds values from -(2^(n-1)) to
2^(n-1) - 1.

---

## 6. Two's Complement at Arbitrary Widths

Two's complement is how computers represent negative integers.  You
already use it with `int8_t`, `int16_t`, `int32_t`, and `int64_t`.
TriePack extends this to any width.

### How It Works

For an n-bit signed number, the value of bit pattern B is:

```
If MSB = 0:  value =  B                    (positive or zero)
If MSB = 1:  value =  B - 2^n              (negative)
```

### Example: 5-bit Signed

```
Bit pattern    Unsigned value    Signed value (5-bit)
───────────    ──────────────    ────────────────────
00000          0                  0
00001          1                  1
00010          2                  2
...
01111          15                 15       (maximum positive)
10000          16                -16       (minimum negative)
10001          17                -15
...
11101          29                -3
11110          30                -2
11111          31                -1
```

The same 5 bits, same bit pattern.  The only difference is
**interpretation**: unsigned treats all bits as magnitude; signed treats
the MSB as a sign bit and negates accordingly.

---

## 7. Sign Extension: From N Bits to 64 Bits

When you read a signed 5-bit value of -3 (bit pattern 11101), the
library needs to return it as an `int64_t` with value -3.  The raw 5
bits give unsigned value 29.  To get -3, the library performs **sign
extension**: it copies the sign bit (bit 4) into all the upper bits.

```
5-bit value:     11101                              (unsigned 29)
                 ^sign bit = 1

Sign-extend to 64 bits:
    11111111 11111111 11111111 11111111
    11111111 11111111 11111111 11111101   = 0xFFFFFFFFFFFFFFFD

As int64_t: -3  ✓
```

For a positive value like 7 (bit pattern 00111), the sign bit is 0, so
the upper bits are all zeros -- which is what you'd get naturally:

```
5-bit value:     00111                              (unsigned 7)
                 ^sign bit = 0

Zero-extend to 64 bits:
    00000000 00000000 00000000 00000000
    00000000 00000000 00000000 00000111   = 0x0000000000000007

As int64_t: 7  ✓
```

Sign extension preserves the numeric value when moving from a narrow
representation to a wider one.

---

## 8. Writing and Reading Signed Values

### Writing

```c
tp_bs_write_bits_signed(w, -3, 5);    // writes 11101 (5 bits)
tp_bs_write_bits_signed(w,  7, 5);    // writes 00111 (5 bits)
tp_bs_write_bits_signed(w, -1, 5);    // writes 11111 (5 bits)
```

The write function truncates the `int64_t` to the low n bits.  For -3:
- `int64_t` -3 = 0xFFFFFFFFFFFFFFFD
- Mask to 5 bits: 0xFFFFFFFFFFFFFFFD & 0x1F = 0x1D = 11101

This is the correct 5-bit two's complement representation of -3.

### Reading

```c
int64_t val;
tp_bs_read_bits_signed(r, 5, &val);   // reads 11101 → val = -3
tp_bs_read_bits_signed(r, 5, &val);   // reads 00111 → val =  7
tp_bs_read_bits_signed(r, 5, &val);   // reads 11111 → val = -1
```

The read function extracts n bits, checks the MSB, and sign-extends to
`int64_t`.

### The Same Bits, Two Interpretations

The bit pattern 11101 (5 bits) can be read as:
- **Unsigned**: `tp_bs_read_bits(r, 5, &u)` gives u = 29
- **Signed**: `tp_bs_read_bits_signed(r, 5, &s)` gives s = -3

The bits in the stream are identical.  Only the read function determines
the interpretation.  This is the same as how `(uint8_t)0xFF` is 255 but
`(int8_t)0xFF` is -1 -- same byte, different type.

---

## 9. Complete Worked Example

Here's a realistic scenario: packing a small struct into a bit stream.

Suppose you have a sensor reading with:
- `channel`: 0-7 (3 bits unsigned)
- `temperature`: -40 to +85 degrees (8 bits signed, range -128..127)
- `humidity`: 0-100% (7 bits unsigned)
- `alert`: on/off (1 bit)

Total: 3 + 8 + 7 + 1 = 19 bits (vs. 4 bytes = 32 bits if byte-aligned).

```c
/* ── Write ── */
tp_bitstream_writer *w = NULL;
tp_bs_writer_create(&w, 256, 0);

tp_bs_write_bits(w, 5, 3);            // channel 5
tp_bs_write_bits_signed(w, -12, 8);   // temperature -12°C
tp_bs_write_bits(w, 67, 7);           // humidity 67%
tp_bs_write_bits(w, 1, 1);            // alert ON

// 19 bits written = 2 bytes + 3 bits
// Byte 0: [1 0 1  1 1 1 1 0]  = channel(101) + temp high 5 bits(11110)
// Byte 1: [1 0 0  1 0 0 0 0]  = temp low 3 bits(100) + humidity(1000011)
// Byte 2: [1 1  . . . . . .]  = humidity last bit(1) + alert(1)

/* ── Read ── */
tp_bitstream_reader *r = NULL;
tp_bs_writer_to_reader(w, &r);

uint64_t channel, humidity, alert;
int64_t temperature;

tp_bs_read_bits(r, 3, &channel);             // 5
tp_bs_read_bits_signed(r, 8, &temperature);  // -12
tp_bs_read_bits(r, 7, &humidity);            // 67
tp_bs_read_bits(r, 1, &alert);              // 1

tp_bs_reader_destroy(&r);
tp_bs_writer_destroy(&w);
```

19 bits instead of 32 -- a 40% savings per reading.  Multiply by
thousands of readings and the savings are significant, especially on
embedded devices with limited flash.

---

## 10. VarInt: Variable-Width Integers

Fixed-width fields are great when the range is known.  But for values
with unpredictable range (like string lengths, key counts, or skip
distances), TriePack uses **VarInt** encoding.

A VarInt uses 1 to 10 bytes.  Each byte contributes 7 data bits and
1 continuation bit:

```
Byte layout:  [C  d6 d5 d4 d3 d2 d1 d0]
               ^                        ^
               continuation bit         data bits

C = 1: more bytes follow
C = 0: this is the last byte
```

Data bits from earlier bytes are less significant (LSB first).

### Example: Encoding 300

```
300 in binary: 100101100

Step 1: Take low 7 bits → 0101100 = 0x2C
        Remaining: 300 >> 7 = 2 (non-zero, so set C=1)
        Byte 0: [1  0101100] = 0xAC

Step 2: Take low 7 bits of 2 → 0000010 = 0x02
        Remaining: 2 >> 7 = 0 (last byte, C=0)
        Byte 1: [0  0000010] = 0x02

Result: 0xAC 0x02 (2 bytes for value 300)
```

### Signed VarInt (Zigzag)

Negative numbers as two's complement have many leading 1 bits, which
would waste bytes.  **Zigzag encoding** solves this by interleaving
positive and negative values:

```
Original → Zigzag → VarInt bytes
   0     →   0    → 0x00 (1 byte)
  -1     →   1    → 0x01 (1 byte)
   1     →   2    → 0x02 (1 byte)
  -2     →   3    → 0x03 (1 byte)
   2     →   4    → 0x04 (1 byte)
  -64    →  127   → 0x7F (1 byte)
   64    →  128   → 0x80 0x01 (2 bytes)
```

Small magnitudes (positive or negative) always take few bytes.

---

## 11. Portability: 32-Bit Processors

The bitstream API uses `uint64_t` and `int64_t` for values and cursor
positions.  On 32-bit processors (i386, 68000, MIPS32, ESP32/Xtensa,
ARM32), the C compiler implements these using pairs of 32-bit registers.

For strict C99 portability, TriePack provides a fallback typedef in
`triepack_common.h`: if the platform's `<stdint.h>` does not define
`uint64_t` (which is technically optional in C99), the library defines
it as `unsigned long long` (which C99 guarantees is at least 64 bits).

### Width Parameter Type

All bit-width parameters (`n`, `bps`) use `unsigned int` rather than
`uint8_t`.  This avoids unnecessary narrowing instructions on 32-bit
targets -- the C ABI passes `unsigned int` in a single register with
no masking, while `uint8_t` may require an extra `AND 0xFF` instruction
at each call site.

```c
// Good: no narrowing needed, value arrives as-is in a register
tp_result tp_bs_write_bits(tp_bitstream_writer *w, uint64_t value, unsigned int n);

// The function validates n internally: n must be 1..64
```

### 32-Bit Fast Path

For fields known to fit in 32 bits, `tp_bs_read_bits32()` avoids the
64-bit intermediate entirely:

```c
uint32_t sym;
tp_bs_read_bits32(r, 5, &sym);   // no uint64_t involved
```

This is the recommended path for trie symbol reads on 32-bit embedded
targets.

---

## 12. Relationship to TriePack

The bitstream library is the foundation of the entire TriePack stack.
Every operation in the encoder and decoder -- writing symbols, skip
pointers, value tags, string lengths -- is a call to `tp_bs_write_bits`
or `tp_bs_read_bits` at the appropriate width.

```
tp_bs_write_bits(w, symbol_code, bps)     → trie symbol
tp_bs_write_bits(w, CTRL_BRANCH, bps)     → control code
tp_bs_write_varint_u(w, child_count)      → branch metadata
tp_bs_write_varint_u(w, skip_distance)    → skip pointer
tp_bs_write_bits(w, type_tag, 4)          → value type
tp_bs_write_varint_s(w, int_value)        → signed integer value
tp_bs_write_bits(w, float_bits, 32)       → IEEE 754 float
```

The trie itself is a flat sequence of these packed fields.  The decoder
reads them back one at a time, using the same widths, to navigate the
trie and extract values.

---

## See Also

- [Bitstream Specification](../internals/bitstream-spec.md) -- byte-level
  format details, VarInt encoding tables, buffer management API
- [API Reference](api-reference.md) -- every function with usage examples
- [How It Works](how-it-works.md) -- how the trie encoder uses the
  bitstream to compress keys
- [fr_math](https://github.com/deftio/fr_math) -- a related library by
  the same author that performs fixed-point arithmetic at arbitrary
  fractional bit widths
