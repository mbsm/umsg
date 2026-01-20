#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file umsg_common.hpp
 * @brief Common types and helpers (no allocation, C++11, freestanding-friendly).
 */

namespace umsg
{
    /** @brief Frame header size in bytes: version(1) + msg_id(1) + msg_hash(4) + len(2). */
    static const size_t kFrameHeaderSize = 8; // version(1) + msg_id(1) + msg_hash(4) + len(2)

    /**
     * @brief Non-owning mutable byte span.
     *
     * Convention in this repo:
     * - For output buffers, `length` is often used as capacity-in / length-out.
     */
    struct bufferSpan
    {
        uint8_t *data;
        size_t length;
    };

    /**
     * @brief COBS worst-case overhead for an input of @p n bytes (delimiter not included).
     *
     * Worst-case expansion is $\lceil n/254 \rceil$.
     */
    inline constexpr size_t cobsMaxOverhead(size_t n)
    {
        return (n + 253u) / 254u;
    }

    /** @brief Compute maximum frame size from a maximum payload size. */
    inline constexpr size_t maxFrameSize(size_t maxPayloadSize)
    {
        return kFrameHeaderSize + maxPayloadSize;
    }

    /**
     * @brief Compute maximum packet size from a maximum payload size.
     *
     * Packet = COBS(frame||crc32) + delimiter(0x00).
     */
    inline constexpr size_t maxPacketSize(size_t maxPayloadSize)
    {
        return (maxFrameSize(maxPayloadSize) + 4u) +
               cobsMaxOverhead(maxFrameSize(maxPayloadSize) + 4u) +
               1u;
    }

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
}
