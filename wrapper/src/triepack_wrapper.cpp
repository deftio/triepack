// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#include "triepack/triepack.hpp"

extern "C" {
#include "triepack/triepack.h"
}

namespace triepack {

// ---------------------------------------------------------------------------
// Encoder
// ---------------------------------------------------------------------------

Encoder::Encoder()
    : handle_(nullptr)
{
    tp_encoder_create(&handle_);
}

Encoder::~Encoder()
{
    tp_encoder_destroy(&handle_);
}

Encoder::Encoder(Encoder&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

Encoder& Encoder::operator=(Encoder&& other) noexcept
{
    if (this != &other) {
        tp_encoder_destroy(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

void Encoder::insert(const char* key, int32_t value)
{
    tp_value v = tp_value_int(value);
    tp_encoder_add(handle_, key, &v);
}

int Encoder::encode(const uint8_t** out_data, size_t* out_size)
{
    /* tp_encoder_build allocates; caller must cast away const to free */
    uint8_t *buf = nullptr;
    tp_result rc = tp_encoder_build(handle_, &buf, out_size);
    *out_data = buf;
    return rc;
}

tp_encoder* Encoder::handle() const
{
    return handle_;
}

// ---------------------------------------------------------------------------
// Dict
// ---------------------------------------------------------------------------

Dict::Dict(const uint8_t* data, size_t size)
    : handle_(nullptr)
{
    tp_dict_open(&handle_, data, size);
}

Dict::~Dict()
{
    tp_dict_close(&handle_);
}

Dict::Dict(Dict&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

Dict& Dict::operator=(Dict&& other) noexcept
{
    if (this != &other) {
        tp_dict_close(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

bool Dict::lookup(const char* key, int32_t* out_value) const
{
    tp_value val;
    tp_result rc = tp_dict_lookup(handle_, key, &val);
    if (rc == TP_OK && out_value) {
        *out_value = (int32_t)val.data.int_val;
    }
    return rc == TP_OK;
}

size_t Dict::size() const
{
    return tp_dict_count(handle_);
}

tp_dict* Dict::handle() const
{
    return handle_;
}

// ---------------------------------------------------------------------------
// Iterator
// ---------------------------------------------------------------------------

Iterator::Iterator(const Dict& dict)
    : handle_(nullptr)
{
    tp_dict_iterate(dict.handle(), &handle_);
}

Iterator::~Iterator()
{
    tp_iter_destroy(&handle_);
}

Iterator::Iterator(Iterator&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

Iterator& Iterator::operator=(Iterator&& other) noexcept
{
    if (this != &other) {
        tp_iter_destroy(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

bool Iterator::next()
{
    const char *k = nullptr;
    size_t klen = 0;
    tp_value v;
    return tp_iter_next(handle_, &k, &klen, &v) == TP_OK;
}

const char* Iterator::key() const
{
    /* TODO: cache key from last next() call */
    return nullptr;
}

int32_t Iterator::value() const
{
    /* TODO: cache value from last next() call */
    return 0;
}

tp_iterator* Iterator::handle() const
{
    return handle_;
}

} // namespace triepack
