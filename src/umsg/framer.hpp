#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "marshalling.hpp"
#include "cobs.hpp"
#include "crc32.hpp"

/**
 * @file framer.hpp
 * @brief Byte-stream framing/deframing using COBS + CRC32.
 * @ingroup umsg
 *
 * Wire packet format:
 * - Encoded payload: `COBS(frame || crc32)`
 * - Terminator: `0x00` delimiter
 *
 * The Framer is agnostic to the contents of `frame`; it only validates CRC and emits the
 * decoded frame bytes.
 */

namespace umsg
{

    /**
     * @brief Stateful byte-stream framer/deframer using COBS + CRC32.
     *
     * @tparam MaxPacketSize Maximum encoded packet size (including delimiter) to accept.
     *
     * RX behavior:
     * - Feed bytes via `processByte()`.
     * - When a full packet is received (delimiter encountered), the packet is COBS-decoded
     *   in-place, CRC32 is verified, and the registered callback is invoked with the decoded
     *   frame.
     *
     * @warning The frame span passed to the callback aliases internal RX storage and is only
     *          valid during the callback call stack.
     */
    template <size_t MaxPacketSize>
    class Framer
    {
    public:
        Framer();

        /**
         * @brief Create a wire packet from a frame (append CRC32, COBS encode, append delimiter 0x00).
         *
         * @param frame Input bytes (frame).
         * @param packet Output buffer span (capacity-in / length-out).
         * @return Error::OK on success, Error::InvalidParameter on invalid args or small buffer.
         */
        Error createPacket(bufferSpan frame, bufferSpan &packet);

        /**
         * @brief Process one incoming byte from the transport.
         *
         * @return Error::OK on success, or specific error code (CobsDecodeFailed, CrcMismatch, FrameTooLarge).
         *
         * When a complete packet is received, the registered callback is invoked.
         */
        Error processByte(uint8_t byte);

        /**
         * @brief Register member-function callback invoked with a complete, CRC-validated frame.
         *
         * Lifetime: the frame span aliases internal RX storage and is only valid during the callback.
         *
         * @warning Do not call `processByte()` re-entrantly from inside the callback.
         * @return Error::OK on success, Error::InvalidParameter if member pointer size exceeds storage.
         */
        template <class T>
        Error registerOnPacketCallback(T *obj, Error (T::*method)(bufferSpan frame))
        {
            // Lifetime: the frame span passed to the callback aliases Framer's internal
            // receive buffer; it is only valid for the duration of the callback.
            // Do not call processByte() re-entrantly from inside the callback.
            // Member-function pointer representations are implementation-defined and
            // cannot be cast to/from void*. Store bytes instead (allocation-free).
            static_assert(sizeof(method) <= kMaxMemberFnPtrSize,
                          "Member function pointer exceeds internal storage size (16 bytes).");

            cbObj_ = static_cast<void *>(obj);
            cbThunk_ = &memberThunk<T, Error (T::*)(bufferSpan)>;
            cbMethodSize_ = sizeof(method);
            ::memcpy(cbMethodBytes_, &method, cbMethodSize_);
            return Error::OK;
        }

    private:
        // internal buffer for incoming packets
        uint8_t rxBuffer_[MaxPacketSize];
        size_t rxIndex_ = 0;

        // ---- callback internals ----
        typedef Error (*Thunk)(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan frame);

        void *cbObj_ = 0; // object for member function callback
        uint8_t cbMethodBytes_[kMaxMemberFnPtrSize];
        size_t cbMethodSize_ = 0;
        Thunk cbThunk_ = 0; // thunk to call member function

        template <class T, class M>
        static Error memberThunk(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan frame)
        {
            (void)methodSize;
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            return (static_cast<T *>(obj)->*m)(frame);
        }

        // Call this internally when a packet is complete
        Error emitPacket(bufferSpan frame)
        {
            if (cbThunk_)
            {
                return cbThunk_(cbObj_, cbMethodBytes_, cbMethodSize_, frame);
            }
            return Error::OK;
        }
    };

    template <size_t MaxPacketSize>
    Framer<MaxPacketSize>::Framer() : rxIndex_(0)
    {
    }

    template <size_t MaxPacketSize>
    Error Framer<MaxPacketSize>::createPacket(bufferSpan frame, bufferSpan &packet)
    {
        if (!frame.data || !packet.data)
        {
            return Error::InvalidParameter;
        }

        // packet.length is treated as output buffer capacity on input.
        const size_t outCapacity = packet.length;
        if (outCapacity < 2)
        {
            return Error::InvalidParameter;
        }

        const uint32_t crc = crc32_iso_hdlc(frame.data, frame.length);
        uint8_t crcBytes[4];
        write_u32_be(crcBytes, crc);

        // COBS encode (frame || crcBytes) into packet.data.
        size_t encodedLength = 0;
        if (!cobsEncode2(frame.data, frame.length, crcBytes, 4, packet.data, outCapacity, encodedLength))
        {
            return Error::InvalidParameter;
        }

        // Append delimiter 0x00.
        if (encodedLength >= outCapacity)
        {
            return Error::InvalidParameter;
        }
        packet.data[encodedLength] = 0x00;
        packet.length = encodedLength + 1;
        return Error::OK;
    }

    template <size_t MaxPacketSize>
    Error Framer<MaxPacketSize>::processByte(uint8_t byte)
    {
        // Delimiter (end-of-packet)
        if (byte == 0x00)
        {
            if (rxIndex_ == 0)
            {
                return Error::OK;
            }

            size_t decodedLength = 0;
            const bool ok = cobsDecodeInPlace(rxBuffer_, rxIndex_, decodedLength);
            rxIndex_ = 0;
            if (!ok)
            {
                return Error::CobsDecodeFailed;
            }

            if (decodedLength < 4)
            {
                return Error::FrameHeaderSize;
            }

            const size_t frameLength = decodedLength - 4;
            const uint32_t receivedCrc = read_u32_be(&rxBuffer_[frameLength]);
            const uint32_t computedCrc = crc32_iso_hdlc(rxBuffer_, frameLength);
            if (receivedCrc != computedCrc)
            {
                return Error::CrcMismatch;
            }

            return emitPacket(bufferSpan{rxBuffer_, frameLength});
        }

        // Accumulate encoded data bytes until delimiter.
        if (rxIndex_ >= MaxPacketSize)
        {
            // Overflow; drop packet and resync on next delimiter.
            rxIndex_ = 0;
            return Error::FrameTooLarge;
        }

        rxBuffer_[rxIndex_++] = byte;
        return Error::OK;
    }
} // namespace umsg
