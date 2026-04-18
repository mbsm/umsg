#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.hpp"
#include "marshalling.hpp"

/**
 * @file protocol.hpp
 * @brief Stateless codec for the inner protocol frame.
 * @ingroup umsg
 *
 * Frame format (big-endian multi-byte fields):
 *   `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`
 *
 * These helpers are pure functions; they own no state and allocate nothing.
 */

namespace umsg
{
    namespace protocol
    {
        /** @brief Maximum payload length (uint16_t bound). */
        static const size_t kMaxPayloadLength = 0xFFFFu;

        /** @brief Parsed frame header. */
        struct Header
        {
            uint8_t version;
            uint8_t msgId;
            uint32_t msgHash;
            uint16_t payloadLength;
        };

        /**
         * @brief Build a frame (header + payload) into @p outFrame.
         *
         * @param version Protocol version byte.
         * @param msgId Message id.
         * @param msgHash Application-defined schema hash.
         * @param payload Payload bytes (may be empty).
         * @param outFrame Output buffer; `length` used as capacity on input, set to
         *                 bytes written on success.
         * @return Error::OK on success.
         */
        inline Error encodeFrame(uint8_t version,
                                 uint8_t msgId,
                                 uint32_t msgHash,
                                 ByteSpan payload,
                                 ByteSpan &outFrame)
        {
            if (!outFrame.data)
            {
                return Error::InvalidArgument;
            }
            if (!payload.data && payload.length)
            {
                return Error::InvalidArgument;
            }
            if (payload.length > kMaxPayloadLength)
            {
                return Error::InvalidArgument;
            }

            const size_t needed = kFrameHeaderSize + payload.length;
            if (outFrame.length < needed)
            {
                return Error::InvalidArgument;
            }

            outFrame.data[0] = version;
            outFrame.data[1] = msgId;
            write_u32_be(&outFrame.data[2], msgHash);
            write_u16_be(&outFrame.data[6], static_cast<uint16_t>(payload.length));
            if (payload.length > 0)
            {
                ::memcpy(&outFrame.data[kFrameHeaderSize], payload.data, payload.length);
            }
            outFrame.length = needed;
            return Error::OK;
        }

        /**
         * @brief Parse a frame into a header and payload span.
         *
         * The @p payload span aliases @p frame's storage.
         *
         * @return Error::OK on success; Error::FrameTooShort if truncated;
         *         Error::LengthMismatch if the length field disagrees with @p frame.length.
         */
        inline Error decodeFrame(ByteSpan frame, Header &outHeader, ByteSpan &outPayload)
        {
            if (!frame.data)
            {
                return Error::InvalidArgument;
            }
            if (frame.length < kFrameHeaderSize)
            {
                return Error::FrameTooShort;
            }

            outHeader.version = frame.data[0];
            outHeader.msgId = frame.data[1];
            outHeader.msgHash = read_u32_be(&frame.data[2]);
            outHeader.payloadLength = read_u16_be(&frame.data[6]);

            if (frame.length != (kFrameHeaderSize + static_cast<size_t>(outHeader.payloadLength)))
            {
                return Error::LengthMismatch;
            }

            outPayload.data = &frame.data[kFrameHeaderSize];
            outPayload.length = outHeader.payloadLength;
            return Error::OK;
        }
    }
}
