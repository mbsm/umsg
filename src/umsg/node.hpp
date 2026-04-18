#pragma once
#include <stddef.h>
#include <stdint.h>

#include "common.hpp"
#include "dispatcher.hpp"
#include "framer.hpp"
#include "protocol.hpp"

/**
 * @file node.hpp
 * @brief High-level integration: transport + Framer + protocol codec + Dispatcher.
 * @ingroup umsg
 */

namespace umsg
{
    /**
     * @brief Integrates a transport, a `Framer`, and a `Dispatcher`.
     *
     * @tparam Transport User type with `bool read(uint8_t&)` and `bool write(const uint8_t*, size_t)`.
     * @tparam MaxPayloadSize Maximum payload size for frames built/accepted.
     * @tparam MaxHandlers Maximum number of handlers to register.
     *
     * Lifecycle:
     * - Construct with a transport reference.
     * - Register handlers via `on()`.
     * - Call `poll()` periodically; call `publish()` to transmit.
     *
     * Reentrancy:
     * - Do not call `poll()` recursively from a handler.
     * - `publish()` is not re-entrant (uses internal fixed-size scratch).
     */
    template <class Transport, size_t MaxPayloadSize, size_t MaxHandlers>
    class Node
    {
    public:
        static const size_t kMaxFrameSize = umsg::maxFrameSize(MaxPayloadSize);
        static const size_t kMaxPacketSize = umsg::maxPacketSize(MaxPayloadSize);

        typedef umsg::Framer<kMaxPacketSize> FramerType;
        typedef umsg::Dispatcher<MaxHandlers> DispatcherType;

        explicit Node(Transport &transport, uint8_t expectedVersion = 1)
            : transport_(transport), expectedVersion_(expectedVersion) {}

        /**
         * @brief Subscribe a raw handler to @p msgId.
         *
         * Only one handler per `msgId`; re-subscribing replaces the previous handler.
         */
        template <class T>
        Error subscribe(uint8_t msgId, T *obj,
                        Error (T::*method)(ByteSpan payload, uint32_t msgHash))
        {
            return dispatcher_.registerHandler(msgId, obj, method);
        }

        /**
         * @brief Subscribe a typed handler to @p msgId (auto-checks `Msg::kMsgHash`
         *        and calls `Msg::decode`).
         *
         * Only one handler per `msgId`; re-subscribing replaces the previous handler.
         */
        template <class T, class Msg>
        Error subscribe(uint8_t msgId, T *obj, Error (T::*method)(const Msg &msg))
        {
            return dispatcher_.registerHandler(msgId, obj, method);
        }

        /**
         * @brief Drain available bytes from the transport and dispatch complete frames.
         *
         * Byte-level framing errors (`CobsInvalid`, `CrcInvalid`, `FrameOverflow`),
         * protocol errors (`VersionMismatch`, `LengthMismatch`, `FrameTooShort`), and
         * handler return values are intentionally discarded here — the handler is the
         * right place to observe message-level outcomes, and framing-level errors only
         * matter to application code in aggregate (see `Framer::feed` if you need
         * per-byte diagnostics).
         *
         * @return Number of bytes consumed from the transport this call.
         */
        size_t poll()
        {
            size_t bytes = 0;
            uint8_t byte = 0;
            while (transport_.read(byte))
            {
                ++bytes;
                typename FramerType::Result r = framer_.feed(byte);
                if (r.complete)
                {
                    protocol::Header h;
                    ByteSpan payload;
                    if (protocol::decodeFrame(r.frame, h, payload) != Error::OK)
                    {
                        continue;
                    }
                    if (h.version != expectedVersion_)
                    {
                        continue;
                    }
                    dispatcher_.dispatch(h.msgId, h.msgHash, payload);
                }
            }
            return bytes;
        }

        /** @brief Build a frame and transmit it. */
        Error publish(uint8_t msgId, uint32_t msgHash, ByteSpan payload)
        {
            ByteSpan frame{txFrame_, kMaxFrameSize};
            Error err = protocol::encodeFrame(expectedVersion_, msgId, msgHash, payload, frame);
            if (err != Error::OK)
            {
                return err;
            }

            ByteSpan packet{txPacket_, kMaxPacketSize};
            err = framer_.encode(frame, packet);
            if (err != Error::OK)
            {
                return err;
            }

            if (!transport_.write(packet.data, packet.length))
            {
                return Error::TransportError;
            }
            return Error::OK;
        }

        /**
         * @brief Publish a typed message.
         *
         * Requires `Msg::kMsgHash` and `bool Msg::encode(ByteSpan& payload) const`.
         */
        template <class Msg>
        Error publish(uint8_t msgId, const Msg &msg)
        {
            ByteSpan payload{txEncode_, MaxPayloadSize};
            if (!msg.encode(payload))
            {
                return Error::InvalidArgument;
            }
            return publish(msgId, Msg::kMsgHash, payload);
        }

    private:
        Transport &transport_;
        FramerType framer_;
        DispatcherType dispatcher_;
        uint8_t expectedVersion_;

        uint8_t txEncode_[MaxPayloadSize];
        uint8_t txFrame_[kMaxFrameSize];
        uint8_t txPacket_[kMaxPacketSize];
    };
}
