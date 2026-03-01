---
layout: default
title: API Reference
---

# API Reference

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This is a practical guide to every public function in TriePack.
For auto-generated docs with parameter types, build with `BUILD_DOCS=ON`.

---

## Error Handling

Every C function returns `tp_result`. Check for `TP_OK` (0) on success.
Use `tp_result_str()` to get a human-readable error message:

```c
tp_result rc = tp_dict_open(&dict, buf, len);
if (rc != TP_OK) {
    fprintf(stderr, "open failed: %s\n", tp_result_str(rc));
    return 1;
}
```

| Code                      | Value | Meaning                              |
|---------------------------|-------|--------------------------------------|
| `TP_OK`                   | 0     | Success                              |
| `TP_ERR_EOF`              | -1    | Read past end of stream              |
| `TP_ERR_ALLOC`            | -2    | Memory allocation failed             |
| `TP_ERR_INVALID_PARAM`    | -3    | NULL pointer or invalid argument     |
| `TP_ERR_INVALID_POSITION` | -4    | Seek beyond bounds                   |
| `TP_ERR_NOT_ALIGNED`      | -5    | Byte operation on non-aligned cursor |
| `TP_ERR_OVERFLOW`         | -6    | VarInt exceeds max groups            |
| `TP_ERR_INVALID_UTF8`     | -7    | Malformed UTF-8                      |
| `TP_ERR_BAD_MAGIC`        | -10   | Not a .trp file                      |
| `TP_ERR_VERSION`          | -11   | Unsupported format version           |
| `TP_ERR_CORRUPT`          | -12   | Integrity check failed               |
| `TP_ERR_NOT_FOUND`        | -13   | Key not in dictionary                |
| `TP_ERR_TRUNCATED`        | -14   | Data shorter than header claims      |
| `TP_ERR_JSON_SYNTAX`      | -20   | Malformed JSON                       |
| `TP_ERR_JSON_DEPTH`       | -21   | Nesting exceeds limit                |
| `TP_ERR_JSON_TYPE`        | -22   | Type mismatch on path lookup         |

---

## Values (`triepack/triepack_common.h`)

### Value Types

TriePack values are tagged unions. The `type` field tells you which union
member to read:

```c
tp_value val;
tp_dict_lookup(dict, "key", &val);

switch (val.type) {
case TP_NULL:    /* no data */                              break;
case TP_BOOL:    printf("%s", val.data.bool_val ? "true" : "false"); break;
case TP_INT:     printf("%lld", (long long)val.data.int_val);        break;
case TP_UINT:    printf("%llu", (unsigned long long)val.data.uint_val); break;
case TP_FLOAT32: printf("%f", (double)val.data.float32_val);         break;
case TP_FLOAT64: printf("%f", val.data.float64_val);                 break;
case TP_STRING:  printf("%.*s", (int)val.data.string_val.str_len,
                                val.data.string_val.str);            break;
case TP_BLOB:    /* val.data.blob_val.data, val.data.blob_val.len */ break;
default: break;
}
```

### Construction Helpers

Use these to build `tp_value` structs for insertion into an encoder:

```c
tp_value tp_value_null(void);
```
Creates a null value. Useful as a sentinel or "key exists with no data."

```c
tp_value tp_value_bool(bool val);
```
Creates a boolean value. Encoded as a single bit (1 = true, 0 = false).

```c
tp_value tp_value_int(int64_t val);
```
Creates a signed 64-bit integer. Encoded as a zigzag VarInt -- small
magnitudes (positive or negative) use fewer bytes.

```c
tp_value tp_value_uint(uint64_t val);
```
Creates an unsigned 64-bit integer. Encoded as an unsigned VarInt.

```c
tp_value tp_value_float32(float val);
```
Creates a 32-bit IEEE 754 float. Always takes 4 bytes in the encoded output.

```c
tp_value tp_value_float64(double val);
```
Creates a 64-bit IEEE 754 double. Always takes 8 bytes in the encoded output.

```c
tp_value tp_value_string(const char *str);
```
Creates a string value from a NUL-terminated C string. The string is
copied into the encoded output. On decode, the string pointer points
directly into the `.trp` buffer (zero-copy).

```c
tp_value tp_value_string_n(const char *str, size_t len);
```
Creates a string value with explicit length. Can contain embedded NUL bytes.

```c
tp_value tp_value_blob(const uint8_t *data, size_t len);
```
Creates a binary blob value. Like string but for arbitrary bytes.

**Example -- inserting values into an encoder:**

```c
tp_value v;

v = tp_value_int(42);
tp_encoder_add(enc, "count", &v);

v = tp_value_string("hello world");
tp_encoder_add(enc, "greeting", &v);

v = tp_value_bool(true);
tp_encoder_add(enc, "active", &v);

/* Key-only entry (set membership, no value) */
tp_encoder_add(enc, "flag", NULL);
```

---

## Encoder (`triepack/triepack.h`)

The encoder collects key-value pairs and builds a compressed `.trp` blob.

### Create an Encoder

```c
tp_result tp_encoder_create(tp_encoder **out);
```
Allocates a new encoder with default options. Pass a pointer to your
`tp_encoder *` variable. Returns `TP_ERR_ALLOC` if allocation fails.

```c
tp_encoder *enc = NULL;
tp_result rc = tp_encoder_create(&enc);
```

For custom options (addressing mode, checksum type, manual bits-per-symbol):

```c
tp_result tp_encoder_create_ex(tp_encoder **out, const tp_encoder_options *opts);
```

```c
tp_encoder_options opts = tp_encoder_defaults();
opts.bits_per_symbol = 6;  /* force 6-bit symbols */

tp_encoder *enc = NULL;
tp_encoder_create_ex(&enc, &opts);
```

### Encoder Options

```c
tp_encoder_options tp_encoder_defaults(void);
```
Returns the default configuration:
- `trie_mode = TP_ADDR_BIT` (bit-level symbol packing)
- `value_mode = TP_ADDR_BYTE` (byte-aligned values)
- `checksum = TP_CHECKSUM_CRC32`
- `bits_per_symbol = 0` (auto-detect from key alphabet)

### Add Entries

```c
tp_result tp_encoder_add(tp_encoder *enc, const char *key, const tp_value *val);
```
Adds a NUL-terminated key with an optional value. Pass `NULL` for `val`
to add a key-only entry (set membership).

```c
tp_result tp_encoder_add_n(tp_encoder *enc, const char *key, size_t key_len,
                            const tp_value *val);
```
Adds a key with explicit length. Use when keys may contain NUL bytes.

```c
uint32_t tp_encoder_count(const tp_encoder *enc);
```
Returns how many entries have been added.

**Duplicate keys:** If you add the same key twice, the last value wins.

### Build the Trie

```c
tp_result tp_encoder_build(tp_encoder *enc, uint8_t **buf, size_t *len);
```
Sorts entries, builds the compressed trie, and returns a heap-allocated
buffer. **The caller must `free()` the buffer when done.**

```c
uint8_t *buf = NULL;
size_t len = 0;
tp_result rc = tp_encoder_build(enc, &buf, &len);
if (rc == TP_OK) {
    /* buf contains len bytes of .trp data */
    /* ... use buf ... */
    free(buf);
}
```

### Reset and Destroy

```c
tp_result tp_encoder_reset(tp_encoder *enc);
```
Clears all entries but keeps the encoder options. Reuse the encoder
without re-allocating.

```c
tp_result tp_encoder_destroy(tp_encoder **enc);
```
Frees the encoder and sets `*enc = NULL`.

---

## Dictionary (`triepack/triepack.h`)

The dictionary reader provides read-only access to a compiled `.trp` blob.

### Open a Dictionary

```c
tp_result tp_dict_open(tp_dict **out, const uint8_t *buf, size_t len);
```
Opens a `.trp` buffer for reading. Validates magic bytes, format version,
and CRC-32 checksum. Returns `TP_ERR_CORRUPT` if the checksum fails.

The caller must keep `buf` alive for the lifetime of the dictionary --
the dict reads directly from it (zero-copy).

```c
tp_dict *dict = NULL;
tp_result rc = tp_dict_open(&dict, buf, len);
if (rc != TP_OK) {
    fprintf(stderr, "open: %s\n", tp_result_str(rc));
}
```

```c
tp_result tp_dict_open_unchecked(tp_dict **out, const uint8_t *buf, size_t len);
```
Opens without CRC-32 verification. Use for trusted data (e.g., ROM) or
when you need maximum open speed.

```c
tp_result tp_dict_close(tp_dict **dict);
```
Closes the dictionary and sets `*dict = NULL`.

### Look Up Keys

```c
tp_result tp_dict_lookup(const tp_dict *dict, const char *key, tp_value *val);
```
Looks up a NUL-terminated key. On success, fills `val` with the decoded
value. Returns `TP_ERR_NOT_FOUND` if the key doesn't exist.

**String values are zero-copy:** `val.data.string_val.str` points directly
into the `.trp` buffer. Do not free it.

```c
tp_value val;
if (tp_dict_lookup(dict, "name", &val) == TP_OK) {
    if (val.type == TP_STRING)
        printf("name = %.*s\n", (int)val.data.string_val.str_len,
               val.data.string_val.str);
}
```

```c
tp_result tp_dict_lookup_n(const tp_dict *dict, const char *key, size_t key_len,
                            tp_value *val);
```
Look up a key with explicit length. Use when keys may contain NUL bytes.

```c
tp_result tp_dict_contains(const tp_dict *dict, const char *key, bool *out);
```
Checks whether a key exists without decoding its value. Faster than
`tp_dict_lookup()` when you only need membership testing.

```c
bool found = false;
tp_dict_contains(dict, "flag", &found);
if (found) { /* key exists */ }
```

### Dictionary Info

```c
uint32_t tp_dict_count(const tp_dict *dict);
```
Returns the number of keys stored in the dictionary.

```c
tp_result tp_dict_get_info(const tp_dict *dict, tp_dict_info *info);
```
Retrieves metadata about the dictionary: format version, key count,
addressing modes, checksum type, bits-per-symbol, and total size.

```c
tp_dict_info info;
tp_dict_get_info(dict, &info);
printf("Format: v%u.%u\n", info.format_version_major, info.format_version_minor);
printf("Keys:   %u\n", info.num_keys);
printf("BPS:    %u\n", info.bits_per_symbol);
printf("Size:   %zu bytes\n", info.total_bytes);
```

The `tp_dict_info` struct:

| Field                   | Type             | Description                     |
|-------------------------|------------------|---------------------------------|
| `format_version_major`  | `uint8_t`        | Major version (must be 1)       |
| `format_version_minor`  | `uint8_t`        | Minor version                   |
| `num_keys`              | `uint32_t`       | Number of keys                  |
| `trie_mode`             | `tp_addr_mode`   | Trie addressing mode            |
| `value_mode`            | `tp_addr_mode`   | Value store addressing mode     |
| `checksum_type`         | `tp_checksum_type` | Checksum algorithm            |
| `bits_per_symbol`       | `uint8_t`        | Bits per trie symbol            |
| `has_values`            | `bool`           | Whether values are stored       |
| `has_suffix_table`      | `bool`           | Whether suffix table is present |
| `compact_mode`          | `bool`           | Whether compact mode is used    |
| `total_bytes`           | `size_t`         | Total encoded size in bytes     |

---

## Iterator (`triepack/triepack.h`)

Iterators traverse dictionary entries without allocating result arrays.

```c
tp_result tp_dict_iterate(const tp_dict *dict, tp_iterator **out);
```
Creates an iterator over all keys in the dictionary. Entries are returned
in lexicographic order.

```c
tp_result tp_iter_next(tp_iterator *it, const char **key, size_t *key_len,
                        tp_value *val);
```
Advances to the next entry. Returns `TP_ERR_EOF` when done. The `key`
pointer is valid until the next call to `tp_iter_next()` or
`tp_iter_destroy()`.

```c
tp_result tp_iter_reset(tp_iterator *it);
```
Resets the iterator to the beginning.

```c
tp_result tp_iter_destroy(tp_iterator **it);
```
Destroys the iterator and sets `*it = NULL`.

**Example -- iterating all entries:**

```c
tp_iterator *it = NULL;
tp_dict_iterate(dict, &it);

const char *key;
size_t key_len;
tp_value val;

while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
    printf("%.*s\n", (int)key_len, key);
}

tp_iter_destroy(&it);
```

---

## Search (`triepack/triepack.h`)

### Prefix Search

```c
tp_result tp_dict_find_prefix(const tp_dict *dict, const char *prefix,
                               tp_iterator **out);
```
Returns an iterator over all keys that start with `prefix`. Results are
in lexicographic order. Use `tp_iter_next()` to consume.

### Fuzzy Search

```c
tp_result tp_dict_find_fuzzy(const tp_dict *dict, const char *query,
                              uint8_t max_dist, tp_iterator **out);
```
Returns an iterator over all keys within Levenshtein edit distance
`max_dist` of `query`. Recommended: `max_dist <= 2` for performance.

```c
tp_result tp_iter_get_distance(const tp_iterator *it, uint8_t *dist);
```
Gets the edit distance for the current iterator position. Only valid
for iterators created by `tp_dict_find_fuzzy()`.

---

## Bitstream (`triepack/triepack_bitstream.h`)

Low-level arbitrary-width bit field I/O. You rarely need this directly --
the encoder and dictionary use it internally -- but it's available for
custom binary formats or direct trie manipulation.

### 32-Bit Platform Portability

The bitstream API uses `uint64_t` and `int64_t` for values, positions, and
stream lengths. C99 makes these exact-width types optional -- they are only
required when the platform has a native 64-bit integer type. TriePack
provides a fallback `typedef` from `unsigned long long` / `long long`
(guaranteed by C99 to be at least 64 bits) so the library compiles on
32-bit targets (i386, 68000, MIPS32, ESP32/Xtensa, ARM32) even if their
`<stdint.h>` does not define `uint64_t` directly.

For narrow reads (up to 32 bits), use `tp_bs_read_bits32()` to avoid the
`uint64_t` intermediate entirely.

### Reader Lifecycle

```c
tp_result tp_bs_reader_create(tp_bitstream_reader **out,
                               const uint8_t *buf, uint64_t bit_len);
```
Creates a reader over an existing buffer. Zero-copy -- the reader reads
directly from `buf`, which must remain valid. `bit_len` is the number of
valid bits (not bytes).

```c
uint8_t data[] = {0xAB, 0xCD};
tp_bitstream_reader *r = NULL;
tp_bs_reader_create(&r, data, 16);  /* 16 bits = 2 bytes */
```

```c
tp_result tp_bs_reader_create_copy(tp_bitstream_reader **out,
                                    const uint8_t *buf, uint64_t bit_len);
```
Creates a reader that copies the buffer into owned memory. Use when `buf`
may be freed before you're done reading.

```c
tp_result tp_bs_reader_destroy(tp_bitstream_reader **reader);
```
Destroys the reader and sets `*reader = NULL`.

```c
tp_result tp_bs_reader_set_bit_order(tp_bitstream_reader *r, tp_bit_order order);
```
Sets MSB-first (default) or LSB-first bit ordering for subsequent reads.

### Writer Lifecycle

```c
tp_result tp_bs_writer_create(tp_bitstream_writer **out,
                               size_t initial_cap, size_t growth);
```
Creates a growable writer. `initial_cap` is the initial buffer size in
bytes (0 = use default 256). `growth` is the growth increment in bytes
(0 = double on each realloc).

```c
tp_bitstream_writer *w = NULL;
tp_bs_writer_create(&w, 1024, 0);  /* 1KB initial, doubling growth */
```

```c
tp_result tp_bs_writer_destroy(tp_bitstream_writer **writer);
```
Destroys the writer and frees its buffer.

### Bit-Level I/O

```c
tp_result tp_bs_write_bits(tp_bitstream_writer *w, uint64_t value, uint8_t n);
```
Writes the low `n` bits (1-64) of `value` to the stream, MSB first.

```c
tp_bs_write_bits(w, 0x15, 5);  /* writes 10101 (5 bits) */
tp_bs_write_bits(w, 0x03, 3);  /* writes 011 (3 bits) */
/* stream now contains: 10101011 = 0xAB */
```

```c
tp_result tp_bs_read_bits(tp_bitstream_reader *r, uint8_t n, uint64_t *out);
```
Reads `n` bits (1-64) as an unsigned value.

```c
uint64_t val;
tp_bs_read_bits(r, 5, &val);  /* reads 5 bits → val */
```

```c
tp_result tp_bs_write_bits_signed(tp_bitstream_writer *w, int64_t value, uint8_t n);
```
Writes the low `n` bits of a signed value. The value is truncated to `n`
bits in two's complement form.

```c
tp_result tp_bs_read_bits_signed(tp_bitstream_reader *r, uint8_t n, int64_t *out);
```
Reads `n` bits as a sign-extended value. The MSB of the `n`-bit field is
treated as the sign bit and extended to fill the `int64_t`.

**Example -- signed bit field round-trip (like a 5-bit signed integer):**

```c
tp_bitstream_writer *w = NULL;
tp_bs_writer_create(&w, 256, 0);

/* Write three signed values as 5-bit fields (range: -16 to +15) */
tp_bs_write_bits_signed(w, -3, 5);   /* stores 11101 */
tp_bs_write_bits_signed(w,  7, 5);   /* stores 00111 */
tp_bs_write_bits_signed(w, -1, 5);   /* stores 11111 */

/* Read them back -- sign extension restores the original values */
tp_bitstream_reader *r = NULL;
tp_bs_writer_to_reader(w, &r);

int64_t val;
tp_bs_read_bits_signed(r, 5, &val);  /* val == -3  */
tp_bs_read_bits_signed(r, 5, &val);  /* val ==  7  */
tp_bs_read_bits_signed(r, 5, &val);  /* val == -1  */

tp_bs_reader_destroy(&r);
tp_bs_writer_destroy(&w);
```

This works exactly like reading a narrow signed field from a hardware
register or binary protocol -- the MSB is the sign bit, and the library
extends it to the full `int64_t` width on read.

```c
tp_result tp_bs_read_bit(tp_bitstream_reader *r, uint8_t *out);
tp_result tp_bs_write_bit(tp_bitstream_writer *w, uint8_t value);
```
Read/write a single bit.

```c
tp_result tp_bs_peek_bits(tp_bitstream_reader *r, uint8_t n, uint64_t *out);
```
Reads `n` bits without advancing the cursor. Useful for lookahead.

```c
tp_result tp_bs_read_bits32(tp_bitstream_reader *r, uint8_t n, uint32_t *out);
```
Reads up to 32 bits into a `uint32_t`. On 32-bit processors (i386, 68k,
MIPS32, ESP32) this avoids 64-bit register-pair emulation. Ideal for
reading trie symbols, control codes, and other fields that fit in 32 bits.

### Byte-Level I/O

These require the cursor to be byte-aligned. They read/write in
big-endian (network) byte order.

```c
tp_result tp_bs_read_u8(tp_bitstream_reader *r, uint8_t *out);
tp_result tp_bs_read_u16(tp_bitstream_reader *r, uint16_t *out);
tp_result tp_bs_read_u32(tp_bitstream_reader *r, uint32_t *out);
tp_result tp_bs_read_u64(tp_bitstream_reader *r, uint64_t *out);
tp_result tp_bs_read_bytes(tp_bitstream_reader *r, uint8_t *buf, size_t n);
```

```c
tp_result tp_bs_write_u8(tp_bitstream_writer *w, uint8_t val);
tp_result tp_bs_write_u16(tp_bitstream_writer *w, uint16_t val);
tp_result tp_bs_write_u32(tp_bitstream_writer *w, uint32_t val);
tp_result tp_bs_write_u64(tp_bitstream_writer *w, uint64_t val);
tp_result tp_bs_write_bytes(tp_bitstream_writer *w, const uint8_t *buf, size_t n);
```

**Example -- writing a binary header:**

```c
tp_bs_write_u32(w, 0x54525000);  /* magic "TRP\0" */
tp_bs_write_u8(w, 1);            /* version major */
tp_bs_write_u8(w, 0);            /* version minor */
tp_bs_write_u16(w, flags);       /* flags */
```

### VarInt

Variable-length integer encoding. Small values use fewer bytes.

```c
tp_result tp_bs_write_varint_u(tp_bitstream_writer *w, uint64_t value);
tp_result tp_bs_read_varint_u(tp_bitstream_reader *r, uint64_t *out);
```
Unsigned VarInt (LEB128): 7 data bits + 1 continuation bit per byte.
Values 0-127 take 1 byte, 128-16383 take 2 bytes, etc.

```c
tp_result tp_bs_write_varint_s(tp_bitstream_writer *w, int64_t value);
tp_result tp_bs_read_varint_s(tp_bitstream_reader *r, int64_t *out);
```
Signed VarInt: zigzag-encodes the value first, then LEB128. This makes
small negative numbers cheap (e.g., -1 takes 1 byte, not 10).

**Example -- VarInt round-trip:**

```c
tp_bs_write_varint_u(w, 300);
tp_bs_write_varint_s(w, -42);

/* ... create reader from writer ... */

uint64_t u;
tp_bs_read_varint_u(r, &u);   /* u == 300 */

int64_t s;
tp_bs_read_varint_s(r, &s);   /* s == -42 */
```

### Symbols and UTF-8

```c
tp_result tp_bs_write_symbol(tp_bitstream_writer *w, uint32_t val, uint8_t bps);
tp_result tp_bs_read_symbol(tp_bitstream_reader *r, uint8_t bps, uint32_t *out);
```
Write/read a fixed-width symbol of `bps` bits (1-32). Used internally
for trie symbols where `bps` is determined by the alphabet size.

```c
tp_result tp_bs_write_utf8(tp_bitstream_writer *w, uint32_t codepoint);
tp_result tp_bs_read_utf8(tp_bitstream_reader *r, uint32_t *out);
```
Write/read a Unicode codepoint in standard UTF-8 encoding (1-4 bytes).
The stream must be byte-aligned. Rejects overlong encodings and
surrogates.

### Cursor Operations

```c
uint64_t tp_bs_reader_position(const tp_bitstream_reader *r);
```
Returns the current read position in bits.

```c
uint64_t tp_bs_reader_remaining(const tp_bitstream_reader *r);
```
Returns the number of bits remaining from the cursor to the end.

```c
uint64_t tp_bs_reader_length(const tp_bitstream_reader *r);
```
Returns the total stream length in bits.

```c
tp_result tp_bs_reader_seek(tp_bitstream_reader *r, uint64_t bit_pos);
```
Seeks to an absolute bit position. Returns `TP_ERR_INVALID_POSITION` if
out of range.

```c
tp_result tp_bs_reader_advance(tp_bitstream_reader *r, uint64_t n);
```
Advances the cursor by `n` bits.

```c
tp_result tp_bs_reader_align_to_byte(tp_bitstream_reader *r);
```
Advances to the next byte boundary. No-op if already aligned.

```c
bool tp_bs_reader_is_byte_aligned(const tp_bitstream_reader *r);
```
Returns true if the cursor is on a byte boundary.

```c
uint64_t tp_bs_writer_position(const tp_bitstream_writer *w);
tp_result tp_bs_writer_align_to_byte(tp_bitstream_writer *w);
```
Writer equivalents for position and byte alignment.

### Stateless ROM Functions

These operate on raw byte pointers with no reader object. No allocation,
no state. Safe for interrupt handlers and bare-metal.

```c
tp_result tp_bs_read_bits_at(const uint8_t *buf, uint64_t bit_pos,
                              uint8_t n, uint64_t *out);
```
Reads `n` bits at the given bit offset from `buf`.

```c
tp_result tp_bs_read_bits_signed_at(const uint8_t *buf, uint64_t bit_pos,
                                     uint8_t n, int64_t *out);
```
Reads `n` bits as a sign-extended value at the given bit offset.

```c
tp_result tp_bs_read_varint_u_at(const uint8_t *buf, uint64_t bit_pos,
                                  uint64_t *out, uint8_t *bits_read);
```
Reads a VarInt at the given bit offset. `bits_read` receives the number
of bits consumed, so you can advance your own cursor.

**Example -- reading from a ROM buffer:**

```c
static const uint8_t rom_data[] = { /* ... */ };
uint64_t val;
tp_bs_read_bits_at(rom_data, 256, 5, &val);  /* read 5 bits at bit 256 */
```

### Buffer Management

```c
tp_result tp_bs_writer_get_buffer(const tp_bitstream_writer *w,
                                   const uint8_t **buf, uint64_t *bit_len);
```
Gets a read-only view of the writer's buffer. The pointer is valid until
the next write or destroy.

```c
tp_result tp_bs_writer_detach_buffer(tp_bitstream_writer *w,
                                      uint8_t **buf, size_t *byte_len,
                                      uint64_t *bit_len);
```
Transfers buffer ownership to the caller. The writer becomes empty.
**The caller must `free()` the buffer.**

```c
tp_result tp_bs_writer_to_reader(tp_bitstream_writer *w,
                                  tp_bitstream_reader **reader);
```
Creates a reader over the writer's contents (copies the data). Useful
for round-trip testing.

```c
tp_result tp_bs_reader_get_buffer(const tp_bitstream_reader *r,
                                   const uint8_t **buf, uint64_t *bit_len);
```
Gets a read-only view of the reader's underlying buffer.

```c
tp_result tp_bs_reader_direct_ptr(tp_bitstream_reader *r,
                                   const uint8_t **ptr, size_t n);
```
Returns a pointer directly into the reader's buffer at the current
position. Requires byte alignment. Advances the cursor by `n * 8` bits.
Used for zero-copy string/blob access.

```c
tp_result tp_bs_writer_append_buffer(tp_bitstream_writer *w,
                                      const uint8_t *buf, uint64_t bit_len);
```
Appends raw bits from an external buffer.

```c
tp_result tp_bs_copy_bits(tp_bitstream_reader *r, tp_bitstream_writer *w,
                           uint64_t n_bits);
```
Copies `n_bits` from reader to writer.

---

## JSON (`triepack/triepack_json.h`)

The JSON library encodes JSON documents into `.trp` format using
flattened dot-path keys. JSON objects become dictionaries with keys like
`"address.city"` and `"items[0].name"`.

### One-Shot Encode

```c
tp_result tp_json_encode(const char *json_str, size_t json_len,
                          uint8_t **buf, size_t *buf_len);
```
Parses a JSON string and encodes it as a `.trp` blob. The output buffer
is heap-allocated -- **caller must `free()` it.**

```c
const char *json = "{\"name\":\"Alice\",\"age\":30}";
uint8_t *buf = NULL;
size_t buf_len = 0;
tp_result rc = tp_json_encode(json, strlen(json), &buf, &buf_len);
if (rc == TP_OK) {
    /* buf contains buf_len bytes of .trp data */
    free(buf);
}
```

### One-Shot Decode

```c
tp_result tp_json_decode(const uint8_t *buf, size_t buf_len,
                          char **json_str, size_t *json_len);
```
Decodes a `.trp` blob back to a compact JSON string. **Caller must
`free()` the returned string.**

```c
tp_result tp_json_decode_pretty(const uint8_t *buf, size_t buf_len,
                                 const char *indent,
                                 char **json_str, size_t *json_len);
```
Decodes to a pretty-printed JSON string. `indent` sets the per-level
indentation (e.g., `"  "` for 2 spaces, `"\t"` for tabs).

```c
char *pretty = NULL;
size_t plen = 0;
tp_json_decode_pretty(buf, buf_len, "  ", &pretty, &plen);
printf("%.*s\n", (int)plen, pretty);
free(pretty);
```

### DOM-Style Access

```c
tp_result tp_json_open(tp_json **out, const uint8_t *buf, size_t buf_len);
```
Opens a `.trp` buffer for DOM-style path lookups. Copies the buffer
internally. Call `tp_json_close()` when done.

```c
tp_result tp_json_close(tp_json **json);
```
Closes the JSON handle and sets `*json = NULL`.

```c
tp_result tp_json_lookup_path(const tp_json *j, const char *path, tp_value *val);
```
Looks up a value by dot/bracket path. Paths match the flattened key
format used internally.

```c
tp_json *j = NULL;
tp_json_open(&j, buf, buf_len);

tp_value val;
tp_json_lookup_path(j, "name", &val);
/* val.type == TP_STRING, val.data.string_val.str == "Alice" */

tp_json_lookup_path(j, "items[0].price", &val);
/* val.type == TP_INT or TP_FLOAT64, depending on the JSON number */

tp_json_close(&j);
```

```c
tp_result tp_json_root_type(const tp_json *j, tp_value_type *type);
```
Returns `TP_DICT` if the root is a JSON object, `TP_ARRAY` if it's an array.

```c
uint32_t tp_json_count(const tp_json *j);
```
Returns the number of top-level keys (object) or elements (array).

```c
tp_result tp_json_iterate(const tp_json *j, tp_iterator **out);
```
Returns an iterator over the top-level entries.

---

## C++ Wrappers (`triepack::` namespace)

RAII wrappers around the C API. Include `<triepack/triepack.hpp>` for
core wrappers and `<triepack/bitstream.hpp>` for bitstream wrappers.
All wrappers are non-copyable and movable.

### `triepack::Encoder`

```cpp
#include <triepack/triepack.hpp>

triepack::Encoder enc;
enc.insert("apple", 42);
enc.insert("banana", 17);

const uint8_t *data;
size_t size;
int rc = enc.encode(&data, &size);
// data points to the encoded .trp blob
// data is valid until enc is destroyed or encode() is called again
```

| Method | Description |
|--------|-------------|
| `Encoder()` | Construct with default options |
| `void insert(const char *key, int32_t value)` | Add a key-value pair |
| `int encode(const uint8_t **data, size_t *size)` | Build the trie, receive pointer to blob |
| `tp_encoder *handle()` | Access the underlying C handle |

### `triepack::Dict`

```cpp
triepack::Dict dict(data, size);

int32_t val;
if (dict.lookup("apple", &val)) {
    // val == 42
}

size_t n = dict.size();  // number of keys
```

| Method | Description |
|--------|-------------|
| `Dict(const uint8_t *data, size_t size)` | Open a `.trp` blob |
| `bool lookup(const char *key, int32_t *val)` | Look up key, return true if found |
| `size_t size()` | Number of entries |
| `tp_dict *handle()` | Access the underlying C handle |

### `triepack::Iterator`

```cpp
triepack::Iterator it(dict);
while (it.next()) {
    printf("%s -> %d\n", it.key(), it.value());
}
```

| Method | Description |
|--------|-------------|
| `Iterator(const Dict &dict)` | Create iterator over dict |
| `bool next()` | Advance; returns false when done |
| `const char *key()` | Current key |
| `int32_t value()` | Current value |

### Move Semantics

All wrappers support move construction and move assignment:

```cpp
triepack::Encoder enc1;
enc1.insert("key", 1);

triepack::Encoder enc2 = std::move(enc1);
// enc1 is now empty (handle() == nullptr)
// enc2 owns the encoder
```
