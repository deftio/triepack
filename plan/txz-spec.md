# TXZ Specification

Consolidated from original notes (2001-2018) by Manu Chatterjee.
Distilled and modernized 2026-02.

---

## 1. Overview

TXZ is a compact binary format for storing string-keyed dictionaries using compressed prefix and suffix tries. It is implemented as a **C library with C++ wrappers**, following the lineage of the original 2001 C implementation.

The format supports **multiple addressing granularities** -- bit, byte, and symbol (e.g. UTF-8 code point) -- selectable per-section via the header. Keys and values are stored as **two separate trie structures** with optionally shared symbol tables. This separation keeps the core TXZ library focused on compressed string storage, while higher-level libraries (e.g. JSON support) can layer on top using the same header format.

### 1.1 Design Goals

| Goal | Description |
|------|-------------|
| **Compact storage** | Trie with prefix sharing, suffix deduplication, and variable-width symbol encoding. Bit-packing mode eliminates byte-alignment waste; byte/symbol modes trade some density for simpler decoding |
| **Multi-granularity** | Selectable addressing: bit (maximum density), byte (simpler decoding, natural for values), or symbol (UTF-8 code points, fixed-width character sets) |
| **ROM-able** | The binary format can live in read-only memory (flash, ROM, embedded) with no load-time fixups -- all offsets are relative or computable |
| **Fast lookup** | Skip-pointer navigation allows O(key-length) lookups without decompressing the entire trie |
| **Decoder-first** | The format is defined by its decoder grammar; encoders may vary in sophistication |
| **Two-tree architecture** | Keys in one trie, values in a separate trie (or value store). Optionally shared symbol tables between the two. Keeps key compression orthogonal to value storage. |
| **Structured values** | Values may be typed: null, bool, integers, floats, strings, byte blobs, arrays, or nested TXZ dictionaries |
| **Versioned and verifiable** | Header carries version, metadata, and integrity check |
| **C library, 100% test coverage** | Core implementation in C (with C++ wrappers). Embedded-grade: every code path tested. |

### 1.2 Historical Context

The original TXZ (2001-2003) stored word lists for spelling correction and word completion using a C implementation with a C++ wrapper. The bit-stream foundation was the `dio_BitFile` library (`dev/bitfile.h`), which provided arbitrary-width bit field read/write (e.g. pack a 5-bit symbol, a 12-bit jump offset, a 7-bit value into a continuous bit stream, then read them back as native integers).

Key ideas carried forward:

- **Two-trie architecture**: The prefix trie stored unique stems; a separate suffix trie stored common endings. Encoded bit-offset jumps in the prefix trie pointed into the suffix trie to complete keys. This is TXZ's core compression mechanism beyond simple prefix sharing.
- **Fixed-width terminal values**: At each terminal node (after resolving stem + suffix), an N-bit integer was stored directly in the bit stream. The bit width was fixed for the entire dictionary (declared in the header). Used for word frequencies, dictionary indices, or addresses. No type tag -- just raw bits read via `GetNBitQty()`.
- **VBEDict (2002)**: Variable Binary Encoded Dictionary -- trie flattened with push/pop operators, then converted to skip-pointer form for random access. Per-symbol bit-width chosen at build time to minimize size.
- **Suffix morphs (2003)**: Common suffixes (e.g. "ing", "ies") extracted into a separate suffix trie and referenced by index, enabling cross-branch deduplication.
- **Flag-enc-num / VarInt**: Variable-length unsigned integer encoding where the high bit of each byte signals continuation (0 = last byte). Encodes 0-127 in 1 byte, up to 16383 in 2 bytes, etc.
- **Compression sections (2018)**: Ideas for RLE, LFSR sequences, LZW sections, and patterned-RLE as additional encoding primitives within the data stream.

---

## 2. Concepts

### 2.1 Prefix Trie

Keys sharing a common prefix are stored once. The trie is flattened into a linear stream where branching is handled by skip-pointers. Depending on the addressing mode, symbols may be bit-packed at arbitrary positions (bit mode), byte-aligned (byte mode), or packed/aligned as code points (symbol mode).

```
Example keys: wonder, wondering, womb, wombat, world, worry, worrying, worries

Prefix tree:
w-o+n-d-e-r[*]
   |       +i-n-g[*]
   +m-b[*]
   |   +a-t[*]
   +r+l-d[*]
     +r+y[*]
       | +i-n-g[*]
       +i-e-s[*]

[*] = terminal (key endpoint, may carry a value)
```

### 2.2 Suffix Trie and Stem-Suffix Jumps

In the original TXZ, the prefix trie and suffix trie were **two separate structures**. The encoder identified common suffixes across the key set and built a dedicated suffix trie. The prefix trie then stored only the unique **stems** (the prefix portion up to the point where a shared suffix begins). At that point, the prefix trie contained an **encoded jump** -- a bit-offset pointer into the suffix trie -- to complete the key.

```
Original architecture (two separate structures with cross-jumps):

PREFIX TRIE (stems):                   SUFFIX TRIE:
w-o+n-d-e-r--->JUMP(suffix 0)           0: "ing"
   |       +--->JUMP(suffix 0)           1: "ies"
   +m-b--->END                           2: "ed"
   |   +-a-t--->END                      3: "ying"
   +r+l-d--->END
     +r+y--->END
       | +--->JUMP(suffix 0)
       +-i--->JUMP(suffix 1)

Lookup "worrying":
  1. Walk prefix trie: w -> o -> r -> r -> y
  2. Hit JUMP to suffix trie entry 0 ("ing")
  3. Compare remaining key "ing" against suffix "ing" -> match
  4. Read value at terminal

Lookup "worries":
  1. Walk prefix trie: w -> o -> r -> r
  2. At branch, take 'i' child
  3. Hit JUMP to suffix trie entry 1 ("ies")
  4. Compare remaining key "ies" against suffix "ies" -> match
```

The suffix trie itself could also be a compressed trie (sharing structure among suffixes like "ing", "ings", "ingly"), or a flat list for small suffix sets. In either case the jumps from prefix to suffix were encoded as bit-offset pointers, making them fast to follow but tricky to construct.

**TXZ 1.0** preserves this two-structure model. The prefix trie uses the SUFFIX control code followed by a VarInt index into the suffix table. The changes from the original are formalizing the jump encoding and replacing the fixed-width terminal value with the typed value system (Section 2.3).

### 2.3 Values

The original TXZ stored only a fixed-width N-bit integer at each terminal (e.g. word frequency as a 12-bit field). Version 1.0 replaces this with a full typed value system. Every terminal node may optionally carry a value:

| Type tag | Type | Description |
|----------|------|-------------|
| 0x00 | null | Key exists, no value (key-only / set member) |
| 0x01 | bool | Single byte: 0 = false, 1 = true |
| 0x02 | int | Signed integer, VarInt encoded |
| 0x03 | uint | Unsigned integer, VarInt encoded |
| 0x04 | float32 | IEEE 754 single, 4 bytes |
| 0x05 | float64 | IEEE 754 double, 8 bytes |
| 0x06 | string | VarInt length + UTF-8 bytes |
| 0x07 | blob | VarInt length + raw bytes |
| 0x08 | array | VarInt count + sequence of typed values |
| 0x09 | dict | Embedded TXZ sub-dictionary (nested) |
| 0x0A-0x0F | reserved | Future use |

This allows TXZ to represent structures like:

```json
{
  "server": {
    "host": "example.com",
    "port": 8080,
    "tls": true,
    "aliases": ["ex.com", "example.net"]
  }
}
```

as a nested TXZ where the outer trie has key "server" whose value is itself a TXZ dict containing keys "host", "port", "tls", "aliases".

### 2.4 Symbol Encoding

TXZ 1.0 supports three symbol encoding strategies, selectable per dictionary. The encoder analyzes the key set at build time and picks the strategy that minimizes total size.

#### 2.4.1 Fixed-Width (Original)

All symbols are the same number of bits. Simple to decode (just read N bits), easy skip-pointer math.

| bits_per_symbol | Fits | Example use |
|-----------------|------|-------------|
| 5 | 32 symbols (26 letters + 6 control codes) | English word lists |
| 6 | 64 symbols (alphanumeric + controls) | Identifiers, URLs |
| 7 | 128 symbols (ASCII) | General ASCII text |
| 8 | 256 symbols (full byte) | Binary keys, UTF-8 bytes |

The symbol table in the header maps N-bit codes to actual character values. The encoder reorders symbols so that control codes occupy codes that minimize escaping.

#### 2.4.2 Huffman-Coded Symbols

For large tries where symbol frequency is skewed (common in natural language -- 'e' is far more frequent than 'z'), the encoder can build a **Huffman tree over the symbol alphabet** and store it in the header. Each symbol is then a variable number of bits, with frequent symbols using fewer bits.

```
Example: English lowercase + controls (32 symbols)
  Fixed-width: every symbol = 5 bits
  Huffman: 'e' = 2 bits, 't' = 3 bits, ... 'z' = 7 bits, ESCAPE = 8 bits
  Savings: ~15-25% on large English word lists
```

**How it works:**

- The Huffman tree is stored in the Trie Config section (see Section 3.3.1).
- The trie data is a bit stream of variable-width Huffman codes (always bit-mode addressing).
- The decoder reads bits and walks the Huffman tree to resolve each symbol before interpreting it (regular symbol, control code, etc.).
- Skip distances must be in **bits** (not symbol-widths, since symbols are variable-width).

**Trade-offs:**

| | Fixed-width | Huffman |
|---|-------------|---------|
| Decode speed | Fast (read N bits, done) | Slower (walk Huffman tree per symbol) |
| Trie size | Larger if frequency is skewed | Smaller (frequent symbols cheaper) |
| Skip pointers | Can use symbol-width units | Must use bit units |
| Header overhead | Small (just symbol table) | Larger (Huffman tree stored in header) |
| Best for | Small key sets, uniform distribution, ROM where decode speed matters | Large key sets (10K+ keys), skewed frequency, size is priority |

The Huffman tree is compact: for N symbols it's at most N-1 internal nodes. Stored as a canonical Huffman code (just the code lengths per symbol, from which the tree is reconstructable), this adds only ~N bytes to the header.

#### 2.4.3 Multi-Width Zones (Hybrid)

A middle ground: partition the symbol alphabet into 2-3 frequency tiers, each with a different fixed width, using a short prefix code to select the tier.

```
Example: 32 symbols partitioned into 3 tiers
  Tier 0 (prefix 0):    8 most frequent symbols   = 1 + 3 = 4 bits each
  Tier 1 (prefix 10):  16 medium symbols           = 2 + 4 = 6 bits each
  Tier 2 (prefix 11):   8 rare symbols + controls  = 2 + 3 = 5 bits each
```

This is simpler to decode than full Huffman (just read the prefix bits, then read the tier's fixed width) while still giving most of the size benefit. The tier boundaries and prefix codes are declared in the header.

**Comparison to Huffman:** Multi-width is ~90% of Huffman's compression with ~50% of the decode complexity. Good default for ROM targets where decode speed matters but you still want better-than-fixed-width.

#### 2.4.4 Encoder Choice

The encoding strategy is an **encoder-side decision** that doesn't affect format compatibility. The header declares which strategy is in use; any decoder must support all three. A simple encoder can always use fixed-width. A sophisticated encoder analyzes symbol frequencies and picks the best strategy (or tries all three and keeps the smallest).

The `bits_per_symbol` field in Trie Config has these meanings:

| bits_per_symbol value | Interpretation |
|----------------------|----------------|
| 1-15 | Fixed-width: all symbols are this many bits |
| 0 | UTF-8 symbol mode (variable-width, byte-aligned) |

When Huffman or multi-width encoding is used, `bits_per_symbol` is set to a fixed-width value that serves as the **maximum** symbol width (for header space calculations), and a flag in Trie Config indicates the actual encoding strategy. See Section 3.3.1.

### 2.5 Skip Pointers

The flattened trie uses skip-pointer operators to enable efficient navigation without full decompression:

- At each branch point, a **skip distance** indicates how far to jump to reach the next sibling.
- The skip distance is VarInt encoded (8-bit groups packed at the current bit position).
- The `skip_unit` flag determines the unit:
  - **bits** (skip_unit=0): skip distance is in raw bits. Most general.
  - **symbol-widths** (skip_unit=1): skip distance is in multiples of `bits_per_symbol`. More compact when all elements are symbol-width-aligned (which they are in tries without inline VarInts between symbols). E.g. at 5 bits/symbol, a skip of 3 means jump 15 bits.

This is what makes TXZ ROM-able: a reader can navigate down the trie by following skip pointers directly on the ROM-mapped data, without allocating memory or decompressing the full structure.

### 2.6 Addressing Modes

Different parts of TXZ benefit from different addressing granularities. The header declares the addressing mode for the trie and value store independently:

| Mode | Cursor unit | Best for | Trade-off |
|------|-------------|----------|-----------|
| **bit** | 1 bit | Maximum density. Trie symbols at arbitrary bit widths (e.g. 5-bit). Original TXZ mode. | Requires bit-shift/mask operations for every read. Skip distances and offsets are in bits. |
| **byte** | 8 bits | Value store, byte-oriented data, simpler decoding on modern CPUs. | Wastes fractional bits per symbol if symbol width doesn't divide 8. Skip distances and offsets are in bytes. |
| **symbol** | Variable (1-4 bytes for UTF-8, or fixed N-bit) | UTF-8 keys where each trie node is a Unicode code point. Also works for fixed-width character sets. | Symbols are variable-width (UTF-8) or declared-width. Skip distances are in symbol counts. |

**How modes interact:**

- The **trie** uses one mode (declared in Trie Config). This determines how symbols are packed and how skip distances are measured.
- The **value store** uses one mode (declared in Trie Config). Typically byte-mode for natural alignment with strings, blobs, and fixed-width numbers.
- **VarInts** are always 8-bit groups regardless of addressing mode. In bit-mode, they start at arbitrary bit positions. In byte-mode, they start at byte boundaries. In symbol-mode, they are emitted as whole bytes (each VarInt byte occupies one cursor step if the symbol width is 8, otherwise they switch to byte-mode temporarily).

**Symbol mode details:**

In symbol mode, the trie cursor advances by one symbol at a time. Symbols can be:
- **Fixed-width** (e.g. 8-bit ASCII, 16-bit UCS-2): `bits_per_symbol` declares the width.
- **UTF-8 code points**: `bits_per_symbol` is set to a special value (0) indicating variable-width UTF-8 encoding. Each symbol is 1-4 bytes, decoded per standard UTF-8 rules. The symbol table maps code-point ranges rather than individual codes.

This means the same TXZ library can compactly store:
- English word lists (5-bit symbols, bit-addressed trie)
- UTF-8 internationalized keys (symbol-addressed trie, code-point granularity)
- Binary config data (byte-addressed trie, byte-addressed values)

---

## 3. Binary Format

### 3.0 Stream Model

After the byte-aligned File Header (which bootstraps the reader), all subsequent data flows through a **unified stream** backed by an underlying byte buffer. The stream library (see `plan/bitstream-spec.md`) provides a cursor that tracks position at bit granularity internally but exposes read/write operations at the addressing mode declared for each section.

| Trie addressing mode | Cursor advances by | Offsets measured in |
|----------------------|--------------------|---------------------|
| bit | 1 bit per symbol-bit | bits |
| byte | 8 bits | bytes |
| symbol (fixed) | bits_per_symbol bits | symbols |
| symbol (UTF-8) | 8-32 bits (1-4 bytes per code point) | code points |

The stream always tracks an internal **bit position** so that mode transitions (e.g. reading a VarInt from within a symbol-mode trie) are exact and lossless. This bit-level foundation is what preserves ROM-ability across all modes.

In **bit mode** (original TXZ behavior), symbols, control codes, VarInts, skip distances, and value references are packed at arbitrary bit positions with no padding. In **byte mode**, each element starts on the next byte boundary. In **symbol mode**, each element starts on the next symbol boundary.

### 3.1 Overall Layout

```
+------------------------+  byte-aligned (bootstrap)
| File Header            |
+------------------------+  <-- data stream begins here
| Trie Config            |  \
+- - - - - - - - - - - - +   |  post-header metadata
| Huffman Sym Trie       |   |  (optional, large objects)
+------------------------+   |
| Symbol Table           |   |  (optionally shared between
+------------------------+   |   key trie and value trie)
| Key Suffix Trie        |   |
+------------------------+   |  continuous data stream
| Key Prefix Trie        |   |
+------------------------+   |
| Value Trie / Store     |   |
+------------------------+  /
+------------------------+  byte-aligned (fixed footer)
| Integrity Check        |
+------------------------+
```

The key trie consists of two parts: the **Key Prefix Trie** (stems) and the **Key Suffix Trie** (common endings with encoded jumps between them). The **Value Trie / Store** is a separate structure holding typed values, referenced by index from the key trie's terminal nodes.

When the symbol table is shared (header flag), both key and value tries use the same symbol-to-character mapping -- useful when keys and values draw from the same character set. When not shared, the value trie has its own symbol config.

The File Header is byte-aligned so it can be read without a bit-reader. Everything between the header and the integrity footer is a packed data stream. The integrity footer is byte-aligned (padded to the next byte boundary after the stream ends).

### 3.2 File Header (fixed size: 32 bytes, byte-aligned)

The header is the only byte-aligned structure. It bootstraps the bit-stream reader.

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | Magic bytes: `TXZ\x00` (0x54 0x58 0x5A 0x00) |
| 4 | 1 | version_major | Format major version |
| 5 | 1 | version_minor | Format minor version |
| 6 | 2 | flags | Bit flags (see below) |
| 8 | 4 | num_keys | Total number of keys |
| 12 | 4 | trie_data_offset | Bit offset (from start of data stream) to Prefix Trie Data |
| 16 | 4 | value_store_offset | Bit offset to Value Store |
| 20 | 4 | suffix_table_offset | Bit offset to Suffix Table (0 if none) |
| 24 | 4 | total_data_bits | Total length of the data stream in bits |
| 28 | 4 | reserved | Reserved, must be 0 |

The data stream begins immediately after the header (byte offset 32). All `*_offset` fields are **bit offsets** relative to this starting point, regardless of the section's addressing mode. The reader converts bit offsets to byte/symbol positions as needed based on the declared mode. Using bit offsets universally ensures the header format is mode-independent.

**Flags (bit field):**

| Bit | Meaning |
|-----|---------|
| 0 | has_values: keys carry values |
| 1 | has_suffix_table: suffix table present |
| 2 | has_nested_dicts: at least one value is type dict |
| 3 | compact_mode: trie was optimized for size over speed |
| 4-5 | trie_addr_mode: 00=bit, 01=byte, 10=symbol-fixed, 11=symbol-utf8 |
| 6-7 | value_addr_mode: 00=bit, 01=byte, 10=symbol-fixed, 11=reserved |
| 8-9 | symbol_encoding: 00=fixed-width, 01=huffman, 10=multi-width, 11=reserved |
| 10 | shared_symbols: key trie and value trie share the same symbol table |
| 11-15 | reserved |

### 3.3 Trie Config

Starts at the beginning of the data stream (immediately after the byte-aligned header). The Trie Config itself is always packed in **bit mode** regardless of the trie addressing mode -- it bootstraps the mode.

| Field | Width | Description |
|-------|-------|-------------|
| bits_per_symbol | 4 bits | Bits per trie symbol (1-15; typically 5-8). Set to 0 for UTF-8 symbol mode (variable-width). For Huffman/multi-width encoding, this is the max symbol code width. |
| symbol_count | 8 bits | Number of symbols in alphabet (includes control codes). For UTF-8 mode, this is the number of control code entries only. |
| special_symbol_map | 6 x bits_per_symbol | Maps the 6 control codes to their symbol values (see below). For UTF-8 mode, 6 x 8 bits (control codes are byte-valued). |

After reading Trie Config, the reader checks the `symbol_encoding` flag. If Huffman or multi-width, the Huffman symbol trie follows immediately (Section 3.3.1). Then the reader switches to the trie_addr_mode declared in the header flags.

Note: `special_symbol_map` width depends on `bits_per_symbol` -- e.g. at 5 bits/symbol, the map is 30 bits.

**Control codes** (from original design, encoded as symbols within the trie stream):

| Code | Meaning | Original notation |
|------|---------|-------------------|
| END | Terminal node, no value | `.` |
| END_VAL | Terminal node, value follows | `!` |
| SKIP | Skip pointer, VarInt distance follows | `^` |
| SUFFIX | Suffix reference, VarInt index follows | `:` |
| ESCAPE | Next symbol is literal (escaping a control code) | `\` |
| BRANCH | Branch point marker | `(` |

The `special_symbol_map` lets the encoder choose which symbol codes represent these controls, minimizing the need for escaping in a given dataset.

### 3.3.1 Huffman Symbol Trie (optional, post-header metadata)

Present only when header flag `symbol_encoding` is 01 (Huffman) or 10 (multi-width). Stored in the data stream immediately after the Trie Config and special_symbol_map, before the Symbol Table. This is part of the post-header metadata that bootstraps the decoder before any trie data is read.

**Why it's only in the metadata, not per-node:** The Huffman trie is a property of the whole TXZ object, declared once. For small TXZ objects (few keys, short keys), the overhead of storing a Huffman tree exceeds the savings -- the encoder should just use fixed-width. The Huffman option pays off on **larger objects** (thousands of keys or more) where the cumulative savings across the trie data outweigh the one-time tree cost.

**Huffman encoding (symbol_encoding = 01):**

Stored as canonical Huffman code lengths -- the most compact representation:

| Field | Width | Description |
|-------|-------|-------------|
| max_code_length | 4 bits | Maximum Huffman code length in bits (typically 8-15) |
| code_lengths | symbol_count x 4 bits | Huffman code length for each symbol (0 = symbol not used). Symbols in symbol-table order. |

From the code lengths, the decoder reconstructs the canonical Huffman tree using the standard algorithm (sort by length, assign codes left-to-right). This is the same approach used by DEFLATE/zlib -- well understood and compact.

Total overhead: 4 + (symbol_count x 4) bits. For 32 symbols: 132 bits = ~17 bytes. This is negligible for any trie with more than a few hundred keys.

```
Example: 32 symbols, code lengths stored as 4-bit values

Decoder reconstructs:
  code_length 2: 'e' -> 00
  code_length 3: 't' -> 010, 'a' -> 011
  code_length 4: 'o' -> 1000, 'i' -> 1001, 'n' -> 1010, 's' -> 1011
  ...
  code_length 7: 'z' -> 1111110, ESCAPE -> 1111111

Reading from trie data: consume bits, walk Huffman tree to resolve symbol.
```

**Multi-width encoding (symbol_encoding = 10):**

| Field | Width | Description |
|-------|-------|-------------|
| num_tiers | 2 bits | Number of tiers (2-3) |
| tier_prefix[] | num_tiers x VarInt | Prefix code for each tier (variable length, stored as bit-length + bits) |
| tier_width[] | num_tiers x 4 bits | Fixed symbol width within each tier |
| tier_symbols[] | num_tiers x VarInt | Number of symbols in each tier |

Decoding: read prefix bits to determine tier, then read tier_width bits for the symbol code.

**Impact on skip pointers:** When symbols are variable-width (Huffman or multi-width), skip pointers **must** be in bits (skip_unit is forced to 0). The encoder sets this automatically.

**Impact on control codes:** Control codes are Huffman-coded just like regular symbols. The encoder can assign them longer codes (since they're less frequent than common letters) to save bits on the frequent symbols.

### 3.4 Symbol Table

A mapping from N-bit symbol codes to output characters, packed in the bit stream immediately after Trie Config.

| Field | Width | Description |
|-------|-------|-------------|
| entries | symbol_count x VarInt | Each entry is the Unicode code point (or byte value) for that symbol index. VarInts are packed at arbitrary bit positions. |

For a 5-bit encoding with 26 lowercase letters + 6 control codes = 32 entries. The 6 control code positions (from `special_symbol_map`) have no entry in the symbol table -- they are skipped.

### 3.5 Suffix Table (optional)

Present when flag bit 1 is set. Packed in the bit stream after the Symbol Table.

| Field | Width | Description |
|-------|-------|-------------|
| num_suffixes | VarInt | Number of suffix entries |
| suffixes[] | variable | Each suffix: VarInt length (in symbols) + length x bits_per_symbol packed symbols |

Suffixes are referenced by 0-based index from the prefix trie using the SUFFIX control code.

For large suffix sets, the suffix table may itself be a TXZ sub-trie (indicated by a flag).

### 3.6 Prefix Trie Data

The main trie, encoded as a packed stream of symbols and control codes at the declared `trie_addr_mode` granularity.

**Encoding by mode:**

- **Bit mode**: Symbols are `bits_per_symbol`-wide bit fields packed with no padding. A symbol at bit 17 is followed by the next at bit 17+bits_per_symbol. VarInts are 8-bit groups starting at the current bit position (not byte-aligned).
- **Byte mode**: Each symbol occupies one byte (bits_per_symbol must be <= 8; unused high bits zero-padded). VarInts start on byte boundaries. Skip distances are in bytes.
- **Symbol-fixed mode**: Each symbol occupies `bits_per_symbol` bits, but the cursor advances in symbol units. Equivalent to bit mode in storage, but skip distances are counted in symbols rather than bits.
- **Symbol-UTF8 mode**: Each symbol is a UTF-8 encoded code point (1-4 bytes). Control codes are single-byte values chosen from unused byte ranges (declared in `special_symbol_map`). Skip distances are in code-point counts.

**Encoding rules (all modes):**

1. Symbols are emitted at the mode's natural width.
2. At a branch point, BRANCH is emitted followed by a VarInt child count, then for each child except the last, a SKIP distance (VarInt, in the mode's addressing unit) to the next sibling.
3. Terminal nodes emit END (no value) or END_VAL (value reference follows as a VarInt offset, in the value store's addressing unit, into the Value Store).
4. Suffix references emit SUFFIX followed by a VarInt index into the suffix table.
5. ESCAPE before a symbol means "this is a literal, not a control code."
6. VarInts are always 8-bit groups. In bit mode they start at arbitrary bit positions. In byte and symbol modes they start at the next byte boundary.

**Walk algorithm (lookup):**

The walk demonstrates the stem-to-suffix jump that is central to TXZ. The reader walks the prefix trie (matching the stem), hits a SUFFIX code, jumps to the suffix trie to match the remainder, then reads the terminal value.

```
function lookup(prefix_trie, suffix_trie, key):
    pos = 0           // bit position in prefix_trie
    key_idx = 0       // position in search key
    while key_idx < len(key):
        sym = read_symbol(prefix_trie, pos)
        if sym == BRANCH:
            child_count = read_varint(prefix_trie, pos)
            // scan children for matching next symbol
            for each child (with skip distances):
                peek next symbol of child
                if matches key[key_idx]:
                    descend into this child
                    break
                else:
                    skip to next sibling using SKIP distance
            if no match: return NOT_FOUND
        elif sym == END:
            if key_idx == len(key): return FOUND_NO_VALUE
            else: return NOT_FOUND
        elif sym == END_VAL:
            if key_idx == len(key):
                return read_value(pos)  // VarInt offset into Value Store
            else: return NOT_FOUND
        elif sym == SUFFIX:
            // ** STEM-TO-SUFFIX JUMP **
            // This is the core cross-trie reference from the original design.
            suffix_idx = read_varint(prefix_trie, pos)
            remaining_key = key[key_idx:]
            suffix = suffix_trie[suffix_idx]
            if remaining_key starts with suffix:
                key_idx += len(suffix)
                // After suffix match, the terminal value follows in the
                // prefix trie stream (right after the SUFFIX + index)
                continue
            else: return NOT_FOUND
        elif sym == ESCAPE:
            literal = read_symbol(prefix_trie, pos)
            if literal != key[key_idx]: return NOT_FOUND
            key_idx++
        else:
            // regular symbol -- advance through the stem
            if sym != key[key_idx]: return NOT_FOUND
            key_idx++
```

### 3.7 Value Trie / Store

Values are stored in a **separate structure** from the key trie. The key trie's END_VAL terminals reference values by index into this section. Two forms are supported:

- **Value store** (simple): a sequential array of typed values, referenced by index. Fast to build, no compression of values against each other.
- **Value trie** (compressed): string values that share prefixes/suffixes are stored in their own compressed trie (same format as the key trie). The `txz-json` library uses this when it detects significant sharing among string values. Non-string values (ints, floats, bools, blobs) remain in a flat store section.

When the header flag `shared_symbols` is set, the value trie shares the symbol table with the key trie. Otherwise, the value trie has its own symbol config (allowing different character sets or bit widths for keys vs. values).

Packing follows the `value_addr_mode` declared in the header flags.

Each value is encoded as:

```
type_tag (4 bits) + type-specific payload (bit-packed)
```

The type tag is 4 bits (supporting types 0x0-0xF from Section 2.3). Payloads:

| Type | Payload |
|------|---------|
| null | (none -- 0 additional bits) |
| bool | 1 bit: 0=false, 1=true |
| int | VarInt (zig-zag encoded signed) |
| uint | VarInt (unsigned) |
| float32 | 32 bits (IEEE 754, packed at current bit position) |
| float64 | 64 bits (IEEE 754, packed at current bit position) |
| string | VarInt byte-length + length x 8 bits (UTF-8 bytes, bit-packed) |
| blob | VarInt byte-length + length x 8 bits (raw bytes, bit-packed) |
| array | VarInt count + count x (type_tag + payload) |
| dict | Embedded TXZ sub-dictionary (complete bit stream with its own trie config) |

For type `dict`, the payload is a nested TXZ bit stream (recursive structure) prefixed by a VarInt bit-length so the reader can skip over it.

### 3.8 Integrity Check (byte-aligned footer)

The bit stream is padded with zero bits to the next byte boundary, then the integrity footer is appended as a byte-aligned structure.

| Field | Size | Description |
|-------|------|-------------|
| checksum_type | 1 byte | 0 = CRC-32, 1 = SHA-256, 2 = xxHash64 |
| checksum | 4-32 bytes | Checksum value |
| padding | to 32 bytes | Zero-padded to fixed 32-byte footer |

The checksum covers all bytes from byte 0 (start of File Header) through the last byte of the zero-padded bit stream, before the integrity footer. This allows validation without parsing the trie.

---

## 4. VarInt Encoding

Carried forward from the original "Flag-enc-num" design. Used throughout for lengths, offsets, counts, and integer values.

VarInts are always composed of 8-bit groups (7 data + 1 continuation), regardless of addressing mode. Their alignment depends on context:

- **In bit-mode sections**: VarInts start at the current bit position and are not byte-aligned. A VarInt starting at bit 13 reads bits 13-20, then 21-28, etc.
- **In byte-mode sections**: VarInts start at byte boundaries (they are conventional byte-aligned VarInts).
- **In symbol-mode sections**: VarInts temporarily switch to byte-mode reads at the current cursor position (byte-aligned if the symbol width is 8; bit-aligned otherwise for sub-byte symbols).

```
Encoding (unsigned):
  Each 8-bit group: [continuation_bit (1) | data_bits (7)]
  continuation_bit: 1 = more groups follow, 0 = this is the last group
  Groups are big-endian (most significant group first)

  Examples:
    0          -> 0b_0_0000000                    (8 bits,   7 data bits)
    127        -> 0b_0_1111111                    (8 bits,   7 data bits)
    128        -> 0b_1_0000001  0b_0_0000000      (16 bits, 14 data bits)
    16383      -> 0b_1_1111111  0b_0_1111111      (16 bits, 14 data bits)

Encoding (signed):
  Zig-zag encode first: (n << 1) ^ (n >> 63), then encode as unsigned VarInt.
```

VarInts always consume a multiple of 8 bits. In bit mode their starting position can be any bit offset; in byte/symbol modes they start byte-aligned.

---

## 5. ROM-ability Constraints

To keep the binary usable directly from ROM/flash without loading into RAM:

1. **No pointers** -- all references are relative offsets (in the section's addressing unit) or indices.
2. **No load-time fixups** -- the binary is usable as-is from any base address.
3. **Direct traversal** -- the reader maintains a cursor (bit-position internally) and extracts symbols using the section's addressing mode. In bit mode, this requires shift/mask operations. In byte/symbol modes, reads are naturally aligned. All modes work directly on ROM-mapped memory with no copying.
4. **Read-only traversal** -- the lookup algorithm needs only a position cursor and a small stack (bounded by max key length) for branch tracking.
5. **No allocator required** -- decoder state fits in a small fixed struct on the stack (cursor + small branch stack).
6. **Mode-appropriate alignment** -- in bit mode, no alignment assumptions. In byte mode, byte boundaries are guaranteed. In symbol-UTF8 mode, code point boundaries are guaranteed. The file header and integrity footer are always byte-aligned regardless of mode.

---

## 6. Compaction Notes

The encoder may apply several optimizations. These are encoder-side choices and do not affect the decoder:

| Technique | Description |
|-----------|-------------|
| **Suffix extraction** | Identify common suffixes across all keys and extract to suffix table. Trade-off: suffix indirection cost vs. saved bytes. |
| **Symbol frequency analysis** | Choose `bits_per_symbol` and symbol ordering to minimize total trie size. For large objects, build a Huffman symbol trie (Section 2.4.2) -- stored once in post-header metadata, amortized across all trie symbols. Multi-width zones (Section 2.4.3) as a simpler alternative. |
| **Branch ordering** | Order children at branch points by frequency (most-accessed first) to improve average lookup time (speculative, does not affect correctness). |
| **Trie minimization** | Merge identical subtrees (DAWG-style). Two branches with identical remaining structure share storage. |
| **Value deduplication** | Identical values stored once in the Value Store, referenced by multiple terminal nodes. |
| **Compact mode** | Flag bit 3. Encoder chose maximal compression over lookup speed (e.g. deeper suffix extraction, smaller skip-pointer units). |

---

## 7. Mutation / Editing

TXZ is optimized for **read** performance. Editing a compressed trie is possible but understood to be slow -- it generally requires rebuilding part or all of the structure.

### 7.1 Mutation Model

| Operation | Cost | Approach |
|-----------|------|----------|
| **Insert key** | Expensive | Decompress the affected trie region, insert into an in-memory trie, re-encode. May invalidate skip pointers, suffix indices, and Huffman codes throughout. |
| **Delete key** | Expensive | Same as insert -- structural change requires re-encoding. |
| **Update value** | Moderate | If the new value is the same size in bits, can overwrite in-place (only in RAM, not ROM). If different size, requires re-encoding from that point forward. |
| **Bulk rebuild** | Expected path | Decode full TXZ to in-memory key-value structure, apply mutations, re-encode from scratch. This is the intended workflow. |

### 7.2 Practical Workflow

```
[Source data]  --(encoder)-->  [TXZ binary]  --(read/query)-->  [results]
      ^                                              |
      |                                              |
      +----(decode, mutate, re-encode)---------------+
```

The encoder is already the expensive path (Section 1.1: "decoder-first" design). Mutation is just: decode + edit in memory + re-encode. This is the same model as SQLite's "build index offline" or protobuf's "deserialize, modify, reserialize."

### 7.3 Future: Incremental Update Optimization

If mutation performance becomes important, possible optimizations (not in 1.0 scope):

- **Append journal**: append new/changed entries to the end of the file. Reader checks journal first, falls back to main trie. Periodic compaction merges journal into trie.
- **Segmented tries**: split large trie into segments that can be rebuilt independently. A top-level routing trie directs to segments by key prefix.
- **Copy-on-write nodes**: for RAM-resident tries, share unchanged subtrees between old and new versions.

For 1.0: just rebuild. Keep it simple.

---

## 8. JSON/DOM Support (Separate Library)

JSON support is **not part of the core TXZ library**. It is a separate library (`txz-json` or similar) that uses TXZ as its storage backend. This separation keeps the core library small, focused, and suitable for embedded use where JSON may not be needed.

### 8.1 Relationship

```
+------------------+
|   txz-json       |  JSON encode/decode, DOM API
+------------------+
        |  uses
+------------------+
|   txz            |  Core: compressed trie, bit-stream, key/value storage
+------------------+
        |  uses
+------------------+
|   txz-bitstream  |  Foundation: bit/byte/symbol read/write
+------------------+
```

Both `txz` and `txz-json` share the same **header format**. A TXZ file produced by `txz-json` is a valid TXZ file -- the core library can read its key and value tries. The JSON library adds the DOM-level interpretation (nesting, arrays, type mapping).

### 8.2 JSON Type Mapping

| JSON type | TXZ storage |
|-----------|-------------|
| `{}` object | Key trie (object keys) + value trie/store (values). Recursive for nesting. |
| `[]` array | Value type `array` -- ordered sequence of typed values in the value store |
| `"string"` | Value type `string` -- if many string values share prefixes, the value trie compresses them |
| `number` (int) | Value type `int` or `uint` |
| `number` (float) | Value type `float64` |
| `true/false` | Value type `bool` |
| `null` | Value type `null` |

### 8.3 Why a Separate Value Trie

In a JSON document, values often share structure just like keys do:

```json
{
  "server_host": "api.example.com",
  "server_backup": "api.example.com",
  "cdn_host": "cdn.example.com",
  "cdn_backup": "cdn.example.com",
  "status_ok": "operational",
  "status_db": "operational",
  "status_cache": "operational"
}
```

Keys share prefixes ("server_", "cdn_", "status_"). Values *also* share prefixes and even full duplicates ("api.example.com", "operational"). With keys and values as two separate tries:

- **Key trie**: compresses "server_", "cdn_", "status_" prefixes
- **Value trie**: compresses "api.example.com", "cdn.example.com" (shared ".example.com" suffix), deduplicates "operational"
- **Shared symbol table** (optional): if keys and values use the same character set (e.g. both ASCII), a single symbol table covers both tries, saving header space

The terminal nodes in the key trie store **indices into the value trie** (or value store for non-string values). This is the same stem-to-suffix jump mechanism used within the key trie itself, just across the key/value boundary.

### 8.4 Example

```json
{
  "app": "myapp",
  "version": 3,
  "features": {
    "autocomplete": true,
    "spellcheck": true
  },
  "words": ["wonder", "womb", "world", "worry"]
}
```

```
Key trie:                    Value trie / store:
  a-p-p -----> val[0]         [0] string "myapp"
  f-e-a-t...  -> val[1]       [1] nested TXZ {autocomplete:true, spellcheck:true}
  v-e-r-s...  -> val[2]       [2] uint 3
  w-o-r-d-s   -> val[3]       [3] array [string "wonder", "womb", "world", "worry"]
                                         (array strings share "wo" prefix in value trie)
```

String arrays in the value store can themselves be compressed as a sub-trie when it makes sense (many strings with shared structure). The `txz-json` library decides when this is worthwhile.

---

## 8. What Changed from the Original

This is version 1.0 -- a clean break. No backward compatibility with the original (2001-2003) binary format. The table below summarizes what was kept, what was changed, and what was added.

| Aspect | Original (2001-2003) | TXZ 1.0 | Status |
|--------|----------------------|---------|--------|
| Two-trie architecture | Prefix trie + suffix trie with bit-offset jumps | Same, formalized as SUFFIX control code + VarInt index | **Kept** |
| Bit-stream foundation | `dio_BitFile`: arbitrary-width bit fields to/from integers | Same core concept, extended to 64-bit, multi-mode | **Kept + extended** |
| Skip pointers | VarInt skip distances in bits | Same, plus symbol-width unit option | **Kept + extended** |
| Variable-width symbols | Fixed bits_per_symbol, chosen at build time | Same, plus symbol remap table in header | **Kept + extended** |
| ROM-able | No pointers, no fixups, no allocator on read path | Same | **Kept** |
| Addressing | Bit-only | Selectable: bit, byte, symbol-fixed, symbol-UTF8 | **New** |
| Keys | Word strings (English-centric) | Any UTF-8 string | **Changed** |
| Values | Fixed-width N-bit integer per terminal, one width for whole dict | Full typed value system: null, bool, int, float, string, blob, array, nested dict | **Changed** |
| Nesting | Not supported | Recursive TXZ dicts as values (JSON/DOM) | **New** |
| Integrity | SHA-256 of whole file | Selectable: CRC-32, SHA-256, or xxHash64 | **Changed** |
| Versioning | None | Major/minor version in header | **New** |
| Key/value separation | Keys and values interleaved in one trie | Two separate tries (key trie + value trie), optionally shared symbol table | **Changed** |
| JSON support | Not applicable | Separate `txz-json` library using TXZ under the hood | **New** |
| Implementation | C with C++ wrapper | C with C++ wrapper, 100% test coverage | **Kept** |
| Streaming | Mentioned in notes | Deferred | -- |

---

## 9. Ideas Deferred (From Original Notes)

These ideas from the original notes are acknowledged but deferred from this spec:

- **LFSR sequence sections** -- using linear feedback shift registers to generate predictable bit patterns for compression. Interesting for general binary compression but not core to the dictionary use case.
- **PRLE (Patterned Run-Length Encoding)** -- RLE where the repeated unit can be any previously seen section. Useful for general compression; could be added as a value-store encoding optimization later.
- **LZW sections** -- dictionary-based compression of the trie data itself. Could be a compaction option in a future version.
- **Embedded VM (2018 idea)** -- a tiny instruction set for programmatic decompression. Powerful but adds complexity; not needed for the dictionary use case.
- **Streaming mode** -- emitting sections with periodic checksums for streaming decode. Relevant for very large datasets; deferred.

---

## 10. Open Questions

1. **Suffix table format**: Flat list (simple, small datasets) vs. suffix trie (better for large shared-suffix sets)? Spec currently allows both via a flag -- is this needed or should we pick one?

2. **Key ordering guarantee**: Should TXZ guarantee lexicographic iteration order? The trie naturally provides this if children are sorted, which aids range queries and prefix enumeration.

3. **Maximum nesting depth**: Should there be a spec-defined limit on nested dict depth for ROM/embedded safety? (e.g. 32 levels)

4. **Value trie vs. value store**: When should string values be compressed into a value trie (expensive to build, better compression) vs. stored as a flat value store (simple, fast)? Should this be encoder heuristic or caller-specified?

5. **Update/append semantics**: The current spec is build-once, read-many. Mutation is decode + modify + re-encode (Section 7). Is this sufficient for all 1.0 use cases?

6. **Endianness**: Spec assumes big-endian VarInts (matching the original flag-enc-num). The byte-aligned header fields (offsets, sizes) -- big or little endian? Recommend little-endian for the header (modern platform consistency), big-endian for VarInts (natural MSB-first reading in the bit stream).

7. **Addressing mode per nested dict**: When a value is a nested TXZ dict, does it inherit the parent's addressing mode, or can it declare its own? (Suggesting: it declares its own via its own Trie Config, allowing e.g. a byte-mode outer dict to contain a bit-mode inner dict for a large word list.)

---

## Appendix A: Source Documents

| Document | Date | Key Contributions |
|----------|------|-------------------|
| `dev/VBEDict/VBEDictNotes.txt` | 2003-06-25 (continued from 2002-09-04) | VBEDict format, trie flattening with push/pop, skip pointers, header structure, per-symbol bit-width |
| `dev/txz-2002.md` | 2002-2003 | Special symbols (`.^_!:\`), suffix substitution, key-value associations, LUT alternative |
| `dev/txz-plus.md` | ~2018 | LFSR, PRLE, LZW section types, flag-enc-num spec, streaming, embedded VM idea |
| `Lossless Data Compression...md` | ~2018 | Same as txz-plus.md with markdown formatting |
| `dev/bitfile.h` | ~2001-2003 | `dio_BitFile` -- original C bit-stream library. Opaque struct, single bit cursor, N-bit read/write with sign extension, configurable bit ordering (BE/LE), static ROM-read function, copy-or-reference construction. Direct ancestor of the stream library. |
