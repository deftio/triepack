// Copyright (c) 2026 M. A. Chatterjee
// SPDX-License-Identifier: BSD-2-Clause

#ifndef TRIEPACK_TRIEPACK_HPP
#define TRIEPACK_TRIEPACK_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Forward declarations of C handles
struct tp_encoder;
struct tp_dict;
struct tp_iterator;

namespace triepack
{

/// Result of an operation. Values match the C API's tp_result.
enum class Status : int {
    Ok = 0,
    Eof = -1,
    Alloc = -2,
    InvalidParam = -3,
    InvalidPosition = -4,
    NotAligned = -5,
    Overflow = -6,
    InvalidUtf8 = -7,
    BadMagic = -10,
    Version = -11,
    Corrupt = -12,
    NotFound = -13,
    Truncated = -14,
    Alphabet = -15,
    Unsupported = -16
};

/// Human-readable description of a status.
const char *message(Status status);

/// Largest number of distinct byte values the keys may use.
extern const size_t kMaxAlphabetSize;

/// The eight value types the format carries.
enum class Type { Null, Bool, Int, UInt, Float32, Float64, String, Blob };

/**
 * A typed value, owning its string or blob payload.
 *
 * Int and UInt are distinct on the wire, so the choice of factory decides how
 * a number is stored; other implementations pick UInt for anything
 * non-negative, and matching that keeps encodings byte-identical.
 */
class Value
{
  public:
    /// A null value.
    Value();

    static Value null();
    static Value boolean(bool v);
    static Value integer(int64_t v);
    static Value unsigned_integer(uint64_t v);
    static Value float32(float v);
    static Value float64(double v);
    static Value string(const std::string &v);
    static Value blob(const std::vector<uint8_t> &v);
    static Value blob(const uint8_t *data, size_t size);

    Type type() const
    {
        return type_;
    }
    bool is_null() const
    {
        return type_ == Type::Null;
    }

    /// Accessors. Reading the wrong one returns a zero-like default rather
    /// than reinterpreting the stored value.
    bool as_bool() const;
    int64_t as_int() const;
    uint64_t as_uint() const;
    float as_float32() const;
    double as_float64() const;
    const std::string &as_string() const;
    const std::vector<uint8_t> &as_blob() const;

    /// Same type and same payload. Doubles compare by bit pattern, so -0.0
    /// differs from 0.0 and two NaNs of the same pattern are equal.
    bool operator==(const Value &other) const;
    bool operator!=(const Value &other) const
    {
        return !(*this == other);
    }

  private:
    Type type_;
    bool bool_;
    int64_t int_;
    uint64_t uint_;
    float f32_;
    double f64_;
    std::string str_;
    std::vector<uint8_t> blob_;
};

/// Builds a .trp dictionary from key/value pairs.
class Encoder
{
  public:
    Encoder();
    ~Encoder();

    Encoder(const Encoder &) = delete;
    Encoder &operator=(const Encoder &) = delete;
    Encoder(Encoder &&other) noexcept;
    Encoder &operator=(Encoder &&other) noexcept;

    /// Add a key/value pair. Later adds of the same key replace earlier ones.
    Status add(const std::string &key, const Value &value);

    /// Add a key that may contain any byte, including NUL.
    Status add(const char *key, size_t key_len, const Value &value);

    /// Number of pairs added so far.
    size_t count() const;

    /// Serialize into `out`, replacing its contents.
    Status build(std::vector<uint8_t> &out);

    /// Drop every pair, keeping the encoder usable.
    Status reset();

    /// Underlying C handle (nullable).
    tp_encoder *handle() const
    {
        return handle_;
    }

  private:
    tp_encoder *handle_;
};

/// Read-only view of a .trp buffer.
class Dict
{
  public:
    Dict();

    /// Open `data` immediately; check status() for the outcome. The buffer is
    /// borrowed, not copied, and must outlive the Dict.
    Dict(const uint8_t *data, size_t size);

    ~Dict();

    Dict(const Dict &) = delete;
    Dict &operator=(const Dict &) = delete;
    Dict(Dict &&other) noexcept;
    Dict &operator=(Dict &&other) noexcept;

    /// Open a buffer, replacing any dictionary already held.
    Status open(const uint8_t *data, size_t size);

    /// Status of the last open().
    Status status() const
    {
        return status_;
    }
    bool is_open() const
    {
        return handle_ != nullptr;
    }

    /// Look up a key. Returns Status::NotFound if it is absent.
    Status lookup(const std::string &key, Value &out) const;
    Status lookup(const char *key, size_t key_len, Value &out) const;

    bool contains(const std::string &key) const;

    /// Number of keys.
    size_t size() const;

    /// Underlying C handle (nullable).
    tp_dict *handle() const
    {
        return handle_;
    }

  private:
    tp_dict *handle_;
    Status status_;
};

/**
 * Walks a dictionary's keys in lexicographic byte order.
 *
 * ```
 * triepack::Iterator it(dict);
 * while (it.next())
 *     use(it.key(), it.value());
 * ```
 */
class Iterator
{
  public:
    /// Iterate every key. The dictionary must outlive the iterator.
    explicit Iterator(const Dict &dict);

    /// Iterate only the keys starting with `prefix`.
    Iterator(const Dict &dict, const std::string &prefix);

    ~Iterator();

    Iterator(const Iterator &) = delete;
    Iterator &operator=(const Iterator &) = delete;
    Iterator(Iterator &&other) noexcept;
    Iterator &operator=(Iterator &&other) noexcept;

    /// Advance. Returns false at the end, or on error — check status().
    bool next();

    /// The current key. Valid after next() returned true.
    const std::string &key() const
    {
        return key_;
    }

    /// The current value. Valid after next() returned true.
    const Value &value() const
    {
        return value_;
    }

    /// Status of construction or of the last next(): Ok while entries remain,
    /// Eof once exhausted, or the error that stopped it.
    Status status() const
    {
        return status_;
    }

    /// Restart from the beginning.
    Status reset();

    /// Underlying C handle (nullable).
    tp_iterator *handle() const
    {
        return handle_;
    }

  private:
    tp_iterator *handle_;
    Status status_;
    std::string key_;
    Value value_;
};

} // namespace triepack

#endif // TRIEPACK_TRIEPACK_HPP
