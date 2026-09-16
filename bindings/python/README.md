# triepack

Compressed trie dictionary format (`.trp`) — compact binary key-value storage
with fast lookups and prefix search.

This is the native Python implementation: pure Python, no dependencies, no C
extension to build. It reads and writes the same bytes as the C reference
library and the JavaScript, Go, Rust, Swift, Java and Kotlin implementations.

## Install

```bash
pip install triepack
```

## Use

```python
from triepack import encode, decode

buf = encode({"hello": 42, "world": "foo"})
# buf is bytes holding the .trp binary

result = decode(buf)
print(result)  # {'hello': 42, 'world': 'foo'}
```

## Values

| Python            | `.trp` type        |
|-------------------|--------------------|
| `None`            | null               |
| `bool`            | bool               |
| `int >= 0`        | uint               |
| `int < 0`         | int                |
| `float`           | float64            |
| `str`             | string (UTF-8)     |
| `bytes`           | blob               |

Decoding also accepts float32 values written by other implementations,
widening them to a Python float. Python integers are arbitrary precision, so
the full 64-bit signed and unsigned ranges round-trip exactly.

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
cd bindings/python
pip install -e ".[test]"
python -m pytest
```

## License

Copyright (c) 2026 M. A. Chatterjee. BSD-2-Clause — see
[LICENSE.txt](LICENSE.txt).
