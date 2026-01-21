#pragma once
#include <stddef.h>
#include <stdint.h>

#include "common.hpp"

/**
 * @file marshalling.hpp
 * @brief Canonical (network byte order) read/write helpers and zero-allocation Writer/Reader.
 * @ingroup umsg
 *
 * Marshaling rules (canonical payload encoding):
 * - All multi-byte scalars are encoded big-endian.
 * - `bool` is encoded as 0x00 (false) or 0x01 (true); other values are invalid on decode.
 * - `float`/`double` are transported by IEEE-754 bit pattern (written as u32/u64 big-endian).
 * - Arrays are encoded element-by-element in increasing index order.
 */

namespace umsg
{
    /** @brief Read a big-endian 16-bit value from @p p. */
    inline uint16_t read_u16_be(const uint8_t *p)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                     (static_cast<uint16_t>(p[1])));
    }

    /** @brief Write a big-endian 16-bit value @p v into @p p. */
    inline void write_u16_be(uint8_t *p, uint16_t v)
    {
        p[0] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        p[1] = static_cast<uint8_t>(v & 0xFFu);
    }

    /** @brief Read a big-endian 32-bit value from @p p. */
    inline uint32_t read_u32_be(const uint8_t *p)
    {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               (static_cast<uint32_t>(p[3]));
    }

    /** @brief Write a big-endian 32-bit value @p v into @p p. */
    inline void write_u32_be(uint8_t *p, uint32_t v)
    {
        p[0] = static_cast<uint8_t>((v >> 24) & 0xFFu);
        p[1] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        p[2] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        p[3] = static_cast<uint8_t>(v & 0xFFu);
    }

    /** @brief Read a big-endian 64-bit value from @p p. */
    inline uint64_t read_u64_be(const uint8_t *p)
    {
        return (static_cast<uint64_t>(p[0]) << 56) |
               (static_cast<uint64_t>(p[1]) << 48) |
               (static_cast<uint64_t>(p[2]) << 40) |
               (static_cast<uint64_t>(p[3]) << 32) |
               (static_cast<uint64_t>(p[4]) << 24) |
               (static_cast<uint64_t>(p[5]) << 16) |
               (static_cast<uint64_t>(p[6]) << 8) |
               (static_cast<uint64_t>(p[7]));
    }

    /** @brief Write a big-endian 64-bit value @p v into @p p. */
    inline void write_u64_be(uint8_t *p, uint64_t v)
    {
        p[0] = static_cast<uint8_t>((v >> 56) & 0xFFu);
        p[1] = static_cast<uint8_t>((v >> 48) & 0xFFu);
        p[2] = static_cast<uint8_t>((v >> 40) & 0xFFu);
        p[3] = static_cast<uint8_t>((v >> 32) & 0xFFu);
        p[4] = static_cast<uint8_t>((v >> 24) & 0xFFu);
        p[5] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        p[6] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        p[7] = static_cast<uint8_t>(v & 0xFFu);
    }

    namespace detail
    {
        inline void copy_bytes(void *dst, const void *src, size_t n)
        {
            uint8_t *d = static_cast<uint8_t *>(dst);
            const uint8_t *s = static_cast<const uint8_t *>(src);
            for (size_t i = 0; i < n; ++i)
            {
                d[i] = s[i];
            }
        }

        template <class To, class From>
        inline To bit_cast(const From &from)
        {
            static_assert(sizeof(To) == sizeof(From), "bit_cast size mismatch");
            To to;
            copy_bytes(&to, &from, sizeof(To));
            return to;
        }
    }

    /**
     * @brief Cursor-based writer for canonical (network byte order) encoding.
     *
     * The Writer never allocates. Writes fail with `false` on overflow.
     *
     * @note This type does not modify `bufferSpan::length`; it writes up to `length` bytes.
     */
    class Writer
    {
    public:
        explicit Writer(bufferSpan out) : out_(out), index_(0) {}

        size_t bytesWritten() const { return index_; }

        bool write(uint8_t value)
        {
            if (!ensure(1))
            {
                return false;
            }
            out_.data[index_++] = value;
            return true;
        }

        bool write(int8_t value) { return write(static_cast<uint8_t>(value)); }

        bool write(bool value) { return write(static_cast<uint8_t>(value ? 1u : 0u)); }

        bool write(uint16_t value)
        {
            if (!ensure(2))
            {
                return false;
            }
            write_u16_be(&out_.data[index_], value);
            index_ += 2;
            return true;
        }

        bool write(int16_t value) { return write(static_cast<uint16_t>(value)); }

        bool write(uint32_t value)
        {
            if (!ensure(4))
            {
                return false;
            }
            write_u32_be(&out_.data[index_], value);
            index_ += 4;
            return true;
        }

        bool write(int32_t value) { return write(static_cast<uint32_t>(value)); }

        bool write(uint64_t value)
        {
            if (!ensure(8))
            {
                return false;
            }
            write_u64_be(&out_.data[index_], value);
            index_ += 8;
            return true;
        }

        bool write(int64_t value) { return write(static_cast<uint64_t>(value)); }

        bool write(float value)
        {
            const uint32_t bits = detail::bit_cast<uint32_t>(value);
            return write(bits);
        }

        bool write(double value)
        {
            const uint64_t bits = detail::bit_cast<uint64_t>(value);
            return write(bits);
        }

        template <class T>
        bool writeArray(const T *values, size_t count)
        {
            if (!values && count)
            {
                return false;
            }
            for (size_t i = 0; i < count; ++i)
            {
                if (!write(values[i]))
                {
                    return false;
                }
            }
            return true;
        }

    private:
        bool ensure(size_t n) const
        {
            if (!out_.data)
            {
                return false;
            }
            return (index_ + n) <= out_.length;
        }

        bufferSpan out_;
        size_t index_;
    };

    /**
     * @brief Cursor-based reader for canonical (network byte order) decoding.
     *
     * The Reader never allocates. Reads fail with `false` on underflow or invalid values
     * (e.g. non-0/1 bool encodings).
     */
    class Reader
    {
    public:
        explicit Reader(bufferSpan in) : in_(in), index_(0) {}

        bool fullyConsumed() const { return index_ == in_.length; }

        bool read(uint8_t &out)
        {
            if (!ensure(1))
            {
                return false;
            }
            out = in_.data[index_++];
            return true;
        }

        bool read(int8_t &out)
        {
            uint8_t u;
            if (!read(u))
            {
                return false;
            }
            out = detail::bit_cast<int8_t>(u);
            return true;
        }

        bool read(bool &out)
        {
            uint8_t b;
            if (!read(b))
            {
                return false;
            }
            if (b > 1u)
            {
                return false;
            }
            out = (b == 1u);
            return true;
        }

        bool read(uint16_t &out)
        {
            if (!ensure(2))
            {
                return false;
            }
            out = read_u16_be(&in_.data[index_]);
            index_ += 2;
            return true;
        }

        bool read(int16_t &out)
        {
            uint16_t u;
            if (!read(u))
            {
                return false;
            }
            out = detail::bit_cast<int16_t>(u);
            return true;
        }

        bool read(uint32_t &out)
        {
            if (!ensure(4))
            {
                return false;
            }
            out = read_u32_be(&in_.data[index_]);
            index_ += 4;
            return true;
        }

        bool read(int32_t &out)
        {
            uint32_t u;
            if (!read(u))
            {
                return false;
            }
            out = detail::bit_cast<int32_t>(u);
            return true;
        }

        bool read(uint64_t &out)
        {
            if (!ensure(8))
            {
                return false;
            }
            out = read_u64_be(&in_.data[index_]);
            index_ += 8;
            return true;
        }

        bool read(int64_t &out)
        {
            uint64_t u;
            if (!read(u))
            {
                return false;
            }
            out = detail::bit_cast<int64_t>(u);
            return true;
        }

        bool read(float &out)
        {
            uint32_t bits;
            if (!read(bits))
            {
                return false;
            }
            out = detail::bit_cast<float>(bits);
            return true;
        }

        bool read(double &out)
        {
            uint64_t bits;
            if (!read(bits))
            {
                return false;
            }
            out = detail::bit_cast<double>(bits);
            return true;
        }

        template <class T>
        bool readArray(T *outValues, size_t count)
        {
            if (!outValues && count)
            {
                return false;
            }
            for (size_t i = 0; i < count; ++i)
            {
                if (!read(outValues[i]))
                {
                    return false;
                }
            }
            return true;
        }

    private:
        bool ensure(size_t n) const
        {
            if (!in_.data && in_.length)
            {
                return false;
            }
            return (index_ + n) <= in_.length;
        }

        bufferSpan in_;
        size_t index_;
    };
}
