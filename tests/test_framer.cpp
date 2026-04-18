#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/common.hpp>
#include <umsg/framer.hpp>

namespace
{
    void test_framer_round_trip(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "framer: encode() -> feed() emits original frame");
        static const size_t kMaxPayload = 64;
        static const size_t kMaxPacket = umsg::maxPacketSize(kMaxPayload);

        umsg::Framer<kMaxPacket> tx;
        umsg::Framer<kMaxPacket> rx;

        uint8_t frameBytes[umsg::kFrameHeaderSize + 10];
        for (size_t i = 0; i < sizeof(frameBytes); ++i)
        {
            frameBytes[i] = static_cast<uint8_t>(i);
        }
        frameBytes[3] = 0; // embed zeros to exercise COBS
        frameBytes[9] = 0;

        uint8_t packetBytes[kMaxPacket];
        umsg::ByteSpan frame{frameBytes, sizeof(frameBytes)};
        umsg::ByteSpan packet{packetBytes, sizeof(packetBytes)};

        UMSG_TEST_EXPECT_TRUE(ctx, tx.encode(frame, packet) == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, packet.length >= 2);
        UMSG_TEST_EXPECT_TRUE(ctx, packet.data[packet.length - 1] == 0x00);

        bool emitted = false;
        size_t emittedLen = 0;
        uint8_t emittedBytes[256];

        for (size_t i = 0; i < packet.length; ++i)
        {
            umsg::Framer<kMaxPacket>::Result r = rx.feed(packet.data[i]);
            UMSG_TEST_EXPECT_TRUE(ctx, r.status == umsg::Error::OK);
            if (r.complete)
            {
                emitted = true;
                emittedLen = r.frame.length;
                if (emittedLen <= sizeof(emittedBytes))
                {
                    ::memcpy(emittedBytes, r.frame.data, emittedLen);
                }
            }
        }

        UMSG_TEST_EXPECT_TRUE(ctx, emitted);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(frameBytes), emittedLen);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, frameBytes, emittedBytes, emittedLen);
    }

    void test_framer_crc_failure(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "framer: CRC mismatch is rejected");
        static const size_t kMaxPayload = 32;
        static const size_t kMaxPacket = umsg::maxPacketSize(kMaxPayload);

        umsg::Framer<kMaxPacket> tx;
        umsg::Framer<kMaxPacket> rx;

        uint8_t frameBytes[16];
        for (size_t i = 0; i < sizeof(frameBytes); ++i)
        {
            frameBytes[i] = static_cast<uint8_t>(0xA0u + i);
        }

        uint8_t packetBytes[kMaxPacket];
        umsg::ByteSpan packet{packetBytes, sizeof(packetBytes)};
        UMSG_TEST_EXPECT_TRUE(ctx, tx.encode(umsg::ByteSpan{frameBytes, sizeof(frameBytes)}, packet) == umsg::Error::OK);

        if (packet.length > 2)
        {
            packet.data[1] ^= 0x01u;
        }

        bool sawFailure = false;
        bool anyComplete = false;
        for (size_t i = 0; i < packet.length; ++i)
        {
            umsg::Framer<kMaxPacket>::Result r = rx.feed(packet.data[i]);
            if (r.complete) anyComplete = true;
            if (r.status == umsg::Error::CrcInvalid)
            {
                sawFailure = true;
                break;
            }
        }

        UMSG_TEST_EXPECT_TRUE(ctx, sawFailure);
        UMSG_TEST_EXPECT_TRUE(ctx, !anyComplete);
    }
}

void test_framer(umsg_test::TestContext &ctx)
{
    test_framer_round_trip(ctx);
    test_framer_crc_failure(ctx);
}
