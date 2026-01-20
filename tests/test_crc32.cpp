#include "tests/test_harness.hpp"

#include <stddef.h>
#include <stdint.h>

#include "crc32.hpp"

namespace
{
    void test_crc32_known_vector(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "crc32: known vector '123456789' -> 0xCBF43926");
        const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        const uint32_t crc = umsg::crc32_iso_hdlc(data, sizeof(data));
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0xCBF43926u, crc);
    }

    void test_crc32_empty(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "crc32: empty input -> 0x00000000");
        const uint32_t crc = umsg::crc32_iso_hdlc(0, 0);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0x00000000u, crc);
    }
}

void test_crc32(umsg_test::TestContext &ctx)
{
    test_crc32_known_vector(ctx);
    test_crc32_empty(ctx);
}
