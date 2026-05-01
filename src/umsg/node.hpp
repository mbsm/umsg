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
     * @tparam Transport User type exposing two methods:
     *   - `int read();` returns the next byte (`0..255`) or `-1` when no byte is available.
     *   - `size_t write(const uint8_t* buf, size_t len);` returns the number of bytes
     *     actually written. A short write (`< len`) is reported as `Error::TransportError`.
     * @tparam MaxPayloadSize Maximum payload size for frames built/accepted.
     * @tparam MaxHandlers Maximum number of handlers to register.
     *
     * Lifecycle:
     * - Construct with a transport reference.
     * - Register handlers via `subscribe()`.
     * - Call `poll()` periodically; call `publish()` to transmit.
     *
     * @warning Lifetime requirements:
     * - The `Transport` object must outlive the `Node` (Node holds a reference).
     * - Every subscribed handler object must outlive the `Node`. The Dispatcher
     *   holds a non-owning pointer to each handler; calling `poll()` after the
     *   handler object has been destroyed is undefined behavior. This is a
     *   common pitfall on Arduino: a global `Node` with a handler instance
     *   declared as a local in `setup()` will silently use-after-free.
     *
     * @warning Thread safety: not thread-safe. `poll()` and `publish()` share
     *   internal scratch buffers and the framer/dispatcher state; concurrent
     *   calls from multiple threads must be serialized externally. A handler
     *   invoked from `poll()` may safely call `publish()` (single-threaded
     *   re-entry is fine), but must not call `poll()` recursively.
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
            for (int c = transport_.read(); c >= 0; c = transport_.read())
            {
                ++bytes;
                typename FramerType::Result r = framer_.feed(static_cast<uint8_t>(c));
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

            if (transport_.write(packet.data, packet.length) != packet.length)
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
