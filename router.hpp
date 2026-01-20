#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "marshalling.hpp"

/**
 * @file router.hpp
 * @brief Protocol frame build/parse and dispatch by msg_id.
 */

namespace umsg
{
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
         * @param msgHash Application-provided hash (passed through to handler).
         * @param payload Payload bytes.
         * @param outFrame Output buffer span (capacity-in / length-out).
         */
        bool buildFrame(uint8_t msgId, uint32_t msgHash, bufferSpan payload, bufferSpan &outFrame)
        {
            if (!outFrame.data)
            {
                return false;
            }
            if (!payload.data && payload.length)
            {
                return false;
            }

            if (payload.length > 0xFFFFu)
            {
                return false;
            }

            const size_t headerSize = kHeaderSize;
            const size_t needed = headerSize + payload.length;
            if (outFrame.length < needed)
            {
                return false;
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
            return true;
        }

        /**
         * @brief Register a handler for a message id.
         *
         * Callback signature: `void (T::*)(bufferSpan payload, uint32_t msgHash)`.
         */
        template <class T>
        bool registerHandler(uint8_t msgId, T *obj, void (T::*method)(bufferSpan payload, uint32_t msgHash))
        {
            if (sizeof(method) > kMaxMemberFnPtrSize)
            {
                return false;
            }

            // Update existing entry if msgId already present.
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (handlers_[i].used && handlers_[i].msgId == msgId)
                {
                    handlers_[i].cbObj = static_cast<void *>(obj);
                    handlers_[i].cbThunk = &memberThunk<T, void (T::*)(bufferSpan, uint32_t)>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return true;
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
                    handlers_[i].cbThunk = &memberThunk<T, void (T::*)(bufferSpan, uint32_t)>;
                    handlers_[i].cbMethodSize = sizeof(method);
                    ::memcpy(handlers_[i].cbMethodBytes, &method, handlers_[i].cbMethodSize);
                    return true;
                }
            }

            return false;
        }

        /**
         * @brief Framer callback: parse and dispatch a complete, CRC-validated frame.
         *
         * Lifetime: frame/payload spans alias Framer-owned storage; only valid during this call.
         */
        void onPacket(bufferSpan frame)
        {
            if (!frame.data)
            {
                return;
            }
            if (frame.length < kHeaderSize)
            {
                return;
            }

            const uint8_t version = frame.data[0];
            if (version != expectedVersion_)
            {
                return;
            }

            const uint8_t msgId = frame.data[1];
            const uint32_t msgHash = read_u32_be(&frame.data[2]);
            const uint16_t payloadLen = read_u16_be(&frame.data[6]);

            if (frame.length != (kHeaderSize + static_cast<size_t>(payloadLen)))
            {
                return;
            }

            const bufferSpan payload{&frame.data[kHeaderSize], static_cast<size_t>(payloadLen)};

            // Find handler
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (!handlers_[i].used || handlers_[i].msgId != msgId)
                {
                    continue;
                }

                if (handlers_[i].cbThunk)
                {
                    handlers_[i].cbThunk(handlers_[i].cbObj,
                                         handlers_[i].cbMethodBytes,
                                         handlers_[i].cbMethodSize,
                                         payload,
                                         msgHash);
                }
                return;
            }
        }

    private:
        static const size_t kHeaderSize = 8;
        static const size_t kMaxMemberFnPtrSize = 16;

        typedef void (*Thunk)(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan payload, uint32_t msgHash);

        template <class T, class M>
        static void memberThunk(void *obj, const uint8_t *methodBytes, size_t methodSize, bufferSpan payload, uint32_t msgHash)
        {
            (void)methodSize;
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            (static_cast<T *>(obj)->*m)(payload, msgHash);
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
