---
layout: default
title: Examples
---

# Examples

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Contents

| Language | Section |
|----------|---------|
| [C](#example-programs) | 6 example programs with source and expected output |
| [C++](#cpp_usage) | RAII wrappers, move semantics, encode/decode lifecycle |
| [Python](#python-examples) | Pure-Python binding: encode, decode, JSON, file I/O |
| [JavaScript](#javascript-examples) | Pure-JS binding: encode, decode, JSON, Node.js file I/O |
| [Go](#go-examples) | Pure-Go binding: encode, decode, file I/O |
| [Rust](#rust-examples) | Pure-Rust binding: encode, decode, file I/O |
| [Swift](#swift-examples) | Pure-Swift binding: encode, decode via SPM |
| [Kotlin](#kotlin-examples) | Pure-Kotlin/JVM binding: encode, decode |
| [Java](#java-examples) | Pure-Java binding: encode, decode |

---

Example programs are built when `BUILD_EXAMPLES=ON` (the default). After
building, the executables are in `build/examples/`.

## Building the Examples

```bash
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_JSON=ON
cmake --build build
```

All examples are also registered as CTest targets. Run them with:

```bash
ctest --test-dir build -R example_
```

---

## Example Programs

### `basic_encode_decode`

**What it demonstrates:** Encodes a small set of key-value pairs into a
triepack blob, then reads them back with the dictionary API. Shows
integer values, string values, and key-only (set membership) entries.

```bash
./build/examples/basic_encode_decode
```

Expected output:

```
Entries added: 6
Encoded blob: XX bytes

Dictionary contains 6 keys

apple  -> 42
banana -> 17
cherry -> 8
date   -> "hello"
elderberry exists: yes
missing exists: no

Done.
```

---

### `compaction_benchmark`

**What it demonstrates:** Generates ~10,000 unique words programmatically
(base words + prefixes + suffixes) and measures TriePack compression ratio.
Encodes both keys-only and keys+values variants. Verifies all lookups.

```bash
./build/examples/compaction_benchmark
```

Example output:

```
TriePack Compaction Benchmark
─────────────────────────────
Keys:              10000
Raw key bytes:     85432

=== Keys Only (no values) ===
Bits per symbol:   5
Encoded blob:      XXXXX bytes
Compression ratio: X.XXx
Bits per key:      XX.X
Lookup verified:   10000/10000 OK

=== Keys + uint values ===
Encoded blob:      XXXXX bytes
Compression ratio: X.XXx
Bits per key:      XX.X
Lookup verified:   10000/10000 OK

Done.
```

---

### `rom_lookup`

**What it demonstrates:** Encodes a dictionary of HTTP status codes, then
opens it with `tp_dict_open_unchecked()` to demonstrate ROM-style access
with zero integrity overhead. Performs lookups and shows dictionary
metadata via `tp_dict_get_info()`.

```bash
./build/examples/rom_lookup
```

Expected output:

```
ROM Lookup Example
──────────────────
Encoded 5 HTTP status codes into XX bytes

Lookup results (unchecked open):
  200 -> OK
  404 -> Not Found
  500 -> Internal Server Error
  999 -> NOT FOUND (expected)

Dictionary info:
  Format version: 1.0
  Keys: 5
  Bits per symbol: X
  Total bytes: XX

Done.
```

---

### `prefix_search`

**What it demonstrates:** Encodes a word list with shared prefixes (words
starting with "app", "ban", "car", etc.), then uses `tp_dict_contains()`
to check membership and `tp_dict_get_info()` to display encoding metadata.

```bash
./build/examples/prefix_search
```

Expected output:

```
Prefix Search Example
─────────────────────
Encoded 12 words into XX bytes

Membership checks:
  apple:   found
  apply:   found
  banana:  found
  missing: NOT found

Dictionary info:
  Keys: 12
  Bits per symbol: X
  Has values: no

Done.
```

---

### `json_roundtrip`

**What it demonstrates:** Encodes a JSON object into the `.trp` binary
format, decodes it back to JSON (both compact and pretty-printed), and
performs DOM-style path lookups on the encoded data.

Requires `BUILD_JSON=ON`.

```bash
./build/examples/json_roundtrip
```

Expected output:

```
JSON Round-Trip Example
───────────────────────
Original JSON (XX bytes):
  {"name":"TriePack","version":1,"active":true}

Encoded to XX bytes (.trp)
Compression ratio: X.Xx

Decoded (compact):
  {"active":true,"name":"TriePack","version":1}

Decoded (pretty):
  {
    "active": true,
    "name": "TriePack",
    "version": 1
  }

DOM path lookups:
  name -> "TriePack"
  version -> 1

Done.
```

---

### `cpp_usage`

**What it demonstrates:** Uses the C++ RAII wrappers (`triepack::Encoder`,
`triepack::Dict`) to encode key-value pairs and look them up. Shows
move semantics and the full encode/decode lifecycle in C++.

```bash
./build/examples/cpp_usage
```

Expected output:

```
C++ Usage Example
─────────────────
Encoded 3 entries into XX bytes

Lookups:
  apple  -> 42
  banana -> 17
  cherry -> 99

Dict size: 3

Done.
```

---

## Python Examples {#python-examples}

The Python binding is a pure-Python implementation -- no C extension or FFI
required. Install with:

```bash
cd bindings/python
pip install -e .
```

### Encode and Decode

```python
from triepack import encode, decode

# Encode a dictionary
data = {
    "apple": 42,
    "banana": 17,
    "cherry": 8,
}
buf = encode(data)
print(f"Encoded {len(data)} entries -> {len(buf)} bytes")

# Decode back
result = decode(buf)
for key, value in sorted(result.items()):
    print(f"  {key} -> {value}")

assert result == data
print("Round-trip OK")
```

### JSON Round-Trip

```python
from triepack import encode, decode
import json

# Start with a JSON string
json_str = '{"name":"TriePack","version":1,"active":true}'
data = json.loads(json_str)

# Encode to .trp binary
buf = encode(data)
print(f"JSON ({len(json_str)} bytes) -> .trp ({len(buf)} bytes)")

# Decode back to dict
result = decode(buf)
print("Decoded:", json.dumps(result, sort_keys=True))
```

### File I/O

```python
from triepack import encode, decode

data = {"hello": "world", "count": 42, "flag": True}

# Write to file
with open("demo.trp", "wb") as f:
    f.write(encode(data))

# Read back
with open("demo.trp", "rb") as f:
    loaded = decode(f.read())

print("From file:", loaded)
assert loaded == data
```

### Running the Python Tests

```bash
cd bindings/python
pip install -e ".[test]"
pytest -v
```

---

## JavaScript Examples {#javascript-examples}

The JavaScript binding is a pure-JS implementation that reads and writes
the same `.trp` binary format. No native dependencies.

```bash
cd bindings/javascript
npm install
```

### Encode and Decode

```js
const { encode, decode } = require('./src/index');

// Encode a dictionary
const data = {
    apple: 42,
    banana: 17,
    cherry: 8,
};
const buf = encode(data);
console.log(`Encoded ${Object.keys(data).length} entries -> ${buf.length} bytes`);

// Decode back
const result = decode(buf);
for (const [key, value] of Object.entries(result).sort()) {
    console.log(`  ${key} -> ${value}`);
}

const assert = require('assert');
assert.deepStrictEqual(result, data);
console.log('Round-trip OK');
```

### JSON Round-Trip

```js
const { encode, decode } = require('./src/index');

// Start with a JSON string
const jsonStr = '{"name":"TriePack","version":1,"active":true}';
const data = JSON.parse(jsonStr);

// Encode to .trp binary
const buf = encode(data);
console.log(`JSON (${jsonStr.length} bytes) -> .trp (${buf.length} bytes)`);

// Decode back to object
const result = decode(buf);
console.log('Decoded:', JSON.stringify(result, Object.keys(result).sort()));
```

### File I/O (Node.js)

```js
const { encode, decode } = require('./src/index');
const fs = require('fs');

const data = { hello: 'world', count: 42, flag: true };

// Write to file
fs.writeFileSync('demo.trp', Buffer.from(encode(data)));

// Read back
const loaded = decode(new Uint8Array(fs.readFileSync('demo.trp')));
console.log('From file:', loaded);

const assert = require('assert');
assert.deepStrictEqual(loaded, data);
```

### Running the JavaScript Tests

```bash
cd bindings/javascript
npm test
```

---

## Go Examples {#go-examples}

The Go binding is a pure-Go implementation with no CGo or external dependencies.

```bash
cd bindings/go
```

### Encode and Decode

```go
package main

import (
    "fmt"
    "github.com/deftio/triepack/bindings/go/triepack"
)

func main() {
    data := map[string]interface{}{
        "apple":  42,
        "banana": 17,
        "cherry": 8,
    }
    buf, _ := triepack.Encode(data)
    fmt.Printf("Encoded %d entries -> %d bytes\n", len(data), len(buf))

    result, _ := triepack.Decode(buf)
    for k, v := range result {
        fmt.Printf("  %s -> %v\n", k, v)
    }
}
```

### Running Go Tests

```bash
cd bindings/go
go test -v ./...
```

---

## Rust Examples {#rust-examples}

The Rust binding is a pure-Rust implementation with no external dependencies.

```bash
cd bindings/rust
```

### Encode and Decode

```rust
use triepack::{encode, decode, Value};
use std::collections::HashMap;

fn main() {
    let mut data = HashMap::new();
    data.insert("apple".to_string(), Value::UInt(42));
    data.insert("banana".to_string(), Value::UInt(17));
    data.insert("cherry".to_string(), Value::UInt(8));

    let buf = encode(&data).unwrap();
    println!("Encoded {} entries -> {} bytes", data.len(), buf.len());

    let result = decode(&buf).unwrap();
    for (k, v) in &result {
        println!("  {} -> {:?}", k, v);
    }
}
```

### Running Rust Tests

```bash
cd bindings/rust
cargo test
```

---

## Swift Examples {#swift-examples}

The Swift binding uses Swift Package Manager with no external dependencies.

```bash
cd bindings/swift
```

### Encode and Decode

```swift
import Triepack

let data: [String: TriepackValue] = [
    "apple": .uint(42),
    "banana": .uint(17),
    "cherry": .uint(8),
]

let buf = try Triepack.encode(data)
print("Encoded \(data.count) entries -> \(buf.count) bytes")

let result = try Triepack.decode(buf)
for (key, value) in result {
    print("  \(key) -> \(value)")
}
```

### Running Swift Tests

```bash
cd bindings/swift
swift test
```

---

## Kotlin Examples {#kotlin-examples}

The Kotlin binding is a pure Kotlin/JVM implementation using Gradle.

```bash
cd bindings/kotlin
```

### Encode and Decode

```kotlin
import com.deftio.triepack.*

fun main() {
    val data = mapOf<String, TpValue?>(
        "apple" to TpValue.UInt(42),
        "banana" to TpValue.UInt(17),
        "cherry" to TpValue.UInt(8),
    )
    val buf = encode(data)
    println("Encoded ${data.size} entries -> ${buf.size} bytes")

    val result = decode(buf)
    for ((k, v) in result) {
        println("  $k -> $v")
    }
}
```

### Running Kotlin Tests

```bash
cd bindings/kotlin
./gradlew test
```

---

## Java Examples {#java-examples}

The Java binding is a pure Java implementation using Gradle.

```bash
cd bindings/java
```

### Encode and Decode

```java
import com.deftio.triepack.*;
import java.util.LinkedHashMap;
import java.util.Map;

public class Example {
    public static void main(String[] args) {
        Map<String, TpValue> data = new LinkedHashMap<>();
        data.put("apple", TpValue.ofUInt(42));
        data.put("banana", TpValue.ofUInt(17));
        data.put("cherry", TpValue.ofUInt(8));

        byte[] buf = TriePack.encode(data);
        System.out.printf("Encoded %d entries -> %d bytes%n",
                          data.size(), buf.length);

        Map<String, TpValue> result = TriePack.decode(buf);
        for (var entry : result.entrySet()) {
            System.out.printf("  %s -> %s%n",
                              entry.getKey(), entry.getValue());
        }
    }
}
```

### Running Java Tests

```bash
cd bindings/java
./gradlew test
```

---

## Cross-Language Compatibility

All bindings produce byte-identical `.trp` output. You can encode in any
language and decode in any other:

```bash
# Encode with Python
python -c "
from triepack import encode
with open('/tmp/cross.trp', 'wb') as f:
    f.write(encode({'greeting': 'hello', 'year': 2026}))
"

# Decode with Node.js
node -e "
const { decode } = require('./bindings/javascript/src/index');
const fs = require('fs');
const result = decode(new Uint8Array(fs.readFileSync('/tmp/cross.trp')));
console.log(result);
"
```

The `tests/fixtures/` directory contains `.trp` files generated by the C
library. All language bindings validate that they can decode these fixtures
and re-encode them to identical bytes.

---

## Writing Your Own

Link against the appropriate library target:

```cmake
add_executable(my_example my_example.c)
target_link_libraries(my_example PRIVATE triepack_core)
```

For JSON examples, link `triepack_json`. For C++ examples, link
`triepack_wrapper`:

```cmake
add_executable(my_cpp_example my_example.cpp)
target_link_libraries(my_cpp_example PRIVATE triepack_wrapper)
```
