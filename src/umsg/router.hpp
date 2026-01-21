#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "marshalling.hpp"

/**
 * @file router.hpp
 * @brief Protocol frame build/parse and dispatch by msg_id.
 * @ingroup umsg
 *
 * Frame format (logical):
 * `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`
 *
 * Notes:
 * - Multi-byte fields are big-endian.
 * - `msg_hash` is an application-defined schema hash; Router treats it as opaque and passes it
 *   through to handlers.
 */

namespace umsg
{
    /**
     * @brief Parse/build protocol frames and dispatch payloads by `msg_id`.
     *
     * @tparam MaxHandlers Maximum number of handler slots.
     *
     * Validation performed on RX (in `onPacket()`):
     * - Checks `version` matches `expectedVersion`.
     * - Checks `len` matches the actual payload size.
     *
     * @note Router assumes incoming frames have already passed CRC validation (typically
     *       performed by `Framer`).
     */
    template <size_t MaxHandlers>
    class Router
    {
    public:
        explicit Router(uint8_t expectedVersion = 1) : expectedVersion_(expectedVersion)
        {
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                handlers_[i].used = false;
            }
        }

        /**
         * @brief Build a protocol frame in network byte order.
         *
         * Format:
         * `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`
         *
         * @param msgId Message id.
         * @param msgHash Application-provided schema hash (passed through to handler).
         * @param payload Payload bytes.
         * @param outFrame Output buffer span (capacity-in / length-out).
         * @return Error::OK on success, Error::InvalidParameter on invalid args or buffer size.
         */
        Error buildFrame(uint8_t msgId, uint32_t msgHash, bufferSpan payload, bufferSpan &outFrame)
        {
            if (!outFrame.data)
            {
                return Error::InvalidParameter;
            }
            if (!payload.data && payload.length)
            {
                return Error::InvalidParameter;
            }

            if (payload.length > 0xFFFFu)
            {
                return Error::InvalidParameter;
            }

            const size_t headerSize = kHeaderSize;
            const size_t needed = headerSize + payload.length;
            if (outFrame.length < needed)
            {
                return Error::InvalidParameter;
            }

            outFrame.data[0] = expectedVersion_;
            outFrame.data[1] = msgId;
            write_u32_be(&outFrame.data[2], msgHash);
            write_u16_be(&outFrame.data[6], static_cast<uint16_t>(payload.length));

            for (size_t i = 0; i < payload.length; ++i)
            {
                outFrame.data[headerSize + i] = payload.data[i];
            }
            outFrame.length = needed;
            return Error::OK;
        }

        /**
         * @brief Register a handler for a message id.
         *
         * Callback signature: `Error (T::*)(bufferSpan payload, uint32_t msgHash)`.
         *
         * If a handler for @p msgId already exists, it is replaced.
         *
         * @note `payload` aliases Framer-owned RX storage; it is only valid during dispatch.
         * @return Error::OK on success, Error::InvalidParameter if full.
         */
        template <class T>
        Error registerHandler(uint8_t msgId, T *obj, Error (T::*method)(bufferSpan payload, uint32_t msgHash))
        {
            static_assert(sizeof(method) <= kMaxMemberFnPtrSize,
                          "Member function pointer exceeds internal storage size (16 bytes).");

            // Update existing entry if msgId already present.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (handlers_[i].used && handlers_[i].msgId == msgId)
                {
                    handlers_[i].cbObj = static_cast<void *>(obj);
                    handlers_[i].cbThunk = &memberThunk<T, Error (T::*)(bufferSpan, uint32_t)>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return Error::OK;
                }
            }

            // Insert into a free slot.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (!handlers_[i].used)
                {
                    handlers_[i].used = true;
                    handlers_[i].msgId = msgId;
                    handlers_[i].cbObj = static_cast<void *>(obj);
                    handlers_[i].cbThunk = &memberThunk<T, Error (T::*)(bufferSpan, uint32_t)>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return Error::OK;
                }
            }

            return Error::InvalidParameter;
        }

        /**
         * @brief Register a type-safe handler for a message id.
         *
         * Callback signature: `Error (T::*)(const Msg& msg)`.
         *
         * Does automatic:
         * 1. Schema hash check (Msg::kMsgHash vs packet hash)
         * 2. Deserialization (Msg::decode)
         *
         * @return Error::OK on success, Error::InvalidParameter if full.
         */
        template <class T, class Msg>
        Error registerHandler(uint8_t msgId, T *obj, Error (T::*method)(const Msg &msg))
        {
            static_assert(sizeof(method) <= kMaxMemberFnPtrSize,
                          "Member function pointer exceeds internal storage size (16 bytes).");

            // Update existing entry if msgId already present.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (handlers_[i].used && handlers_[i].msgId == msgId)
                {
                    handlers_[i].cbObj = static_cast<void *>(obj);
                    handlers_[i].cbThunk = &memberThunkMsg<T, Error (T::*)(const Msg &), Msg>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return Error::OK;
                }
            }

            // Insert into a free slot.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (!handlers_[i].used)
                {
                    handlers_[i].used = true;
                    handlers_[i].msgId = msgId;
                    handlers_[i].cbObj = static_cast<void *>(obj);
                    handlers_[i].cbThunk = &memberThunkMsg<T, Error (T::*)(const Msg &), Msg>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return Error::OK;
                }
            }

            return Error::InvalidParameter;
        }

        /**
         * @brief Framer callback: parse and dispatch a complete, CRC-validated frame.
         *
         * Lifetime: frame/payload spans alias Framer-owned storage; only valid during this call.
         *
         * @return Error status of the routing/dispatch operation.
         */
        Error onPacket(bufferSpan frame)
        {
            if (!frame.data)
            {
                return Error::InvalidParameter;
            }
            if (frame.length < kHeaderSize)
            {
                return Error::FrameHeaderSize;
            }

            const uint8_t version = frame.data[0];
            if (version != expectedVersion_)
            {
                return Error::MsgVersionMismatch;
            }

            const uint8_t msgId = frame.data[1];
            const uint32_t msgHash = read_u32_be(&frame.data[2]);
            const uint16_t payloadLen = read_u16_be(&frame.data[6]);

            if (frame.length != (kHeaderSize + static_cast<size_t>(payloadLen)))
            {
                return Error::MsgLengthMismatch;
            }

            const bufferSpan payload{&frame.data[kHeaderSize], static_cast<size_t>(payloadLen)};

            // Find handler for msgId and invoke it.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (!handlers_[i].used || handlers_[i].msgId != msgId)
                {
                    continue;
                }

                if (handlers_[i].cbThunk)
                {
                    return handlers_[i].cbThunk(handlers_[i].cbObj,
                                         handlers_[i].cbMethodBytes,
                                         handlers_[i].cbMethodSize,
                                         payload,
                                         msgHash);
                }
                return Error::OK;
            }
            return Error::MsgIdUnknown;
        }

    private:
        static const size_t kHeaderSize = 8;
        
        typedef Error (*Thunk)(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan payload, uint32_t msgHash);

        template <class T, class M>
        static Error memberThunk(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan payload, uint32_t msgHash)
        {
            (void)methodSize;
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            return (static_cast<T *>(obj)->*m)(payload, msgHash);
        }

        template <class T, class M, class Msg>
        static Error memberThunkMsg(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan payload, uint32_t msgHash)
        {
            (void)methodSize;
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));

            if (msgHash != Msg::kMsgHash)
            {
                return Error::MsgVersionMismatch;
            }

            Msg msg;
            if (!msg.decode(payload))
            {
                return Error::InvalidParameter;
            }

            return (static_cast<T *>(obj)->*m)(msg);
        }

        struct Handler
        {
            bool used;
            uint8_t msgId;
            void *cbObj;
            uint8_t cbMethodBytes[kMaxMemberFnPtrSize];
            size_t cbMethodSize;
            Thunk cbThunk;
        };

        uint8_t expectedVersion_;
        Handler handlers_[MaxHandlers];
    };
}
