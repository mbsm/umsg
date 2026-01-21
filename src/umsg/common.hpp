#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file common.hpp
 * @brief Common types and helpers (no allocation, C++11, freestanding-friendly).
 * @ingroup umsg
 */

namespace umsg
{
    /** @brief Frame header size in bytes: version(1) + msg_id(1) + msg_hash(4) + len(2). */
    static const size_t kFrameHeaderSize = 8;

    /** @brief Maximum size in bytes for a member function pointer (storage limit). */
    static const size_t kMaxMemberFnPtrSize = 16;

    /**
     * @brief Error codes returned by Library functions.
     */
    enum class Error : uint8_t
    {
        OK = 0,

        // --- Framer / Link Layer ---
        FrameTooLarge,    ///< Incoming packet larger than MaxPacketSize
        CobsDecodeFailed, ///< Invalid COBS encoding (e.g. zero inside frame)
        CrcMismatch,      ///< CRC check failed
        FrameHeaderSize,  ///< Decoded frame shorter than minimum header size

        // --- Router / Application Layer ---
        MsgVersionMismatch, ///< Protocol version byte mismatch
        MsgIdUnknown,       ///< No handler registered for this ID
        MsgLengthMismatch,  ///< Payload length header does not match frame size

        // --- Generic ---
        InvalidParameter, ///< Null pointers or invalid arguments
        TransportError    ///< Transport read/write failed
    };

    /**
     * @brief Non-owning mutable byte span.
     *
     * This is a minimal “span-like” type used throughout the library.
     *
     * Conventions:
     * - RX (inputs): `length` is the number of valid bytes.
     * - TX (outputs): `length` is commonly used as capacity-in / length-out
     *   (callers provide buffer capacity; callees overwrite `length` with bytes written).
     *
     * @warning `bufferSpan` does not own its memory. Any span passed to callbacks typically
     *          aliases internal fixed storage and is only valid for the duration of that call.
     */
    struct bufferSpan
    {
        /** @brief Pointer to the first byte (may be null only when length is 0). */
        uint8_t *data;
        /** @brief Byte count (valid length or capacity depending on context). */
        size_t length;
    };

    /**
     * @brief COBS worst-case overhead for an input of @p n bytes (delimiter not included).
     *
     * Worst-case expansion is `ceil(n/254)`.
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
     * Packet = `COBS(frame || crc32) || 0x00`.
     *
     * Notes:
     * - CRC32 is 4 bytes.
     * - COBS worst-case expansion adds `ceil(n/254)` bytes.
     */
    inline constexpr size_t maxPacketSize(size_t maxPayloadSize)
    {
        return (maxFrameSize(maxPayloadSize) + 4u) +
               cobsMaxOverhead(maxFrameSize(maxPayloadSize) + 4u) +
               1u;
    }
}
