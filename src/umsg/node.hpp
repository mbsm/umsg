#pragma once
#include <stddef.h>
#include <stdint.h>

#include "common.hpp"
#include "framer.hpp"
#include "router.hpp"

/**
 * @file node.hpp
 * @brief High-level integration: Framer + Router + user transport.
 * @ingroup umsg
 *
 * `umsg::Node` is the primary integration object (and typical entry point) for application usage:
 *
 * - RX: drain bytes from a transport, feed them into `Framer::processByte()`, and dispatch
 *   complete frames via `Router`.
 * - TX: build a frame via `Router::buildFrame()`, wrap it into a COBS+CRC32 packet via
 *   `Framer::createPacket()`, then write it to the transport.
 *
 * This library is header-only, C++11, freestanding-friendly, and does not allocate.
 */

namespace umsg
{
    /**
     * @brief Integrates `Framer + Router` with a user-provided transport.
     *
     * @tparam Transport User type implementing the transport concept.
     * @tparam MaxPayloadSize Maximum payload size (bytes) for frames built/accepted.
     * @tparam MaxHandlers Maximum number of Router handlers to register.
     *
     * Transport concept (minimal):
     * - `bool read(uint8_t& byte)`
     *   - Non-blocking.
     *   - Returns `true` and sets `byte` when a byte is available.
     *   - Returns `false` when no more bytes are available right now.
     * - `bool write(const uint8_t* data, size_t length)`
     *   - Returns `true` if all bytes were written.
     *
     * Lifecycle:
     * - Construct a `Node`, then check `ok()`.
     * - Register handlers using `registerHandler()`.
     * - Periodically call `poll()` to receive/dispatch.
     * - Use `publish()` to transmit.
     *
     * @note `ok()` can be false if the underlying Framer callback wiring fails (the Framer
     *       uses a fixed-size buffer to store a member-function pointer representation).
     *
     * Buffer lifetime / reentrancy:
     * - RX handler arguments (`bufferSpan`) alias the Framer’s internal RX buffer.
     *   They are only valid during the callback call stack.
     * - Do not call `poll()` (or `Framer::processByte()`) re-entrantly from inside a handler.
     * - TX uses internal fixed-size buffers; `publish()` is not re-entrant.
     */
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
            wired_ = (framer_.registerOnPacketCallback(&router_, &RouterType::onPacket) == Error::OK);
        }

        FramerType &framer() { return framer_; }
        RouterType &router() { return router_; }

        /** @brief True if Framer->Router callback wiring succeeded. */
        bool ok() const { return wired_; }

        /**
         * @brief Drain available bytes from the transport and feed them into the framer.
         *
         * This method is intended to be called periodically (e.g., in your main loop).
         *
         * @return Number of errors encountered (framing, CRC, or dispatch).
         */
        size_t poll()
        {
            if (!wired_)
            {
                return 0;
            }

            size_t errors = 0;
            uint8_t byte = 0;
            while (transport_.read(byte))
            {
                if (framer_.processByte(byte) != Error::OK)
                {
                    errors++;
                }
            }
            return errors;
        }

        /**
         * @brief Convenience wrapper for Router handler registration.
         *
         * Handler signature:
         * - `Error (T::*)(umsg::bufferSpan payload, uint32_t msgHash)`
         *
         * @note `payload` aliases internal RX storage; copy bytes out if you need to retain them.
         */
        template <class T>
        Error registerHandler(uint8_t msgId, T *obj, Error (T::*method)(bufferSpan payload, uint32_t msgHash))
        {
            return router_.registerHandler(msgId, obj, method);
        }

        /**
         * @brief Convenience wrapper for type-safe Router handler registration.
         *
         * Handler signature: `Error (T::*)(const Msg& msg)`.
         * Performs automatic schema check and deserialization.
         */
        template <class T, class Msg>
        Error registerHandler(uint8_t msgId, T *obj, Error (T::*method)(const Msg &msg))
        {
            return router_.registerHandler(msgId, obj, method);
        }

        /**
         * @brief Build frame -> packet and write it to the transport.
         *
         * @return Error::OK on success, or specific error on failure.
         */
        Error publish(uint8_t msgId, uint32_t msgHash, bufferSpan payload)
        {
            if (!wired_)
            {
                return Error::InvalidParameter;
            }

            bufferSpan frame{txFrame_, kMaxFrameSize};
            Error err = router_.buildFrame(msgId, msgHash, payload, frame);
            if (err != Error::OK)
            {
                return err;
            }

            bufferSpan packet{txPacket_, kMaxPacketSize};
            err = framer_.createPacket(frame, packet);
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
         * @brief Publish a typed message with an explicit msgId.
         *
         * Requirements on Msg:
         * - static const uint32_t kMsgHash
         * - bool encode(umsg::bufferSpan& payload) const
         *
         * @note The message is encoded into an internal scratch buffer and then published.
         *       This function is not re-entrant.
         */
        template <class Msg>
        Error publish(uint8_t msgId, const Msg &msg)
        {
            bufferSpan payload{txPacket_, MaxPayloadSize};
            if (!msg.encode(payload))
            {
                return Error::InvalidParameter;
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
