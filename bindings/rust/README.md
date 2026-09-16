# triepack — Rust

Native Rust implementation of the Triepack `.trp` binary format.

Reads and writes the same bytes as the C reference library and every other
Triepack implementation, checked by the [shared conformance
suite](https://github.com/deftio/triepack/tree/main/tests/conformance).

## Use

```rust
use std::collections::HashMap;
use triepack::{decode, encode, Value};

let mut data = HashMap::new();
data.insert("hello".to_string(), Value::UInt(42));
data.insert("world".to_string(), Value::String("foo".into()));

let buf = encode(&data);
let result = decode(&buf)?;
```

## Build and test

```bash
cargo build
cargo test
```

## Links

- [Documentation](https://deftio.github.io/triepack/)
- [API reference](https://deftio.github.io/triepack/guide/api-reference/)
- [Source and issues](https://github.com/deftio/triepack)

## License

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
