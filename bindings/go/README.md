# triepack — Go

Native Go implementation of the Triepack `.trp` binary format.

Reads and writes the same bytes as the C reference library and every other
Triepack implementation, checked by the [shared conformance
suite](https://github.com/deftio/triepack/tree/main/tests/conformance).

## Use

```go
import "github.com/deftio/triepack/bindings/go"

buf, err := triepack.Encode(map[string]interface{}{
    "hello": uint64(42),
    "world": "foo",
})
result, err := triepack.Decode(buf)
```

## Build and test

```bash
go build ./...
go test ./...
```

## Links

- [Documentation](https://deftio.github.io/triepack/)
- [API reference](https://deftio.github.io/triepack/guide/api-reference/)
- [Source and issues](https://github.com/deftio/triepack)

## License

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
