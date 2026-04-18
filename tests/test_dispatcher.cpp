#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/dispatcher.hpp>
#include <umsg/protocol.hpp>
#include <umsg/marshalling.hpp>

namespace
{
    struct HandlerCapture
    {
        bool called;
        uint32_t msgHash;
        uint8_t payload[64];
        size_t payloadLen;

        HandlerCapture() : called(false), msgHash(0), payloadLen(0) {}

        umsg::Error onMsg(umsg::ByteSpan payloadSpan, uint32_t hash)
        {
            called = true;
            msgHash = hash;
            payloadLen = payloadSpan.length;
            if (payloadLen > sizeof(payload))
            {
                payloadLen = sizeof(payload);
            }
            if (payloadLen)
            {
                ::memcpy(payload, payloadSpan.data, payloadLen);
            }
            return umsg::Error::OK;
        }
    };

    void test_dispatch_by_msg_id(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "dispatcher: dispatch() routes by msg_id");
        umsg::Dispatcher<4> d;
        HandlerCapture cap;

        UMSG_TEST_EXPECT_TRUE(ctx, d.registerHandler(7, &cap, &HandlerCapture::onMsg) == umsg::Error::OK);

        uint8_t payloadBytes[] = {1, 2, 3, 4};
        UMSG_TEST_EXPECT_TRUE(ctx,
            d.dispatch(7, 0x12345678u, umsg::ByteSpan{payloadBytes, sizeof(payloadBytes)}) == umsg::Error::OK);

        UMSG_TEST_EXPECT_TRUE(ctx, cap.called);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x12345678u, cap.msgHash);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(payloadBytes), cap.payloadLen);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, payloadBytes, cap.payload, cap.payloadLen);
    }

    void test_dispatch_unknown_id(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "dispatcher: dispatch() returns HandlerNotFound for unknown msg_id");
        umsg::Dispatcher<2> d;
        HandlerCapture cap;
        UMSG_TEST_EXPECT_TRUE(ctx, d.registerHandler(1, &cap, &HandlerCapture::onMsg) == umsg::Error::OK);

        UMSG_TEST_EXPECT_TRUE(ctx,
            d.dispatch(2, 0u, umsg::ByteSpan{nullptr, 0}) == umsg::Error::HandlerNotFound);
        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }

    void test_protocol_encode_decode(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "protocol: encodeFrame() + decodeFrame() round-trip");
        uint8_t payloadBytes[] = {9, 8, 7};
        uint8_t frameBuf[32];
        umsg::ByteSpan frame{frameBuf, sizeof(frameBuf)};
        UMSG_TEST_EXPECT_TRUE(ctx,
            umsg::protocol::encodeFrame(1, 42, 0x01020304u,
                                        umsg::ByteSpan{payloadBytes, sizeof(payloadBytes)},
                                        frame) == umsg::Error::OK);

        umsg::protocol::Header h;
        umsg::ByteSpan payload;
        UMSG_TEST_EXPECT_TRUE(ctx, umsg::protocol::decodeFrame(frame, h, payload) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, h.version == 1);
        UMSG_TEST_EXPECT_TRUE(ctx, h.msgId == 42);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x01020304u, h.msgHash);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(payloadBytes), payload.length);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, payloadBytes, payload.data, payload.length);
    }

    void test_protocol_length_mismatch(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "protocol: decodeFrame() rejects length mismatch");
        uint8_t payloadBytes[] = {9, 8, 7};
        uint8_t frameBuf[32];
        umsg::ByteSpan frame{frameBuf, sizeof(frameBuf)};
        UMSG_TEST_EXPECT_TRUE(ctx,
            umsg::protocol::encodeFrame(1, 1, 0u,
                                        umsg::ByteSpan{payloadBytes, sizeof(payloadBytes)},
                                        frame) == umsg::Error::OK);

        frame.data[6] = 0;
        frame.data[7] = 1; // declared length != actual payload bytes

        umsg::protocol::Header h;
        umsg::ByteSpan payload;
        UMSG_TEST_EXPECT_TRUE(ctx,
            umsg::protocol::decodeFrame(frame, h, payload) == umsg::Error::LengthMismatch);
    }

    struct TypedMsg
    {
        static const uint32_t kMsgHash = 0xAA55AA55;
        uint32_t val;

        bool decode(umsg::ByteSpan p)
        {
            if (p.length < 4) return false;
            val = umsg::read_u32_be(p.data);
            return true;
        }
    };

    struct TypedReceiver
    {
        bool called;
        uint32_t val;

        TypedReceiver() : called(false), val(0) {}

        umsg::Error onMsg(const TypedMsg &msg)
        {
            called = true;
            val = msg.val;
            return umsg::Error::OK;
        }
    };

    void test_dispatch_typed(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "dispatcher: typed handler with schema-hash check");
        umsg::Dispatcher<4> d;
        TypedReceiver recv;

        UMSG_TEST_EXPECT_TRUE(ctx, d.registerHandler(10, &recv, &TypedReceiver::onMsg) == umsg::Error::OK);

        uint8_t payload[4];
        umsg::write_u32_be(payload, 0x12345678);

        UMSG_TEST_EXPECT_TRUE(ctx,
            d.dispatch(10, TypedMsg::kMsgHash, umsg::ByteSpan{payload, 4}) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, recv.called);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x12345678, recv.val);

        recv.called = false;
        UMSG_TEST_EXPECT_TRUE(ctx,
            d.dispatch(10, 0x00000000u, umsg::ByteSpan{payload, 4}) == umsg::Error::HashMismatch);
        UMSG_TEST_EXPECT_TRUE(ctx, !recv.called);
    }
}

void test_dispatcher(umsg_test::TestContext &ctx)
{
    test_dispatch_by_msg_id(ctx);
    test_dispatch_unknown_id(ctx);
    test_protocol_encode_decode(ctx);
    test_protocol_length_mismatch(ctx);
    test_dispatch_typed(ctx);
}
