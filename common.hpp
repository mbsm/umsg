#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file common.hpp
 * @brief Common types and helpers (no allocation, C++11, freestanding-friendly).
 */

namespace umsg
{
    /** @brief Frame header size in bytes: version(1) + msg_id(1) + msg_hash(4) + len(2). */
    static const size_t kFrameHeaderSize = 8;

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
}
