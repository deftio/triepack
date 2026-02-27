// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#include "triepack/bitstream.hpp"

extern "C" {
#include "triepack/triepack_bitstream.h"
}

namespace triepack {

// ---------------------------------------------------------------------------
// BitstreamReader
// ---------------------------------------------------------------------------

BitstreamReader::BitstreamReader(const uint8_t* data, size_t size)
    : handle_(nullptr)
{
    tp_bs_reader_create(&handle_, data, (uint64_t)size * 8);
}

BitstreamReader::~BitstreamReader()
{
    tp_bs_reader_destroy(&handle_);
}

BitstreamReader::BitstreamReader(BitstreamReader&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

BitstreamReader& BitstreamReader::operator=(BitstreamReader&& other) noexcept
{
    if (this != &other) {
        tp_bs_reader_destroy(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

uint32_t BitstreamReader::read(unsigned bits)
{
    uint32_t val = 0;
    tp_bs_read_bits32(handle_, (uint8_t)bits, &val);
    return val;
}

size_t BitstreamReader::position() const
{
    return (size_t)tp_bs_reader_position(handle_);
}

tp_bitstream_reader* BitstreamReader::handle() const
{
    return handle_;
}

// ---------------------------------------------------------------------------
// BitstreamWriter
// ---------------------------------------------------------------------------

BitstreamWriter::BitstreamWriter(size_t initial_capacity)
    : handle_(nullptr)
{
    tp_bs_writer_create(&handle_, initial_capacity, 0);
}

BitstreamWriter::~BitstreamWriter()
{
    tp_bs_writer_destroy(&handle_);
}

BitstreamWriter::BitstreamWriter(BitstreamWriter&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

BitstreamWriter& BitstreamWriter::operator=(BitstreamWriter&& other) noexcept
{
    if (this != &other) {
        tp_bs_writer_destroy(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

void BitstreamWriter::write(uint32_t value, unsigned bits)
{
    tp_bs_write_bits(handle_, value, (uint8_t)bits);
}

size_t BitstreamWriter::position() const
{
    return (size_t)tp_bs_writer_position(handle_);
}

const uint8_t* BitstreamWriter::data() const
{
    const uint8_t *buf = nullptr;
    uint64_t bit_len = 0;
    tp_bs_writer_get_buffer(handle_, &buf, &bit_len);
    return buf;
}

size_t BitstreamWriter::size() const
{
    const uint8_t *buf = nullptr;
    uint64_t bit_len = 0;
    tp_bs_writer_get_buffer(handle_, &buf, &bit_len);
    return (size_t)((bit_len + 7) / 8);
}

tp_bitstream_writer* BitstreamWriter::handle() const
{
    return handle_;
}

} // namespace triepack
