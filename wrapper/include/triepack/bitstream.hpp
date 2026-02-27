// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#ifndef TRIEPACK_BITSTREAM_HPP
#define TRIEPACK_BITSTREAM_HPP

#include <cstddef>
#include <cstdint>

// Forward declarations of C handles
struct tp_bitstream_reader;
struct tp_bitstream_writer;

namespace triepack {

/// RAII wrapper around tp_bitstream_reader.
/// Reads arbitrary-width bit fields from a memory buffer.
class BitstreamReader {
public:
    /// Construct a reader over the given buffer.
    /// @param data  Pointer to the data buffer (not owned).
    /// @param size  Size of the buffer in bytes.
    BitstreamReader(const uint8_t* data, size_t size);

    /// Destructor. Releases the underlying C handle.
    ~BitstreamReader();

    // Non-copyable
    BitstreamReader(const BitstreamReader&) = delete;
    BitstreamReader& operator=(const BitstreamReader&) = delete;

    // Movable
    BitstreamReader(BitstreamReader&& other) noexcept;
    BitstreamReader& operator=(BitstreamReader&& other) noexcept;

    /// Read up to 32 bits and return the value.
    /// @param bits  Number of bits to read (1-32).
    /// @return The value read.
    uint32_t read(unsigned bits);

    /// Get the current bit position in the stream.
    size_t position() const;

    /// Return the underlying C handle (nullable).
    tp_bitstream_reader* handle() const;

private:
    tp_bitstream_reader* handle_;
};

/// RAII wrapper around tp_bitstream_writer.
/// Writes arbitrary-width bit fields into a growable buffer.
class BitstreamWriter {
public:
    /// Construct a writer with an optional initial capacity in bytes.
    explicit BitstreamWriter(size_t initial_capacity = 256);

    /// Destructor. Releases the underlying C handle and its buffer.
    ~BitstreamWriter();

    // Non-copyable
    BitstreamWriter(const BitstreamWriter&) = delete;
    BitstreamWriter& operator=(const BitstreamWriter&) = delete;

    // Movable
    BitstreamWriter(BitstreamWriter&& other) noexcept;
    BitstreamWriter& operator=(BitstreamWriter&& other) noexcept;

    /// Write a value using the given number of bits.
    /// @param value The value to write.
    /// @param bits  Number of bits to write (1-32).
    void write(uint32_t value, unsigned bits);

    /// Get the current bit position in the stream.
    size_t position() const;

    /// Get a pointer to the written data.
    const uint8_t* data() const;

    /// Get the number of bytes written (rounded up).
    size_t size() const;

    /// Return the underlying C handle (nullable).
    tp_bitstream_writer* handle() const;

private:
    tp_bitstream_writer* handle_;
};

} // namespace triepack

#endif // TRIEPACK_BITSTREAM_HPP
