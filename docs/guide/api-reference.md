# API Reference

<!-- Copyright (c) 2026 M. A. Chatterjee -->

This is a human-readable summary organized by use case. For full details,
see the generated Doxygen documentation (`BUILD_DOCS=ON`).

---

## Bitstream (`triepack/txz_bitstream.h`)

Low-level arbitrary-width bit field read/write operations.

```c
/* Lifecycle */
txz_bitstream_t *txz_bs_create(size_t initial_capacity);
void             txz_bs_destroy(txz_bitstream_t *bs);

/* Writing */
int txz_bs_write_bits(txz_bitstream_t *bs, uint64_t value, uint8_t width);
int txz_bs_write_byte(txz_bitstream_t *bs, uint8_t byte);

/* Reading */
int txz_bs_read_bits(txz_bitstream_t *bs, uint64_t *value, uint8_t width);
int txz_bs_read_byte(txz_bitstream_t *bs, uint8_t *byte);

/* Positioning */
size_t txz_bs_bit_position(const txz_bitstream_t *bs);
int    txz_bs_seek(txz_bitstream_t *bs, size_t bit_offset);

/* Buffer access */
const uint8_t *txz_bs_buffer(const txz_bitstream_t *bs);
size_t          txz_bs_byte_length(const txz_bitstream_t *bs);
```

---

## Encoder (`triepack/txz_encoder.h`)

Build a compressed trie from key-value pairs.

```c
/* Lifecycle */
txz_encoder_t *txz_encoder_create(const txz_encoder_opts_t *opts);
void            txz_encoder_destroy(txz_encoder_t *enc);

/* Inserting entries */
int txz_encoder_add_string(txz_encoder_t *enc, const char *key, const char *value);
int txz_encoder_add_int(txz_encoder_t *enc, const char *key, int64_t value);
int txz_encoder_add_float(txz_encoder_t *enc, const char *key, double value);
int txz_encoder_add_bool(txz_encoder_t *enc, const char *key, int value);
int txz_encoder_add_null(txz_encoder_t *enc, const char *key);
int txz_encoder_add_blob(txz_encoder_t *enc, const char *key, const void *data, size_t len);

/* Finalization */
int txz_encoder_finalize(txz_encoder_t *enc, const uint8_t **out, size_t *out_len);
```

---

## Dictionary Reader (`triepack/txz_dict.h`)

Read-only access to an encoded trie blob.

```c
/* Lifecycle */
txz_dict_t *txz_dict_open(const uint8_t *blob, size_t len);
void         txz_dict_close(txz_dict_t *dict);

/* Lookup */
int txz_dict_lookup(const txz_dict_t *dict, const char *key, txz_value_t *value);
int txz_dict_has_key(const txz_dict_t *dict, const char *key);

/* Iteration */
txz_iter_t *txz_dict_iter(const txz_dict_t *dict);
int          txz_iter_next(txz_iter_t *it, const char **key, txz_value_t *value);
void         txz_iter_destroy(txz_iter_t *it);

/* Prefix search */
int txz_dict_prefix(const txz_dict_t *dict, const char *prefix,
                    txz_match_cb callback, void *user_data);

/* Metadata */
size_t txz_dict_count(const txz_dict_t *dict);
size_t txz_dict_blob_size(const txz_dict_t *dict);
```

---

## JSON (`triepack/txz_json.h`)

Encode/decode JSON documents as triepack blobs. Provided by the separate
`triepack_json` library.

```c
/* Encode JSON string to triepack blob */
int txz_json_encode(const char *json, size_t json_len,
                    uint8_t **out, size_t *out_len);

/* Decode triepack blob back to JSON string */
int txz_json_decode(const uint8_t *blob, size_t blob_len,
                    char **out, size_t *out_len);
```

---

## C++ Wrappers (`triepack/triepack.hpp`)

RAII wrappers around the C API.

```cpp
namespace triepack {

class Encoder {
public:
    Encoder();
    void add(const std::string &key, const std::string &value);
    void add(const std::string &key, int64_t value);
    std::vector<uint8_t> finalize();
};

class Dict {
public:
    explicit Dict(const std::vector<uint8_t> &blob);
    explicit Dict(const uint8_t *data, size_t len);
    Value lookup(const std::string &key) const;
    bool  has(const std::string &key) const;
    size_t size() const;
};

} // namespace triepack
```

---

## Error Codes

| Code             | Value | Meaning                        |
|------------------|-------|--------------------------------|
| `TXZ_OK`         | 0     | Success                        |
| `TXZ_ERR_NOMEM`  | -1    | Memory allocation failed       |
| `TXZ_ERR_INVAL`  | -2    | Invalid argument               |
| `TXZ_ERR_FORMAT` | -3    | Malformed blob or header       |
| `TXZ_ERR_NOTFOUND`| -4   | Key not found                  |
| `TXZ_ERR_OVERFLOW`| -5   | Buffer or bit width overflow   |
