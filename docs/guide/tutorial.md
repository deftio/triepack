---
layout: default
title: Tutorial
---

# Tutorial: JSON Round-Trip

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This walkthrough encodes a JSON object into the `.trp` binary format,
decodes it back, and verifies the result. Choose your language:

<script src="{{ '/assets/tabs.js' | relative_url }}"></script>

<style>
.tp-tabs { margin: 1.5rem 0; }
.tp-tab-bar { display: flex; gap: 0; border-bottom: 2px solid #e0e0e0; }
.tp-tab-btn {
  padding: 0.5rem 1.25rem; border: none; background: none;
  cursor: pointer; font-size: 0.95rem; font-weight: 500;
  border-bottom: 2px solid transparent; margin-bottom: -2px; color: #555;
}
.tp-tab-btn.active { color: #006666; border-bottom-color: #006666; font-weight: 600; }
.tp-tab-panel { display: none; }
.tp-tab-panel.active { display: block; }
body:not(.tp-js-enabled) .tp-tab-panel { display: block; }
body:not(.tp-js-enabled) .tp-tab-bar { display: none; }
</style>

---

## What We Will Build

We will take this JSON:

```json
{
  "project": "triepack",
  "version": 1,
  "stable": true,
  "license": "BSD-2-Clause"
}
```

and:
1. **Encode** it into the `.trp` binary format
2. **Decode** it back to JSON
3. **Look up** individual keys
4. **Verify** the round-trip is lossless

---

## Step-by-Step

<div class="tp-tabs">
<div class="tp-tab-bar">
  <button class="tp-tab-btn active" data-lang="c">C</button>
  <button class="tp-tab-btn" data-lang="cpp">C++</button>
  <button class="tp-tab-btn" data-lang="js">JavaScript</button>
</div>

<!-- ───────────────── C ───────────────── -->
<div class="tp-tab-panel active" data-lang="c" markdown="1">

### C (using `triepack_json`)

#### 1. Include and set up

```c
#include "triepack/triepack_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *json =
        "{\"project\":\"triepack\",\"version\":1,"
        "\"stable\":true,\"license\":\"BSD-2-Clause\"}";
    size_t json_len = strlen(json);
```

#### 2. Encode JSON to `.trp`

`tp_json_encode` parses the JSON string, builds a compressed trie, and
writes the binary blob into a `malloc`-allocated buffer:

```c
    uint8_t *buf = NULL;
    size_t   buf_len = 0;

    tp_result rc = tp_json_encode(json, json_len, &buf, &buf_len);
    if (rc != TP_OK) {
        fprintf(stderr, "encode: %s\n", tp_result_str(rc));
        return 1;
    }
    printf("Encoded %zu JSON bytes -> %zu .trp bytes\n", json_len, buf_len);
```

#### 3. Decode `.trp` back to JSON

```c
    char  *out_json = NULL;
    size_t out_len  = 0;

    rc = tp_json_decode(buf, buf_len, &out_json, &out_len);
    if (rc != TP_OK) {
        fprintf(stderr, "decode: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }
    printf("Decoded: %s\n", out_json);
```

#### 4. DOM-style key lookup

Without decoding the entire blob, you can look up individual keys:

```c
    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    if (rc == TP_OK) {
        tp_value val;
        if (tp_json_lookup_path(j, "project", &val) == TP_OK)
            printf("project = \"%.*s\"\n",
                   (int)val.data.string_val.str_len,
                   val.data.string_val.str);

        if (tp_json_lookup_path(j, "version", &val) == TP_OK)
            printf("version = %lld\n", (long long)val.data.int_val);

        tp_json_close(&j);
    }
```

#### 5. Clean up

```c
    free(out_json);
    free(buf);
    printf("Done.\n");
    return 0;
}
```

#### Build and run

```bash
cmake -B build -DBUILD_JSON=ON
cmake --build build
./build/examples/json_roundtrip      # ships with triepack
```

Or compile your own file:

```bash
gcc -o my_demo my_demo.c \
    -Iinclude -Lbuild/src/json -Lbuild/src/core -Lbuild/src/bitstream \
    -ltriepack_json -ltriepack_core -ltriepack_bitstream
```

</div>

<!-- ───────────────── C++ ───────────────── -->
<div class="tp-tab-panel" data-lang="cpp" markdown="1">

### C++ (using `triepack::Json`)

The C++ wrapper provides RAII lifetime management. No manual `free()`
calls needed.

#### 1. Include and set up

```cpp
#include "triepack/json.hpp"

extern "C" {
#include "triepack/triepack_json.h"   // tp_json_decode_pretty, tp_result_str
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main()
{
    const char *json =
        "{\"project\":\"triepack\",\"version\":1,"
        "\"stable\":true,\"license\":\"BSD-2-Clause\"}";
```

#### 2. Encode

```cpp
    triepack::Json codec;

    const uint8_t *buf  = nullptr;
    size_t         buf_len = 0;

    int rc = codec.encode(json, &buf, &buf_len);
    if (rc != 0) {
        std::fprintf(stderr, "encode: %s\n", tp_result_str(rc));
        return 1;
    }
    std::printf("Encoded %zu JSON bytes -> %zu .trp bytes\n",
                std::strlen(json), buf_len);
```

#### 3. Decode

```cpp
    const char *out_json = nullptr;
    rc = codec.decode(buf, buf_len, &out_json);
    if (rc != 0) {
        std::fprintf(stderr, "decode: %s\n", tp_result_str(rc));
        return 1;
    }
    std::printf("Decoded: %s\n", out_json);
```

#### 4. Pretty-print (via C API)

```cpp
    char  *pretty = nullptr;
    size_t pretty_len = 0;
    rc = tp_json_decode_pretty(buf, buf_len, "  ", &pretty, &pretty_len);
    if (rc == 0) {
        std::printf("Pretty:\n%s\n", pretty);
        std::free(pretty);
    }
```

#### 5. Done

The `triepack::Json` destructor cleans up automatically when `codec`
goes out of scope. The `out_json` string returned by `decode` is owned
by the codec instance and freed with it.

#### Build

```cmake
add_executable(my_demo my_demo.cpp)
target_link_libraries(my_demo PRIVATE triepack_wrapper triepack_json)
```

</div>

<!-- ───────────────── JavaScript ───────────────── -->
<div class="tp-tab-panel" data-lang="js" markdown="1">

### JavaScript (Node.js)

The JavaScript binding is a pure-JS implementation that reads and writes
the same `.trp` binary format. No native dependencies.

#### 1. Install

```bash
cd bindings/javascript
npm install
```

#### 2. Encode

```js
const { encode, decode } = require('./src/index');

const data = {
    project: "triepack",
    version: 1,
    stable: true,
    license: "BSD-2-Clause"
};

const buf = encode(data);
console.log(`Encoded -> ${buf.length} bytes`);
```

`encode()` takes a plain object and returns a `Uint8Array` containing
the `.trp` binary.

#### 3. Decode

```js
const result = decode(buf);
console.log("Decoded:", result);
// { license: 'BSD-2-Clause', project: 'triepack', stable: true, version: 1 }
```

`decode()` takes a `Uint8Array` and returns a plain object. Keys are
sorted alphabetically (trie order).

#### 4. Write to file and read back

```js
const fs = require('fs');

// Save to disk
fs.writeFileSync('demo.trp', Buffer.from(buf));

// Read back
const loaded = decode(new Uint8Array(fs.readFileSync('demo.trp')));
console.log("From file:", loaded);
```

The `.trp` file is byte-for-byte identical to what the C library
produces. You can encode in JavaScript and decode in C (or Python), or
vice versa.

#### 5. Verify round-trip

```js
const assert = require('assert');
assert.deepStrictEqual(result, data);
console.log("Round-trip OK");
```

</div>
</div>

---

## What Happened Under the Hood

1. **Parse** -- the JSON string is parsed into key-value pairs
2. **Sort** -- keys are sorted alphabetically
3. **Build trie** -- shared prefixes are collapsed into a compressed trie
4. **Encode values** -- each value is stored with a type tag (null, bool, int, float, string)
5. **Write header** -- magic bytes (`TRP\0`), version, key count, offsets
6. **CRC-32** -- an integrity checksum is appended

The resulting `.trp` blob is typically **40--60% smaller** than the
original JSON for structured data with shared key prefixes.

---

## Key Differences from JSON

| Feature            | JSON                   | TriePack (.trp)             |
|--------------------|------------------------|-----------------------------|
| Format             | Text                   | Binary                      |
| Key order          | Insertion order        | Sorted (trie order)         |
| Key lookup         | O(n) scan              | O(k) trie walk (k = key length) |
| Duplicate keys     | Allowed (last wins)    | Not allowed                 |
| Compression        | None                   | Shared-prefix trie          |
| Integrity          | None                   | CRC-32 checksum             |
| Human-readable     | Yes                    | No (use `trp decode`)       |

---

## Next Steps

- [Examples](examples) -- more programs covering all value types, ROM lookup, prefix search
- [API Reference](api-reference) -- complete function reference for all languages
- [How It Works](how-it-works) -- deep dive into the trie compression algorithm
