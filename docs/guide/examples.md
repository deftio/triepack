# Examples

<!-- Copyright (c) 2026 M. A. Chatterjee -->

Example programs are built when `BUILD_EXAMPLES=ON` (the default). After
building, the executables are in `build/examples/`.

## Building the Examples

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build
```

---

## Example Programs

### `basic_encode_decode`

**What it demonstrates:** Encodes a small set of key-value pairs into a
triepack blob, then reads them back with the dictionary API.

```bash
./build/examples/basic_encode_decode
```

---

### `prefix_search`

**What it demonstrates:** Inserts a word list and performs prefix queries,
printing all keys that match a given prefix.

```bash
./build/examples/prefix_search
```

---

### `json_roundtrip`

**What it demonstrates:** Reads a JSON file, compresses it into a triepack
blob, then decodes it back to JSON. Prints compression ratio.

Requires `BUILD_JSON=ON`.

```bash
./build/examples/json_roundtrip sample.json
```

---

### `bitstream_demo`

**What it demonstrates:** Low-level bitstream operations -- writing and
reading arbitrary-width bit fields.

```bash
./build/examples/bitstream_demo
```

---

### `rom_blob`

**What it demonstrates:** Generates a C header file containing a triepack
blob as a `const uint8_t[]` array, suitable for embedding in ROM-based
firmware.

```bash
./build/examples/rom_blob wordlist.txt > blob.h
```

---

## Writing Your Own

Link against the appropriate library target:

```cmake
add_executable(my_example my_example.c)
target_link_libraries(my_example PRIVATE triepack::triepack)
```

For JSON examples, also link `triepack::triepack_json`.
