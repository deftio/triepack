# Architecture

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Library Stack

triepack is organized as a layered library stack. Each layer depends only
on the layers below it.

```
┌─────────────────────────────────────────────┐
│            triepack_wrapper (C++)            │
│         RAII wrappers around all C APIs      │
├───────────────────────┬─────────────────────┤
│   triepack_core (C)   │  triepack_json (C)  │
│  encoder, dict, trie  │  JSON codec using   │
│  lookup, iteration    │  triepack_core       │
├───────────────────────┴─────────────────────┤
│          triepack_bitstream (C)              │
│  arbitrary-width bit field read/write        │
└─────────────────────────────────────────────┘
```

**Dependencies:**
- `triepack_bitstream` -- standalone, no dependencies beyond libc
- `triepack_core` -- depends on `triepack_bitstream`
- `triepack_json` -- depends on `triepack_core`
- `triepack_wrapper` -- C++ layer wrapping all three C libraries

## Encoding Pipeline

The encoding pipeline transforms key-value pairs into a compact binary blob:

```
Input key-value pairs
        │
        ▼
┌──────────────────┐
│  Symbol analysis  │  Determine symbol alphabet, choose encoding
│                   │  strategy (fixed-width, multi-width zones,
│                   │  or Huffman for large objects)
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Trie construction│  Build prefix trie (stems) and suffix trie
│                   │  (common endings) in memory
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Value encoding   │  Encode typed values (null, bool, int, float,
│                   │  string, blob, array, nested dict) at terminals
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Serialization    │  Write header, symbol table, trie nodes, and
│                   │  values to a bitstream
└────────┬─────────┘
         ▼
    Output blob (uint8_t[])
```

## Two-Trie Architecture

triepack uses two tries that share an (optionally common) symbol table:

- **Prefix trie (stems):** Encodes the distinguishing prefixes of keys.
  Interior nodes carry symbol edges; terminal nodes carry encoded jump
  offsets or inline values.

- **Suffix trie (common endings):** Factors out shared suffixes (e.g.
  "-tion", "-ing", "-ment") to reduce redundancy. The prefix trie's
  terminal nodes reference positions in the suffix trie.

## Addressing Modes

Three addressing modes control how offsets and positions are measured
within the encoded blob:

| Mode     | Unit    | Use case                            |
|----------|---------|-------------------------------------|
| Bit      | 1 bit   | Maximum compression                 |
| Byte     | 8 bits  | Simpler decoding, byte-aligned      |
| Symbol   | Variable| UTF-8 or fixed-width symbol offsets  |

## ROM Constraints

The read path (dictionary lookup, iteration, prefix search) is designed
for ROM-friendly operation:

- **No heap allocation** in the read path -- the blob is accessed in place
- **No writable state** required beyond a small stack-allocated context
- **Byte-order aware** -- blobs are portable across endiannesses
- Supports both **32-bit and 64-bit** targets without conditional compilation
