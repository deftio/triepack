// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#ifndef TRIEPACK_JSON_HPP
#define TRIEPACK_JSON_HPP

#include <cstddef>
#include <cstdint>

// Forward declaration of C handle
struct tp_json;

namespace triepack {

/// RAII wrapper around tp_json.
/// Encodes and decodes JSON documents using the triepack format.
class Json {
public:
    /// Construct a Json instance with default options.
    Json();

    /// Destructor. Releases the underlying C handle.
    ~Json();

    // Non-copyable
    Json(const Json&) = delete;
    Json& operator=(const Json&) = delete;

    // Movable
    Json(Json&& other) noexcept;
    Json& operator=(Json&& other) noexcept;

    /// Encode a JSON string into a triepack blob.
    /// @param json_str   Null-terminated JSON string.
    /// @param out_data   Receives a pointer to the encoded data.
    /// @param out_size   Receives the size of the encoded data in bytes.
    /// @return 0 on success, non-zero on error.
    int encode(const char* json_str, const uint8_t** out_data, size_t* out_size);

    /// Decode a triepack blob back into a JSON string.
    /// @param data       Pointer to the encoded data.
    /// @param size       Size of the encoded data in bytes.
    /// @param out_json   Receives a pointer to the decoded JSON string.
    /// @return 0 on success, non-zero on error.
    int decode(const uint8_t* data, size_t size, const char** out_json);

    /// Return the underlying C handle (nullable).
    tp_json* handle() const;

private:
    tp_json* handle_;
};

} // namespace triepack

#endif // TRIEPACK_JSON_HPP
