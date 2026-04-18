#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.hpp"

/**
 * @file dispatcher.hpp
 * @brief Handler table keyed by `msgId` (no frame parsing; see `protocol.hpp`).
 * @ingroup umsg
 */

namespace umsg
{
    /**
     * @brief Handler table keyed by `msgId`; dispatches to a registered member function.
     *
     * @tparam MaxHandlers Fixed handler-table capacity.
     *
     * Two registration forms:
     * - Raw: `Error (T::*)(ByteSpan payload, uint32_t msgHash)` — caller owns hash checking.
     * - Typed: `Error (T::*)(const Msg& msg)` — auto-checks `Msg::kMsgHash` and calls `Msg::decode`.
     *
     * @note Payload spans alias the caller's buffer and are only valid for the dispatch call.
     */
    template <size_t MaxHandlers>
    class Dispatcher
    {
    public:
        Dispatcher()
        {
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                handlers_[i].used = false;
            }
        }

        /**
         * @brief Register a raw handler for @p msgId (replaces any existing entry).
         * @return Error::OK, or Error::InvalidArgument if the table is full.
         */
        template <class T>
        Error registerHandler(uint8_t msgId, T *obj,
                              Error (T::*method)(ByteSpan payload, uint32_t msgHash))
        {
            static_assert(sizeof(method) <= kMaxMemberFnPtrSize,
                          "Member function pointer exceeds internal storage size.");
            return install(msgId, static_cast<void *>(obj),
                           &rawThunk<T, Error (T::*)(ByteSpan, uint32_t)>,
                           &method, sizeof(method));
        }

        /**
         * @brief Register a typed handler for @p msgId (auto-checks `Msg::kMsgHash`
         *        and calls `Msg::decode`).
         * @return Error::OK, or Error::InvalidArgument if the table is full.
         */
        template <class T, class Msg>
        Error registerHandler(uint8_t msgId, T *obj,
                              Error (T::*method)(const Msg &msg))
        {
            static_assert(sizeof(method) <= kMaxMemberFnPtrSize,
                          "Member function pointer exceeds internal storage size.");
            return install(msgId, static_cast<void *>(obj),
                           &typedThunk<T, Error (T::*)(const Msg &), Msg>,
                           &method, sizeof(method));
        }

        /**
         * @brief Dispatch a payload to the handler registered for @p msgId.
         * @return Error::HandlerNotFound if no handler is registered.
         */
        Error dispatch(uint8_t msgId, uint32_t msgHash, ByteSpan payload)
        {
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                const Slot &s = handlers_[i];
                if (!s.used || s.msgId != msgId)
                {
                    continue;
                }
                return s.thunk(s.obj, s.methodBytes, payload, msgHash);
            }
            return Error::HandlerNotFound;
        }

    private:
        typedef Error (*Thunk)(void *obj, const uint8_t *methodBytes,
                               ByteSpan payload, uint32_t msgHash);

        struct Slot
        {
            bool used;
            uint8_t msgId;
            void *obj;
            uint8_t methodBytes[kMaxMemberFnPtrSize];
            Thunk thunk;
        };

        Error install(uint8_t msgId, void *obj, Thunk thunk,
                      const void *methodPtr, size_t methodSize)
        {
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (handlers_[i].used && handlers_[i].msgId == msgId)
                {
                    writeSlot(handlers_[i], msgId, obj, thunk, methodPtr, methodSize);
                    return Error::OK;
                }
            }
            for (size_t i = 0; i < MaxHandlers; ++i)
            {
                if (!handlers_[i].used)
                {
                    writeSlot(handlers_[i], msgId, obj, thunk, methodPtr, methodSize);
                    return Error::OK;
                }
            }
            return Error::InvalidArgument;
        }

        static void writeSlot(Slot &s, uint8_t msgId, void *obj, Thunk thunk,
                              const void *methodPtr, size_t methodSize)
        {
            s.used = true;
            s.msgId = msgId;
            s.obj = obj;
            s.thunk = thunk;
            ::memcpy(s.methodBytes, methodPtr, methodSize);
        }

        template <class T, class M>
        static Error rawThunk(void *obj, const uint8_t *methodBytes,
                              ByteSpan payload, uint32_t msgHash)
        {
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            return (static_cast<T *>(obj)->*m)(payload, msgHash);
        }

        template <class T, class M, class Msg>
        static Error typedThunk(void *obj, const uint8_t *methodBytes,
                                ByteSpan payload, uint32_t msgHash)
        {
            if (msgHash != Msg::kMsgHash)
            {
                return Error::HashMismatch;
            }
            Msg msg;
            if (!msg.decode(payload))
            {
                return Error::InvalidArgument;
            }
            M m;
            ::memcpy(&m, methodBytes, sizeof(m));
            return (static_cast<T *>(obj)->*m)(msg);
        }

        Slot handlers_[MaxHandlers];
    };
}
