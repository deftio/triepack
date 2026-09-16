# triepack v1.3.1

[![CI Build & Test](https://github.com/deftio/triepack/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/triepack/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/deftio/triepack?sort=semver&logo=github&logoColor=white&label=GitHub&color=24292F)](https://github.com/deftio/triepack/releases)
[![npm](https://img.shields.io/npm/v/triepack?logo=npm&logoColor=white&label=npm&color=A1231F)](https://www.npmjs.com/package/triepack)
[![PyPI](https://img.shields.io/pypi/v/triepack?logo=pypi&logoColor=white&label=PyPI&color=2B5B84)](https://pypi.org/project/triepack/)
[![C Coverage](https://img.shields.io/endpoint?url=https://deftio.github.io/triepack/coverage-badge.json)](https://deftio.github.io/triepack/coverage/)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD_2--Clause-0A5C9E)](LICENSE.txt)

A compressed trie-based dictionary format for fast, compact key-value storage.

TriePack encodes dictionaries into a compact binary format (`.trp`) optimized for fast lookups, prefix search, and ROM-safe deployment. It uses prefix sharing and bit-level packing with configurable symbol encoding and full value type support.

## Features

- **Compact binary format** — compressed tries with prefix sharing and bit-level packing
- **Fast lookups** — O(key-length) point queries via skip pointers
- **Prefix search** — iterate all keys matching a prefix, by descending the trie
- **ROM-safe** — readers work directly on `const` buffers with zero allocation
- **Typed values** — null, bool, int, uint, float32, float64, string, blob
- **Same bytes everywhere** — every implementation encodes identically, checked by a [conformance suite](tests/conformance/README.md) all ten run
- **JSON support** — encode/decode JSON documents to/from `.trp` format
- **10 languages** — C core with native implementations across 9 additional languages
- **Small footprint** — trie codec and bitstream are 49 KB of static library (Release, 64-bit); bindings are 934-1,796 source lines each

### Supported Languages

All bindings are native implementations that read/write the `.trp` binary format directly (no FFI).

| Language | Type | Source Lines | Binary/Library Size | Notes |
|----------|------|-------------|---------------------|-------|
| C | Core library | 5,387 | 49 KB (codec + bitstream), 72 KB with JSON | C99, 32-bit and 64-bit, ROM-safe |
| C++ | Wrapper | 1,113 | 37 KB (static) | C++11 RAII, owning `Value`, iteration and prefix search |
| Python | Binding | 934 | pure source | No dependencies |
| JavaScript | Binding | 1,134 | pure source | Node.js and browser, ships type declarations |
| TypeScript | Binding | 49 | pure source | In-repo wrapper; published types come with the npm package |
| Go | Binding | 1,307 | pure source | No dependencies |
| Rust | Binding | 1,796 | pure source | No dependencies, no `unsafe` |
| Swift | Binding | 1,156 | pure source | SPM package |
| Kotlin | Binding | 1,150 | pure source | Kotlin/JVM, Gradle |
| Java | Binding | 1,585 | pure source | Java 11+, Gradle |

Static library sizes are a Release build; the archives above are the trie
codec plus bitstream, and the figure with JSON adds `triepack_json`.

## Quick Start

### C / C++

```bash
# Install
git clone https://github.com/deftio/triepack.git
cd triepack
cmake -B build -DBUILD_TESTS=ON -DBUILD_JSON=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

```c
#include "triepack/triepack.h"

tp_encoder *enc = NULL;
tp_encoder_create(&enc);
tp_value v = tp_value_int(42);
tp_encoder_add(enc, "hello", &v);
uint8_t *buf = NULL;  size_t len = 0;
tp_encoder_build(enc, &buf, &len);

tp_dict *dict = NULL;
tp_dict_open(&dict, buf, len);
tp_value val;
if (tp_dict_lookup(dict, "hello", &val) == TP_OK)
    printf("hello -> %lld\n", (long long)val.data.int_val);
tp_dict_close(&dict);
tp_encoder_destroy(&enc);
free(buf);
```

### Python

```bash
pip install triepack
```

```python
from triepack import encode, decode

buf = encode({"hello": 42, "world": "foo"})
result = decode(buf)
print(result)  # {'hello': 42, 'world': 'foo'}
```

### JavaScript

```bash
npm install triepack
```

```js
const { encode, decode } = require('triepack');

const buf = encode({ hello: 42, world: 'foo' });
const result = decode(buf);
console.log(result);  // { hello: 42, world: 'foo' }
```

### TypeScript

The npm package ships its own type declarations, so there is nothing extra to
install:

```bash
npm install triepack
```

```ts
import { encode, decode, TriePackData } from 'triepack';

const data: TriePackData = { hello: 42, world: 'foo' };
const buf: Uint8Array = encode(data);
const result: TriePackData = decode(buf);
```

### Go

```bash
# The module lives in bindings/go; vendor it or add a replace directive:
# replace github.com/deftio/triepack => ./path/to/triepack/bindings/go
```

```go
data := map[string]interface{}{"hello": uint64(42), "world": "foo"}
buf, _ := triepack.Encode(data)
result, _ := triepack.Decode(buf)
```

### Rust

```bash
# Not on crates.io yet; add from source:
# [dependencies]
# triepack = { path = "bindings/rust" }
```

```rust
use triepack::{encode, decode, Value};
use std::collections::HashMap;

let mut data = HashMap::new();
data.insert("hello".into(), Value::UInt(42));
let buf = encode(&data);          // infallible for String keys
let result = decode(&buf)?;
```

### Swift

```bash
# Add to Package.swift dependencies
# .package(path: "bindings/swift")
```

```swift
import Triepack

let data: [String: TriepackValue] = ["hello": .uint(42)]
let buf = try Triepack.encode(data)
let result = try Triepack.decode(buf)
```

### Kotlin

```bash
# Add bindings/kotlin/ to your Gradle project
cd bindings/kotlin
gradle test
```

```kotlin
import com.deftio.triepack.*

val data = mapOf("hello" to TpValue.UInt(42))
val buf = encode(data)
val result = decode(buf)
```

### Java

```bash
# Add bindings/java/ to your Gradle project
cd bindings/java
gradle test
```

```java
import com.deftio.triepack.*;

Map<String, TpValue> data = new LinkedHashMap<>();
data.put("hello", TpValue.ofUInt(42));
byte[] buf = TriePack.encode(data);
Map<String, TpValue> result = TriePack.decode(buf);
```

### Asking a build what it is

Every implementation reports the same metadata, so a polyglot system can ask
each one and compare:

```js
require('triepack').version()
// { name: 'triepack', implementation: 'javascript', version: '1.3.1',
//   versionMajor: 1, versionMinor: 3, versionPatch: 1,
//   formatVersionMajor: 1, formatVersionMinor: 0, maxAlphabetSize: 249 }
```

The same call is `tp_version()` in C, `triepack::version()` in C++,
`version()` in Python, Rust and Kotlin, `VersionMetadata()` in Go,
`Triepack.versionInfo()` in Swift and `TriePack.version()` in Java. The
library version comes from `triepack-version.txt` at build time; the *format*
version is separate and moves only when the bytes change.

### Iterating and prefix search (C and C++)

```c
tp_iterator *it = NULL;
tp_dict_find_prefix(dict, "app", &it);      /* descends, does not scan */

const char *key; size_t key_len; tp_value val;
while (tp_iter_next(it, &key, &key_len, &val) == TP_OK)
    printf("%.*s\n", (int)key_len, key);
tp_iter_destroy(&it);
```

Keys come out in lexicographic byte order. The bindings decode to a native
map instead, so they iterate with whatever their language already provides.

See [Examples](docs/guide/examples.md) for more detailed usage including JSON round-trips, file I/O, and cross-language interop.

## Library Stack

```
triepack_json          (JSON encode/decode)
    |
triepack_core          (trie codec: encoder, dictionary, iterator)
    |
triepack_bitstream     (bit-level I/O, VarInt, UTF-8)
```

Each layer can be used independently. `triepack_wrapper` provides C++11 RAII wrappers over all three.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build test suite |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `BUILD_JSON` | ON | Build JSON library |
| `BUILD_DOCS` | OFF | Build Doxygen documentation |
| `ENABLE_COVERAGE` | OFF | Enable code coverage instrumentation |

## File Format

- Magic bytes: `TRP\0` (`0x54 0x52 0x50 0x00`)
- File extension: `.trp`
- 32-byte fixed header
- Bit-packed prefix trie, with the value store following it
- CRC-32 integrity check over the whole buffer

A valid checksum means the buffer is intact, not that it is trustworthy —
anyone who can supply a buffer can supply a matching CRC.

See `docs/internals/` for format details.

## Documentation

- [Getting Started](docs/guide/getting-started.md)
- [Building](docs/guide/building.md)
- [API Reference](docs/guide/api-reference.md)
- [Examples](docs/guide/examples.md)
- [Testing](docs/guide/testing.md)
- [Release Process](docs/guide/release-process.md)

## Project Status

**v1.3.1.** Core C library (bitstream, trie codec, JSON), C++ wrapper, and 8
language bindings (Python, JavaScript, TypeScript, Go, Rust, Swift, Kotlin,
Java) are implemented. C/C++, Python and JavaScript maintain **100% line
coverage**.

All ten implementations run a [shared conformance
suite](tests/conformance/README.md): for each of 50 cases every one must
decode the same C-generated fixture to the same values *and* re-encode it byte
for byte, and reject the same 11 malformed buffers. About 1,550 tests in total.

`scripts/make-release.sh --check` builds and tests all ten targets locally;
`scripts/test-ci-linux.sh` runs the ubuntu-only jobs in a container.

## Roadmap

### v1.1 — Client Libraries
- [x] TypeScript binding (wraps JS implementation)
- [x] Go binding
- [x] Swift binding (with SPM package)
- [x] Rust binding
- [x] Kotlin binding
- [x] Java binding
- [x] npm package for JavaScript/TypeScript (ships bundled type declarations)
- [x] PyPI package for Python
- [ ] crates.io package for Rust

### v1.2 — Format Enhancements
- [ ] Suffix table (shared ending compression)
- [ ] Huffman symbol encoding (for large dictionaries)
- [ ] Nested dict values (embed sub-dictionaries inline)

### v1.3 — Tooling & Ecosystem
- [x] `trp` CLI: encode/decode/validate/inspect
- [x] Language binding conformance test suite
- [x] Trie iteration and prefix search
- [ ] Fuzzy search (edit distance d<=2) — declared, returns `TP_ERR_UNSUPPORTED`
- [ ] Performance benchmarks across languages

## Contributing

Bug reports and pull requests are welcome. See
[CONTRIBUTING.md](.github/CONTRIBUTING.md) for how to build and test each
target, and for what a change to the binary format has to satisfy — every
implementation has to agree byte for byte, which the
[conformance suite](tests/conformance/README.md) checks.

Participation is covered by the [Code of Conduct](CODE_OF_CONDUCT.md).
Security issues go through [SECURITY.md](SECURITY.md) rather than the public
tracker.

Releases are cut with `./scripts/make-release.sh`; see [RELEASE.md](RELEASE.md).

## License

BSD-2-Clause. See [LICENSE.txt](LICENSE.txt).

Copyright (c) 2026 M. A. Chatterjee
