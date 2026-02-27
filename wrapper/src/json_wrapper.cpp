// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#include "triepack/json.hpp"

extern "C" {
#include "triepack/triepack_json.h"
}

#include <cstring>

namespace triepack {

// ---------------------------------------------------------------------------
// Json
// ---------------------------------------------------------------------------

Json::Json()
    : handle_(nullptr)
{
}

Json::~Json()
{
    tp_json_close(&handle_);
}

Json::Json(Json&& other) noexcept
    : handle_(nullptr)
{
    auto *tmp = other.handle_;
    other.handle_ = nullptr;
    handle_ = tmp;
}

Json& Json::operator=(Json&& other) noexcept
{
    if (this != &other) {
        tp_json_close(&handle_);
        auto *tmp = other.handle_;
        other.handle_ = nullptr;
        handle_ = tmp;
    }
    return *this;
}

int Json::encode(const char* json_str, const uint8_t** out_data, size_t* out_size)
{
    if (!json_str || !out_data || !out_size) return TP_ERR_INVALID_PARAM;
    uint8_t *buf = nullptr;
    tp_result rc = tp_json_encode(json_str, std::strlen(json_str),
                                  &buf, out_size);
    *out_data = buf;
    return rc;
}

int Json::decode(const uint8_t* data, size_t size, const char** out_json)
{
    if (!data || !out_json) return TP_ERR_INVALID_PARAM;
    char *str = nullptr;
    size_t len = 0;
    tp_result rc = tp_json_decode(data, size, &str, &len);
    *out_json = str;
    return rc;
}

tp_json* Json::handle() const
{
    return handle_;
}

} // namespace triepack
