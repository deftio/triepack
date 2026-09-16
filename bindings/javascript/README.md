# triepack

Compressed trie dictionary format (`.trp`) — compact binary key-value storage
with fast lookups and prefix search.

This is the native JavaScript implementation: no dependencies, no native
addon, no build step. It reads and writes the same bytes as the C reference
library and the Python, Go, Rust, Swift, Java and Kotlin implementations.

## Install

```bash
npm install triepack
```

## Use

```js
const { encode, decode } = require('triepack');

const buf = encode({ hello: 42, world: 'foo' });
// buf is a Uint8Array holding the .trp binary

const result = decode(buf);
console.log(result);  // { hello: 42, world: 'foo' }
```

TypeScript declarations ship with the package, so `import { encode, decode }
from 'triepack'` is typed without a separate `@types` install.

## Values

| JavaScript        | `.trp` type        |
|-------------------|--------------------|
| `null`            | null               |
| `true` / `false`  | bool               |
| integer `>= 0`    | uint               |
| integer `< 0`     | int                |
| non-integer       | float64            |
| `string`          | string (UTF-8)     |
| `Uint8Array`      | blob               |

Decoding also accepts float32 values written by other implementations,
widening them to a JavaScript number.

### Integer range

JavaScript keeps integers in a double, so values must be exactly
representable: up to `Number.MAX_SAFE_INTEGER` for unsigned, and down to
-2^52 for signed once zigzag encoding is applied. Anything outside that raises
a `RangeError` rather than writing bytes that would decode to a different
number. A buffer from another language carrying a larger integer raises on
decode for the same reason.

## Format

Every buffer carries a 32-byte header, a bit-packed prefix trie, a typed value
store and a CRC-32. Keys are stored once per shared prefix, and symbols are
packed at the minimum width the key alphabet needs.

The encoder is deterministic: the same input produces the same bytes in every
implementation, which is checked by a
[shared conformance suite](https://github.com/deftio/triepack/tree/main/tests/conformance)
that all nine implementations run.

## Links

- [Documentation](https://deftio.github.io/triepack/)
- [API reference](https://deftio.github.io/triepack/guide/api-reference/)
- [Source and issues](https://github.com/deftio/triepack)
- [Changelog](https://github.com/deftio/triepack/blob/main/CHANGELOG.md)

## Development

```bash
cd bindings/javascript
npm install
npm test
```

## License

Copyright (c) 2026 M. A. Chatterjee. BSD-2-Clause — see
[LICENSE.txt](LICENSE.txt).
