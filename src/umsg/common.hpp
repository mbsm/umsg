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
    /** @brief Maximum size in bytes for a member function pointer (storage limit). */
    static const size_t kMaxMemberFnPtrSize = 16;

    /**
     * @brief Error codes returned by library functions.
     *
     * Naming: `<Subject><Problem>`; `Invalid` for format/structure violations,
     * `Mismatch` for comparison failures against an expected value.
     */
    enum class Error : uint8_t
    {
        OK = 0,

        // --- Framer / Link Layer ---
        FrameOverflow,    ///< Incoming packet larger than MaxPacketSize
        CobsInvalid,      ///< Invalid COBS encoding (e.g. zero inside frame)
        CrcInvalid,       ///< CRC check failed
        FrameTooShort,    ///< Decoded frame shorter than minimum header size

        // --- Protocol / Dispatcher ---
        VersionMismatch,  ///< Protocol version byte mismatch
        HashMismatch,     ///< Typed handler: schema hash mismatch
        LengthMismatch,   ///< Payload length header does not match frame size
        HandlerNotFound,  ///< No handler registered for this msg id

        // --- Generic ---
        InvalidArgument,  ///< Null pointers or invalid arguments
        TransportError    ///< Transport read/write failed
    };

    /**
     * @brief Non-owning mutable byte span.
     *
     * Conventions:
     * - RX (inputs): `length` is the number of valid bytes.
     * - TX (outputs): `length` is capacity-in / length-out (callers provide
     *   buffer capacity; callees overwrite `length` with bytes written).
     *
     * @warning `ByteSpan` does not own its memory. Any span passed to callbacks
     *          typically aliases internal fixed storage and is only valid for
     *          the duration of that call.
     */
    struct ByteSpan
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

    /** @brief Wire frame header size: `version(1) | msg_id(1) | msg_hash(4) | len(2)`. */
    static const size_t kFrameHeaderSize = 8;

    /** @brief Compute maximum frame size (header + payload) for a given payload size. */
    inline constexpr size_t maxFrameSize(size_t maxPayloadSize)
    {
        return kFrameHeaderSize + maxPayloadSize;
    }

    /**
     * @brief Compute maximum wire packet size from a maximum payload size.
     *
     * Packet = `COBS(frame || crc32) || 0x00`.
     */
    inline constexpr size_t maxPacketSize(size_t maxPayloadSize)
    {
        return (maxFrameSize(maxPayloadSize) + 4u) +
               cobsMaxOverhead(maxFrameSize(maxPayloadSize) + 4u) +
               1u;
    }
}
