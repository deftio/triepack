// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#include "triepack/triepack.hpp"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "triepack/triepack.h"
}

namespace triepack
{

const size_t kMaxAlphabetSize = TP_MAX_ALPHABET_SIZE;

const char *message(Status status)
{
    return tp_result_str(static_cast<tp_result>(status));
}

namespace
{

Status to_status(tp_result rc)
{
    return static_cast<Status>(rc);
}

Type to_type(tp_value_type t)
{
    switch (t) {
    case TP_BOOL:
        return Type::Bool;
    case TP_INT:
        return Type::Int;
    case TP_UINT:
        return Type::UInt;
    case TP_FLOAT32:
        return Type::Float32;
    case TP_FLOAT64:
        return Type::Float64;
    case TP_STRING:
        return Type::String;
    case TP_BLOB:
        return Type::Blob;
    default:
        return Type::Null;
    }
}

/// Convert a decoded C value into an owning Value.
Value from_c(const tp_value &v)
{
    switch (to_type(v.type)) {
    case Type::Bool:
        return Value::boolean(v.data.bool_val);
    case Type::Int:
        return Value::integer(v.data.int_val);
    case Type::UInt:
        return Value::unsigned_integer(v.data.uint_val);
    case Type::Float32:
        return Value::float32(v.data.float32_val);
    case Type::Float64:
        return Value::float64(v.data.float64_val);
    case Type::String:
        return Value::string(std::string(v.data.string_val.str, v.data.string_val.str_len));
    case Type::Blob:
        return Value::blob(v.data.blob_val.data, v.data.blob_val.len);
    default:
        return Value::null();
    }
}

/// Build a C value borrowing this Value's storage; it must outlive the call.
tp_value to_c(const Value &v)
{
    switch (v.type()) {
    case Type::Bool:
        return tp_value_bool(v.as_bool());
    case Type::Int:
        return tp_value_int(v.as_int());
    case Type::UInt:
        return tp_value_uint(v.as_uint());
    case Type::Float32:
        return tp_value_float32(v.as_float32());
    case Type::Float64:
        return tp_value_float64(v.as_float64());
    case Type::String:
        return tp_value_string_n(v.as_string().data(), v.as_string().size());
    case Type::Blob:
        return tp_value_blob(v.as_blob().empty() ? reinterpret_cast<const uint8_t *>("")
                                                 : v.as_blob().data(),
                             v.as_blob().size());
    default:
        return tp_value_null();
    }
}

const std::string &empty_string()
{
    static const std::string s;
    return s;
}

const std::vector<uint8_t> &empty_blob()
{
    static const std::vector<uint8_t> b;
    return b;
}

} // namespace

// ---------------------------------------------------------------------------
// Value
// ---------------------------------------------------------------------------

Value::Value() : type_(Type::Null), bool_(false), int_(0), uint_(0), f32_(0.0f), f64_(0.0) {}

Value Value::null()
{
    return Value();
}

Value Value::boolean(bool v)
{
    Value out;
    out.type_ = Type::Bool;
    out.bool_ = v;
    return out;
}

Value Value::integer(int64_t v)
{
    Value out;
    out.type_ = Type::Int;
    out.int_ = v;
    return out;
}

Value Value::unsigned_integer(uint64_t v)
{
    Value out;
    out.type_ = Type::UInt;
    out.uint_ = v;
    return out;
}

Value Value::float32(float v)
{
    Value out;
    out.type_ = Type::Float32;
    out.f32_ = v;
    return out;
}

Value Value::float64(double v)
{
    Value out;
    out.type_ = Type::Float64;
    out.f64_ = v;
    return out;
}

Value Value::string(const std::string &v)
{
    Value out;
    out.type_ = Type::String;
    out.str_ = v;
    return out;
}

Value Value::blob(const std::vector<uint8_t> &v)
{
    Value out;
    out.type_ = Type::Blob;
    out.blob_ = v;
    return out;
}

Value Value::blob(const uint8_t *data, size_t size)
{
    Value out;
    out.type_ = Type::Blob;
    if (data && size > 0)
        out.blob_.assign(data, data + size);
    return out;
}

bool Value::as_bool() const
{
    return type_ == Type::Bool ? bool_ : false;
}

int64_t Value::as_int() const
{
    return type_ == Type::Int ? int_ : 0;
}

uint64_t Value::as_uint() const
{
    return type_ == Type::UInt ? uint_ : 0;
}

float Value::as_float32() const
{
    return type_ == Type::Float32 ? f32_ : 0.0f;
}

double Value::as_float64() const
{
    return type_ == Type::Float64 ? f64_ : 0.0;
}

const std::string &Value::as_string() const
{
    return type_ == Type::String ? str_ : empty_string();
}

const std::vector<uint8_t> &Value::as_blob() const
{
    return type_ == Type::Blob ? blob_ : empty_blob();
}

bool Value::operator==(const Value &other) const
{
    if (type_ != other.type_)
        return false;
    switch (type_) {
    case Type::Null:
        return true;
    case Type::Bool:
        return bool_ == other.bool_;
    case Type::Int:
        return int_ == other.int_;
    case Type::UInt:
        return uint_ == other.uint_;
    case Type::Float32:
        /* Bit pattern, so -0.0 differs from 0.0 and NaN payloads are kept. */
        return std::memcmp(&f32_, &other.f32_, sizeof(f32_)) == 0;
    case Type::Float64:
        return std::memcmp(&f64_, &other.f64_, sizeof(f64_)) == 0;
    case Type::String:
        return str_ == other.str_;
    case Type::Blob:
        return blob_ == other.blob_;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Encoder
// ---------------------------------------------------------------------------

Encoder::Encoder() : handle_(nullptr)
{
    tp_encoder_create(&handle_);
}

Encoder::~Encoder()
{
    tp_encoder_destroy(&handle_);
}

Encoder::Encoder(Encoder &&other) noexcept : handle_(other.handle_)
{
    other.handle_ = nullptr;
}

Encoder &Encoder::operator=(Encoder &&other) noexcept
{
    if (this != &other) {
        tp_encoder_destroy(&handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

Status Encoder::add(const std::string &key, const Value &value)
{
    return add(key.data(), key.size(), value);
}

Status Encoder::add(const char *key, size_t key_len, const Value &value)
{
    if (!handle_ || !key)
        return Status::InvalidParam;
    tp_value v = to_c(value);
    return to_status(tp_encoder_add_n(handle_, key, key_len, &v));
}

size_t Encoder::count() const
{
    return handle_ ? tp_encoder_count(handle_) : 0;
}

Status Encoder::build(std::vector<uint8_t> &out)
{
    out.clear();
    if (!handle_)
        return Status::InvalidParam;

    uint8_t *buf = nullptr;
    size_t len = 0;
    tp_result rc = tp_encoder_build(handle_, &buf, &len);
    if (rc != TP_OK)
        return to_status(rc);

    out.assign(buf, buf + len);
    free(buf);
    return Status::Ok;
}

Status Encoder::reset()
{
    if (!handle_)
        return Status::InvalidParam;
    return to_status(tp_encoder_reset(handle_));
}

// ---------------------------------------------------------------------------
// Dict
// ---------------------------------------------------------------------------

Dict::Dict() : handle_(nullptr), status_(Status::InvalidParam) {}

Dict::Dict(const uint8_t *data, size_t size) : handle_(nullptr), status_(Status::InvalidParam)
{
    open(data, size);
}

Dict::~Dict()
{
    tp_dict_close(&handle_);
}

Dict::Dict(Dict &&other) noexcept : handle_(other.handle_), status_(other.status_)
{
    other.handle_ = nullptr;
    other.status_ = Status::InvalidParam;
}

Dict &Dict::operator=(Dict &&other) noexcept
{
    if (this != &other) {
        tp_dict_close(&handle_);
        handle_ = other.handle_;
        status_ = other.status_;
        other.handle_ = nullptr;
        other.status_ = Status::InvalidParam;
    }
    return *this;
}

Status Dict::open(const uint8_t *data, size_t size)
{
    tp_dict_close(&handle_);
    status_ = to_status(tp_dict_open(&handle_, data, size));
    if (status_ != Status::Ok)
        handle_ = nullptr;
    return status_;
}

Status Dict::lookup(const std::string &key, Value &out) const
{
    return lookup(key.data(), key.size(), out);
}

Status Dict::lookup(const char *key, size_t key_len, Value &out) const
{
    out = Value::null();
    if (!handle_ || !key)
        return Status::InvalidParam;
    tp_value v;
    tp_result rc = tp_dict_lookup_n(handle_, key, key_len, &v);
    if (rc == TP_OK)
        out = from_c(v);
    return to_status(rc);
}

bool Dict::contains(const std::string &key) const
{
    Value ignored;
    return lookup(key, ignored) == Status::Ok;
}

size_t Dict::size() const
{
    return handle_ ? tp_dict_count(handle_) : 0;
}

// ---------------------------------------------------------------------------
// Iterator
// ---------------------------------------------------------------------------

Iterator::Iterator(const Dict &dict) : handle_(nullptr), status_(Status::InvalidParam)
{
    status_ = to_status(tp_dict_iterate(dict.handle(), &handle_));
}

Iterator::Iterator(const Dict &dict, const std::string &prefix)
    : handle_(nullptr), status_(Status::InvalidParam)
{
    status_ = to_status(tp_dict_find_prefix(dict.handle(), prefix.c_str(), &handle_));
}

Iterator::~Iterator()
{
    tp_iter_destroy(&handle_);
}

Iterator::Iterator(Iterator &&other) noexcept
    : handle_(other.handle_), status_(other.status_), key_(std::move(other.key_)),
      value_(std::move(other.value_))
{
    other.handle_ = nullptr;
    other.status_ = Status::InvalidParam;
}

Iterator &Iterator::operator=(Iterator &&other) noexcept
{
    if (this != &other) {
        tp_iter_destroy(&handle_);
        handle_ = other.handle_;
        status_ = other.status_;
        key_ = std::move(other.key_);
        value_ = std::move(other.value_);
        other.handle_ = nullptr;
        other.status_ = Status::InvalidParam;
    }
    return *this;
}

bool Iterator::next()
{
    if (!handle_) {
        return false;
    }
    const char *key = nullptr;
    size_t key_len = 0;
    tp_value val;
    tp_result rc = tp_iter_next(handle_, &key, &key_len, &val);
    status_ = to_status(rc);
    if (rc != TP_OK) {
        key_.clear();
        value_ = Value::null();
        return false;
    }
    /* The C iterator's key buffer is reused, so copy it out. */
    key_.assign(key, key_len);
    value_ = from_c(val);
    return true;
}

Status Iterator::reset()
{
    if (!handle_)
        return Status::InvalidParam;
    key_.clear();
    value_ = Value::null();
    status_ = to_status(tp_iter_reset(handle_));
    return status_;
}

} // namespace triepack
