# Triepack — Swift

Native Swift implementation of the Triepack `.trp` binary format.

Reads and writes the same bytes as the C reference library and every other
Triepack implementation, checked by the [shared conformance
suite](https://github.com/deftio/triepack/tree/main/tests/conformance).

## Use

```swift
import Triepack

let data: [String: TriepackValue] = [
    "hello": .uint(42),
    "world": .string("foo"),
]

let buf = try Triepack.encode(data)
let result = try Triepack.decode(buf)
```

## Build and test

```bash
swift build
swift test
```

## Links

- [Documentation](https://deftio.github.io/triepack/)
- [API reference](https://deftio.github.io/triepack/guide/api-reference/)
- [Source and issues](https://github.com/deftio/triepack)

## License

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
