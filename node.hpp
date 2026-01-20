#pragma once
#include <stddef.h>
#include <stdint.h>

#include "common.hpp"
#include "framer.hpp"
#include "router.hpp"

/**
 * @file node.hpp
 * @brief High-level integration: Framer + Router + user transport.
 */

namespace umsg
{
    // High-level wrapper that combines a Framer + Router with a user-provided transport.
    //
    // Transport requirements (minimal):
    // - bool read(uint8_t& byte)
    //     - returns true and sets byte when a byte is available
    //     - returns false when no more bytes are available (non-blocking)
    // - bool write(const uint8_t* data, size_t length)
    //     - returns true if all bytes were written
    //
    // Buffer ownership:
    // - RX: Router callbacks (frame/payload spans) alias Framer internal storage; valid only
    //       during the callback call stack.
    // - TX: Node uses internal TX buffers (no heap); publish() copies/encodes into those buffers
    //       before calling transport.write(...).
    template <class Transport, size_t MaxPayloadSize, size_t MaxHandlers>
    class Node
    {
    public:
        static const size_t kMaxFrameSize = umsg::maxFrameSize(MaxPayloadSize);
        static const size_t kMaxPacketSize = umsg::maxPacketSize(MaxPayloadSize);

        typedef umsg::Framer<kMaxPacketSize> FramerType;
        typedef umsg::Router<MaxHandlers> RouterType;

        explicit Node(Transport &transport, uint8_t expectedVersion = 1)
            : transport_(transport), router_(expectedVersion)
        {
            // Wire framer -> router.
            wired_ = framer_.registerOnPacketCallback(&router_, &RouterType::onPacket);
        }

        FramerType &framer() { return framer_; }
        RouterType &router() { return router_; }

        /** @brief True if Framer->Router callback wiring succeeded. */
        bool ok() const { return wired_; }

        /**
         * @brief Drain available bytes from the transport and feed them into the framer.
         * @return false on framing/CRC errors or if not wired.
         */
        bool poll()
        {
            if (!wired_)
            {
                return false;
            }

            uint8_t byte = 0;
            while (transport_.read(byte))
            {
                if (!framer_.processByte(byte))
                {
                    return false;
                }
            }
            return true;
        }

        /** @brief Convenience wrapper for Router handler registration. */
        template <class T>
        bool registerHandler(uint8_t msgId, T *obj, void (T::*method)(bufferSpan payload, uint32_t msgHash))
        {
            return router_.registerHandler(msgId, obj, method);
        }

        /**
         * @brief Build frame -> packet and write it to the transport.
         * @return false if build/encode/write fails or if not wired.
         */
        bool publish(uint8_t msgId, uint32_t msgHash, bufferSpan payload)
        {
            if (!wired_)
            {
                return false;
            }

            bufferSpan frame{txFrame_, kMaxFrameSize};
            if (!router_.buildFrame(msgId, msgHash, payload, frame))
            {
                return false;
            }

            bufferSpan packet{txPacket_, kMaxPacketSize};
            if (!framer_.createPacket(frame, packet))
            {
                return false;
            }

            return transport_.write(packet.data, packet.length);
        }

        /**
         * @brief Publish a typed message with an explicit msgId.
         *
         * Requirements on Msg:
         * - static const uint32_t kMsgHash
         * - bool encode(umsg::bufferSpan& payload) const
         */
        template <class Msg>
        bool publish(uint8_t msgId, const Msg &msg)
        {
            bufferSpan payload{txPacket_, MaxPayloadSize};
            if (!msg.encode(payload))
            {
                return false;
            }
            return publish(msgId, Msg::kMsgHash, payload);
        }

    private:
        Transport &transport_;
        FramerType framer_;
        RouterType router_;

        bool wired_;

        uint8_t txFrame_[kMaxFrameSize];
        uint8_t txPacket_[kMaxPacketSize];
    };
}
