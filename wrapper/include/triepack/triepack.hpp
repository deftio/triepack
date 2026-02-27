// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#ifndef TRIEPACK_TRIEPACK_HPP
#define TRIEPACK_TRIEPACK_HPP

#include <cstddef>
#include <cstdint>

// Forward declarations of C handles
struct tp_encoder;
struct tp_dict;
struct tp_iterator;

namespace triepack {

/// RAII wrapper around tp_encoder.
/// Builds a compressed trie dictionary from key-value pairs.
class Encoder {
public:
    /// Construct an encoder with default options.
    Encoder();

    /// Destructor. Releases the underlying C handle.
    ~Encoder();

    // Non-copyable
    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    // Movable
    Encoder(Encoder&& other) noexcept;
    Encoder& operator=(Encoder&& other) noexcept;

    /// Insert a key-value pair into the encoder.
    /// @param key    Null-terminated key string.
    /// @param value  Integer value to associate with the key.
    void insert(const char* key, int32_t value);

    /// Encode the trie and return a serialized blob.
    /// @param out_data  Receives a pointer to the encoded data.
    /// @param out_size  Receives the size of the encoded data in bytes.
    /// @return 0 on success, non-zero on error.
    int encode(const uint8_t** out_data, size_t* out_size);

    /// Return the underlying C handle (nullable).
    tp_encoder* handle() const;

private:
    tp_encoder* handle_;
};

/// RAII wrapper around tp_dict.
/// Provides read-only access to a compressed trie dictionary.
class Dict {
public:
    /// Construct a dict from a serialized blob.
    /// @param data  Pointer to the encoded data (not owned).
    /// @param size  Size of the encoded data in bytes.
    Dict(const uint8_t* data, size_t size);

    /// Destructor. Releases the underlying C handle.
    ~Dict();

    // Non-copyable
    Dict(const Dict&) = delete;
    Dict& operator=(const Dict&) = delete;

    // Movable
    Dict(Dict&& other) noexcept;
    Dict& operator=(Dict&& other) noexcept;

    /// Look up a key and return its value.
    /// @param key        Null-terminated key string.
    /// @param out_value  Receives the value if found.
    /// @return true if the key was found, false otherwise.
    bool lookup(const char* key, int32_t* out_value) const;

    /// Return the number of entries in the dictionary.
    size_t size() const;

    /// Return the underlying C handle (nullable).
    tp_dict* handle() const;

private:
    tp_dict* handle_;
};

/// RAII wrapper around tp_iterator.
/// Iterates over all entries in a Dict.
class Iterator {
public:
    /// Construct an iterator for the given dictionary.
    /// @param dict  The dictionary to iterate (must outlive the iterator).
    explicit Iterator(const Dict& dict);

    /// Destructor. Releases the underlying C handle.
    ~Iterator();

    // Non-copyable
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    // Movable
    Iterator(Iterator&& other) noexcept;
    Iterator& operator=(Iterator&& other) noexcept;

    /// Advance to the next entry.
    /// @return true if there is a valid entry, false if iteration is complete.
    bool next();

    /// Get the current key. Valid only after a successful next() call.
    const char* key() const;

    /// Get the current value. Valid only after a successful next() call.
    int32_t value() const;

    /// Return the underlying C handle (nullable).
    tp_iterator* handle() const;

private:
    tp_iterator* handle_;
};

} // namespace triepack

#endif // TRIEPACK_TRIEPACK_HPP
