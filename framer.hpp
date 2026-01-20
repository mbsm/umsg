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
 */

namespace umsg
{

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
         */
        bool createPacket(bufferSpan frame, bufferSpan &packet);

        /**
         * @brief Process one incoming byte from the transport.
         *
         * Returns false on framing/CRC errors. On success, returns true.
         * When a complete packet is received, the registered callback is invoked.
         */
        bool processByte(uint8_t byte);

        /**
         * @brief Register member-function callback invoked with a complete, CRC-validated frame.
         *
         * Lifetime: the frame span aliases internal RX storage and is only valid during the callback.
         */
        template <class T>
        bool registerOnPacketCallback(T *obj, void (T::*method)(bufferSpan frame))
        {
            // Lifetime: the frame span passed to the callback aliases Framer's internal
            // receive buffer; it is only valid for the duration of the callback.
            // Do not call processByte() re-entrantly from inside the callback.
            // Member-function pointer representations are implementation-defined and
            // cannot be cast to/from void*. Store bytes instead (allocation-free).
            if (sizeof(method) > kMaxMemberFnPtrSize)
            {
                return false;
            }

            cbObj_ = static_cast<void *>(obj);
            cbThunk_ = &memberThunk<T, void (T::*)(bufferSpan)>;
            cbMethodSize_ = sizeof(method);
            ::memcpy(cbMethodBytes_, &method, cbMethodSize_);
            return true;
        }

    private:
        // internal buffer for incoming packets
        uint8_t rxBuffer_[MaxPacketSize];
        size_t rxIndex_ = 0;

        // ---- callback internals ----
        static const size_t kMaxMemberFnPtrSize = 16;
        typedef void (*Thunk)(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan frame);

        void *cbObj_ = 0; // object for member function callback
        uint8_t cbMethodBytes_[kMaxMemberFnPtrSize];
        size_t cbMethodSize_ = 0;
        Thunk cbThunk_ = 0; // thunk to call member function

        template <class T, class M>
        static void memberThunk(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan frame)
        {
            (void)methodSize;
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            (static_cast<T *>(obj)->*m)(frame);
        }

        // Call this internally when a packet is complete
        void emitPacket(bufferSpan frame)
        {
            if (cbThunk_)
            {
                cbThunk_(cbObj_, cbMethodBytes_, cbMethodSize_, frame);
            }
        }
    };

    template <size_t MaxPacketSize>
    Framer<MaxPacketSize>::Framer() : rxIndex_(0)
    {
    }

    template <size_t MaxPacketSize>
    bool Framer<MaxPacketSize>::createPacket(bufferSpan frame, bufferSpan &packet)
    {
        if (!frame.data || !packet.data)
        {
            return false;
        }

        // packet.length is treated as output buffer capacity on input.
        const size_t outCapacity = packet.length;
        if (outCapacity < 2)
        {
            return false;
        }

        const uint32_t crc = crc32_iso_hdlc(frame.data, frame.length);
        uint8_t crcBytes[4];
        write_u32_be(crcBytes, crc);

        // COBS encode (frame || crcBytes) into packet.data.
        size_t encodedLength = 0;
        if (!cobsEncode2(frame.data, frame.length, crcBytes, 4, packet.data, outCapacity, encodedLength))
        {
            return false;
        }

        // Append delimiter 0x00.
        if (encodedLength >= outCapacity)
        {
            return false;
        }
        packet.data[encodedLength] = 0x00;
        packet.length = encodedLength + 1;
        return true;
    }

    template <size_t MaxPacketSize>
    bool Framer<MaxPacketSize>::processByte(uint8_t byte)
    {
        // Delimiter (end-of-packet)
        if (byte == 0x00)
        {
            if (rxIndex_ == 0)
            {
                return true;
            }

            size_t decodedLength = 0;
            const bool ok = cobsDecodeInPlace(rxBuffer_, rxIndex_, decodedLength);
            rxIndex_ = 0;
            if (!ok)
            {
                return false;
            }

            if (decodedLength < 4)
            {
                return false;
            }

            const size_t frameLength = decodedLength - 4;
            const uint32_t receivedCrc = read_u32_be(&rxBuffer_[frameLength]);
            const uint32_t computedCrc = crc32_iso_hdlc(rxBuffer_, frameLength);
            if (receivedCrc != computedCrc)
            {
                return false;
            }

            emitPacket(bufferSpan{rxBuffer_, frameLength});
            return true;
        }

        // Accumulate encoded data bytes until delimiter.
        if (rxIndex_ >= MaxPacketSize)
        {
            // Overflow; drop packet and resync on next delimiter.
            rxIndex_ = 0;
            return false;
        }

        rxBuffer_[rxIndex_++] = byte;
        return true;
    }
} // namespace umsg
