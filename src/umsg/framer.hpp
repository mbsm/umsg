#pragma once
#include <stddef.h>
#include <stdint.h>

#include "common.hpp"
#include "cobs.hpp"
#include "crc32.hpp"
#include "marshalling.hpp"

/**
 * @file framer.hpp
 * @brief Byte-stream framing/deframing using COBS + CRC32.
 * @ingroup umsg
 *
 * Wire packet format:
 *   `COBS(frame || crc32) || 0x00`
 *
 * The Framer is agnostic to the contents of `frame`; it only validates CRC and
 * returns the decoded frame bytes.
 */

namespace umsg
{
    /**
     * @brief Stateful byte-stream framer/deframer using COBS + CRC32.
     *
     * @tparam MaxPacketSize Maximum encoded packet size (including delimiter) to accept.
     *
     * RX: feed bytes with `feed()`; when a complete packet is received, the returned
     * `Result` has `complete == true` and `frame` aliases internal RX storage.
     *
     * @warning The `frame` span returned by `feed()` aliases internal RX storage and
     *          is only valid until the next call to `feed()`.
     */
    template <size_t MaxPacketSize>
    class Framer
    {
    public:
        /** @brief Result of feeding one byte. */
        struct Result
        {
            Error status;   ///< Outcome for this byte.
            bool complete;  ///< True when a full frame has been decoded into `frame`.
            ByteSpan frame; ///< Valid only when `complete == true`.
        };

        Framer() : rxIndex_(0), resyncing_(false) {}

        /**
         * @brief Encode a frame into a wire packet (append CRC32, COBS encode, append `0x00`).
         *
         * @param frame Input bytes.
         * @param packet Output buffer; `length` used as capacity on input, set to
         *               bytes written on success.
         */
        Error encode(ByteSpan frame, ByteSpan &packet)
        {
            if (!frame.data || !packet.data)
            {
                return Error::InvalidArgument;
            }

            const size_t outCapacity = packet.length;
            if (outCapacity < 2)
            {
                return Error::InvalidArgument;
            }

            const uint32_t crc = crc32_iso_hdlc(frame.data, frame.length);
            uint8_t crcBytes[4];
            write_u32_be(crcBytes, crc);

            size_t encodedLength = 0;
            if (!cobsEncode2(frame.data, frame.length, crcBytes, 4,
                             packet.data, outCapacity, encodedLength))
            {
                return Error::InvalidArgument;
            }

            if (encodedLength >= outCapacity)
            {
                return Error::InvalidArgument;
            }
            packet.data[encodedLength] = 0x00;
            packet.length = encodedLength + 1;
            return Error::OK;
        }

        /**
         * @brief Feed one incoming byte from the transport.
         *
         * @return `Result{status, complete, frame}`. `complete` is true when a full
         *         frame has been decoded; `frame` then aliases internal RX storage.
         */
        Result feed(uint8_t byte)
        {
            Result r{Error::OK, false, ByteSpan{nullptr, 0}};

            if (byte == 0x00)
            {
                const bool wasResyncing = resyncing_;
                resyncing_ = false;
                const size_t encodedLen = rxIndex_;
                rxIndex_ = 0;

                if (encodedLen == 0)
                {
                    return r;
                }
                if (wasResyncing)
                {
                    // Dropped the oversized packet; this delimiter restarts framing.
                    return r;
                }

                size_t decodedLength = 0;
                if (!cobsDecodeInPlace(rxBuffer_, encodedLen, decodedLength))
                {
                    r.status = Error::CobsInvalid;
                    return r;
                }
                if (decodedLength < 4)
                {
                    r.status = Error::FrameTooShort;
                    return r;
                }

                const size_t frameLength = decodedLength - 4;
                const uint32_t receivedCrc = read_u32_be(&rxBuffer_[frameLength]);
                const uint32_t computedCrc = crc32_iso_hdlc(rxBuffer_, frameLength);
                if (receivedCrc != computedCrc)
                {
                    r.status = Error::CrcInvalid;
                    return r;
                }

                r.complete = true;
                r.frame = ByteSpan{rxBuffer_, frameLength};
                return r;
            }

            if (resyncing_)
            {
                return r;
            }

            if (rxIndex_ >= MaxPacketSize)
            {
                rxIndex_ = 0;
                resyncing_ = true;
                r.status = Error::FrameOverflow;
                return r;
            }

            rxBuffer_[rxIndex_++] = byte;
            return r;
        }

    private:
        uint8_t rxBuffer_[MaxPacketSize];
        size_t rxIndex_;
        bool resyncing_;
    };
}
