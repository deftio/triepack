---
layout: default
title: Architecture
---

# Architecture

<!-- Copyright (c) 2026 M. A. Chatterjee -->

> **Sections below are marked `[built]` or `[design intent]`.** This page
> previously described the intended architecture in the present tense, with
> no way for a reader to tell which parts existed. Several did not. See
> [Status](../status.md) for the full audit and
> [Format Specification v2](format-spec-v2.md) for how the intended parts
> get built.

## Library Stack [built]

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

## Encoding Pipeline [partly built]

The encoding pipeline transforms key-value pairs into a compact binary blob:

```
Input key-value pairs
        │
        ▼
┌──────────────────┐
│  Symbol analysis  │  Determine symbol alphabet and fixed symbol
│                   │  width. (Multi-width zones and Huffman are
│                   │  design intent, not implemented.)
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Trie construction│  Build the prefix trie. (The suffix trie is
│                   │  design intent; src/core/suffix.c is a stub.)
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Value encoding   │  Encode typed values (null, bool, int, uint,
│                   │  float32/64, string, blob) at terminals.
│                   │  array and nested dict are not implemented.
└────────┬─────────┘
         ▼
┌──────────────────┐
│  Serialization    │  Write header, symbol table, trie nodes, and
│                   │  values to a bitstream
└────────┬─────────┘
         ▼
    Output blob (uint8_t[])
```

## Two-Trie Architecture [design intent — not implemented]

This is the original design and it was never built. `src/core/suffix.c` is a
stub, `TP_CTRL_SUFFIX` is never emitted, and `suffix_table_offset` is always
written as zero. Every occurrence of a shared ending is currently stored in
full.

[Format Specification v2 §8](format-spec-v2.md) is how this gets built: the
suffix trie becomes a deduplicated tail pool, which serves the same purpose —
storing "-tion", "-ing", "-ment" once — in a form that survives ten
independent implementations.

The original description follows.

triepack uses two tries that share an (optionally common) symbol table:

- **Prefix trie (stems):** Encodes the distinguishing prefixes of keys.
  Interior nodes carry symbol edges; terminal nodes carry encoded jump
  offsets or inline values.

- **Suffix trie (common endings):** Factors out shared suffixes (e.g.
  "-tion", "-ing", "-ment") to reduce redundancy. The prefix trie's
  terminal nodes reference positions in the suffix trie.

## Addressing Modes [design intent — not implemented]

Only bit addressing exists. `tp_addr_mode` is a public enum with four values
and `tp_encoder_options` accepts two of them, but the encoder hardcodes
bit-addressed trie offsets and byte-aligned values. As of 1.3.2 requesting
any other mode returns `TP_ERR_UNSUPPORTED` rather than being ignored.

The intended modes were:

| Mode     | Unit    | Use case                            |
|----------|---------|-------------------------------------|
| Bit      | 1 bit   | Maximum compression                 |
| Byte     | 8 bits  | Simpler decoding, byte-aligned      |
| Symbol   | Variable| UTF-8 or fixed-width symbol offsets  |

## ROM Constraints [partly built]

The read path is designed for ROM-friendly operation:

- **No heap allocation in lookup** -- the blob is accessed in place. This
  holds for `tp_dict_lookup` and `tp_dict_contains`. It does **not** hold for
  `tp_dict_open`, which allocates one control structure, or for iteration,
  which allocates an iterator and a growable key buffer.
- **No writable state** required beyond a small stack-allocated context
- **Byte-order aware** -- blobs are portable across endiannesses
- Supports both **32-bit and 64-bit** targets without conditional compilation
