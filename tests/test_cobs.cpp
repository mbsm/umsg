#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/cobs.hpp>

namespace
{
    void round_trip(umsg_test::TestContext &ctx, const uint8_t *input, size_t inputLen)
    {
        (void)ctx;
        uint8_t encoded[1024];
        size_t encodedLen = 0;
        const bool encOk = umsg::cobsEncode(input, inputLen, encoded, sizeof(encoded), encodedLen);
        UMSG_TEST_EXPECT_TRUE(ctx, encOk);
        UMSG_TEST_EXPECT_TRUE(ctx, encodedLen <= sizeof(encoded));

        for (size_t i = 0; i < encodedLen; ++i)
        {
            UMSG_TEST_EXPECT_TRUE(ctx, encoded[i] != 0);
        }

        uint8_t scratch[1024];
        UMSG_TEST_EXPECT_TRUE(ctx, encodedLen <= sizeof(scratch));
        ::memcpy(scratch, encoded, encodedLen);

        size_t decodedLen = 0;
        const bool decOk = umsg::cobsDecodeInPlace(scratch, encodedLen, decodedLen);
        UMSG_TEST_EXPECT_TRUE(ctx, decOk);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, inputLen, decodedLen);

        if (inputLen > 0)
        {
            UMSG_TEST_EXPECT_BUF_EQ(ctx, input, scratch, inputLen);
        }
    }

    void test_patterns(umsg_test::TestContext &ctx)
    {
        // empty
        UMSG_TEST_SECTION(ctx, "cobs: round-trip empty buffer");
        round_trip(ctx, 0, 0);

        // no zeros
        UMSG_TEST_SECTION(ctx, "cobs: round-trip without zero bytes");
        const uint8_t a[] = {0x11, 0x22, 0x33, 0x44};
        round_trip(ctx, a, sizeof(a));

        // with zeros
        UMSG_TEST_SECTION(ctx, "cobs: round-trip with embedded zeros");
        const uint8_t b[] = {0x11, 0x00, 0x22, 0x00, 0x00, 0x33};
        round_trip(ctx, b, sizeof(b));

        // boundary-ish length (forces 0xFF code blocks)
        UMSG_TEST_SECTION(ctx, "cobs: round-trip large non-zero payload (exercises 0xFF blocks)");
        uint8_t big[300];
        for (size_t i = 0; i < sizeof(big); ++i)
        {
            big[i] = static_cast<uint8_t>((i % 251) + 1); // never 0
        }
        round_trip(ctx, big, sizeof(big));

        // boundary with zeros sprinkled
        UMSG_TEST_SECTION(ctx, "cobs: round-trip large payload with sprinkled zeros");
        for (size_t i = 0; i < sizeof(big); ++i)
        {
            big[i] = (i % 50 == 0) ? 0u : static_cast<uint8_t>((i % 251) + 1);
        }
        round_trip(ctx, big, sizeof(big));
    }
}

void test_cobs(umsg_test::TestContext &ctx)
{
    test_patterns(ctx);
}
