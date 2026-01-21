#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/router.hpp>

namespace
{
    struct HandlerCapture
    {
        bool called;
        uint32_t msgHash;
        uint8_t payload[64];
        size_t payloadLen;

        HandlerCapture() : called(false), msgHash(0), payloadLen(0) {}

        umsg::Error onMsg(umsg::bufferSpan payloadSpan, uint32_t hash)
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

    void test_router_dispatch(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: buildFrame() + onPacket() dispatches by msg_id");
        umsg::Router<4> r(1);
        HandlerCapture cap;

        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(7, &cap, &HandlerCapture::onMsg) == umsg::Error::OK);

        uint8_t payloadBytes[] = {1, 2, 3, 4};
        uint8_t frameBuf[128];
        umsg::bufferSpan out{frameBuf, sizeof(frameBuf)};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(7, 0x12345678u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, out) == umsg::Error::OK);

        UMSG_TEST_EXPECT_TRUE(ctx, r.onPacket(out) == umsg::Error::OK);

        UMSG_TEST_EXPECT_TRUE(ctx, cap.called);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x12345678u, cap.msgHash);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(payloadBytes), cap.payloadLen);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, payloadBytes, cap.payload, cap.payloadLen);
    }

    void test_router_rejects_bad_version(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: rejects frames with unexpected version");
        umsg::Router<2> r(1);
        HandlerCapture cap;
        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(1, &cap, &HandlerCapture::onMsg) == umsg::Error::OK);

        uint8_t frameBuf[16];
        umsg::bufferSpan frame{frameBuf, sizeof(frameBuf)};

        uint8_t payloadBytes[] = {9};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(1, 0x01020304u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, frame) == umsg::Error::OK);

        frame.data[0] = 2; // wrong version
        umsg::Error err = r.onPacket(frame);

        UMSG_TEST_EXPECT_TRUE(ctx, err == umsg::Error::MsgVersionMismatch);
        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }

    void test_router_rejects_len_mismatch(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: rejects frames whose len field mismatches actual payload size");
        umsg::Router<2> r(1);
        HandlerCapture cap;
        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(1, &cap, &HandlerCapture::onMsg) == umsg::Error::OK);

        uint8_t frameBuf[32];
        umsg::bufferSpan frame{frameBuf, sizeof(frameBuf)};

        uint8_t payloadBytes[] = {9, 8, 7};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(1, 0x01020304u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, frame) == umsg::Error::OK);

        // Corrupt declared payload length.
        frame.data[6] = 0;
        frame.data[7] = 1;

        umsg::Error err = r.onPacket(frame);
        UMSG_TEST_EXPECT_TRUE(ctx, err == umsg::Error::MsgLengthMismatch);
        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }
    
    struct TypedMsg
    {
        static const uint32_t kMsgHash = 0xAA55AA55;
        uint32_t val;
        
        bool decode(umsg::bufferSpan p)
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
        
        umsg::Error onMsg(const TypedMsg& msg)
        {
            called = true;
            val = msg.val;
            return umsg::Error::OK;
        }
    };

    void test_router_typed(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: type-safe handler registration and dispatch");
        umsg::Router<4> r(1);
        TypedReceiver recv;
        
        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(10, &recv, &TypedReceiver::onMsg) == umsg::Error::OK);
        
        uint8_t frameBuf[64];
        umsg::bufferSpan frame{frameBuf, sizeof(frameBuf)};
        
        uint8_t payload[4];
        umsg::write_u32_be(payload, 0x12345678);
        
        // Good hash
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(10, TypedMsg::kMsgHash, umsg::bufferSpan{payload, 4}, frame) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, r.onPacket(frame) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, recv.called);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x12345678, recv.val);
        
        recv.called = false;
        
        // Bad hash
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(10, 0x00000000, umsg::bufferSpan{payload, 4}, frame) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, r.onPacket(frame) == umsg::Error::MsgVersionMismatch);
        UMSG_TEST_EXPECT_TRUE(ctx, !recv.called);
    }
}

void test_router(umsg_test::TestContext &ctx)
{
    test_router_dispatch(ctx);
    test_router_rejects_bad_version(ctx);
    test_router_rejects_len_mismatch(ctx);
    test_router_typed(ctx);
}
