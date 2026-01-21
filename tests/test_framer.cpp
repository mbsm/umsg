#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/common.hpp>
#include <umsg/framer.hpp>

namespace
{
    struct Capture
    {
        bool called;
        uint8_t data[256];
        size_t length;

        Capture() : called(false), length(0) {}

        umsg::Error onFrame(umsg::bufferSpan frame)
        {
            called = true;
            length = frame.length;
            if (length > sizeof(data))
            {
                length = sizeof(data);
            }
            if (length)
            {
                ::memcpy(data, frame.data, length);
            }
            return umsg::Error::OK;
        }
    };

    void test_framer_round_trip(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "framer: createPacket() -> processByte() emits original frame");
        static const size_t kMaxPayload = 64;
        static const size_t kMaxPacket = umsg::maxPacketSize(kMaxPayload);

        umsg::Framer<kMaxPacket> tx;
        umsg::Framer<kMaxPacket> rx;

        Capture cap;
        const umsg::Error cbOk = rx.registerOnPacketCallback(&cap, &Capture::onFrame);
        UMSG_TEST_EXPECT_TRUE(ctx, cbOk == umsg::Error::OK);

        uint8_t frameBytes[umsg::kFrameHeaderSize + 10];
        // Not necessarily a valid protocol frame; framer is agnostic.
        for (size_t i = 0; i < sizeof(frameBytes); ++i)
        {
            frameBytes[i] = static_cast<uint8_t>(i);
        }
        frameBytes[3] = 0; // embed some zeros to exercise COBS
        frameBytes[9] = 0;

        uint8_t packetBytes[kMaxPacket];
        umsg::bufferSpan frame{frameBytes, sizeof(frameBytes)};
        umsg::bufferSpan packet{packetBytes, sizeof(packetBytes)};

        const umsg::Error pktOk = tx.createPacket(frame, packet);
        UMSG_TEST_EXPECT_TRUE(ctx, pktOk == umsg::Error::OK);
        UMSG_TEST_EXPECT_TRUE(ctx, packet.length >= 2);
        UMSG_TEST_EXPECT_TRUE(ctx, packet.data[packet.length - 1] == 0x00);

        // Feed packet byte-by-byte.
        for (size_t i = 0; i < packet.length; ++i)
        {
            const umsg::Error ok = rx.processByte(packet.data[i]);
            UMSG_TEST_EXPECT_TRUE(ctx, ok == umsg::Error::OK);
        }

        UMSG_TEST_EXPECT_TRUE(ctx, cap.called);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(frameBytes), cap.length);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, frameBytes, cap.data, cap.length);
    }

    void test_framer_crc_failure(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "framer: CRC mismatch is rejected (no callback)");
        static const size_t kMaxPayload = 32;
        static const size_t kMaxPacket = umsg::maxPacketSize(kMaxPayload);

        umsg::Framer<kMaxPacket> tx;
        umsg::Framer<kMaxPacket> rx;

        Capture cap;
        UMSG_TEST_EXPECT_TRUE(ctx, rx.registerOnPacketCallback(&cap, &Capture::onFrame) == umsg::Error::OK);

        uint8_t frameBytes[16];
        for (size_t i = 0; i < sizeof(frameBytes); ++i)
        {
            frameBytes[i] = static_cast<uint8_t>(0xA0u + i);
        }

        uint8_t packetBytes[kMaxPacket];
        umsg::bufferSpan packet{packetBytes, sizeof(packetBytes)};
        UMSG_TEST_EXPECT_TRUE(ctx, tx.createPacket(umsg::bufferSpan{frameBytes, sizeof(frameBytes)}, packet) == umsg::Error::OK);

        // Flip one byte (not the delimiter) so CRC should fail.
        if (packet.length > 2)
        {
            packet.data[1] ^= 0x01u;
        }

        bool sawFailure = false;
        for (size_t i = 0; i < packet.length; ++i)
        {
            const umsg::Error ok = rx.processByte(packet.data[i]);
            if (ok == umsg::Error::CrcMismatch)
            {
                sawFailure = true;
                break;
            }
        }

        UMSG_TEST_EXPECT_TRUE(ctx, sawFailure);
        UMSG_TEST_EXPECT_TRUE(ctx, !cap.called);
    }
}

void test_framer(umsg_test::TestContext &ctx)
{
    test_framer_round_trip(ctx);
    test_framer_crc_failure(ctx);
}
