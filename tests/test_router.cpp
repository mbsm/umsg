#include "tests/test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "router.hpp"

namespace
{
    struct HandlerCapture
    {
        bool called;
        uint32_t msgHash;
        uint8_t payload[64];
        size_t payloadLen;

        HandlerCapture() : called(false), msgHash(0), payloadLen(0) {}

        void onMsg(umsg::bufferSpan payloadSpan, uint32_t hash)
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
        }
    };

    void test_router_dispatch(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: buildFrame() + onPacket() dispatches by msg_id");
        umsg::Router<4> r(1);
        HandlerCapture cap;

        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(7, &cap, &HandlerCapture::onMsg));

        uint8_t payloadBytes[] = {1, 2, 3, 4};
        uint8_t frameBuf[128];
        umsg::bufferSpan out{frameBuf, sizeof(frameBuf)};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(7, 0x12345678u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, out));

        r.onPacket(out);

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
        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(1, &cap, &HandlerCapture::onMsg));

        uint8_t frameBuf[16];
        umsg::bufferSpan frame{frameBuf, sizeof(frameBuf)};

        uint8_t payloadBytes[] = {9};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(1, 0x01020304u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, frame));

        frame.data[0] = 2; // wrong version
        r.onPacket(frame);

        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }

    void test_router_rejects_len_mismatch(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "router: rejects frames whose len field mismatches actual payload size");
        umsg::Router<2> r(1);
        HandlerCapture cap;
        UMSG_TEST_EXPECT_TRUE(ctx, r.registerHandler(1, &cap, &HandlerCapture::onMsg));

        uint8_t frameBuf[32];
        umsg::bufferSpan frame{frameBuf, sizeof(frameBuf)};

        uint8_t payloadBytes[] = {9, 8, 7};
        UMSG_TEST_EXPECT_TRUE(ctx, r.buildFrame(1, 0x01020304u, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)}, frame));

        // Corrupt declared payload length.
        frame.data[6] = 0;
        frame.data[7] = 1;

        r.onPacket(frame);
        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }
}

void test_router(umsg_test::TestContext &ctx)
{
    test_router_dispatch(ctx);
    test_router_rejects_bad_version(ctx);
    test_router_rejects_len_mismatch(ctx);
}
