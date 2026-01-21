#include "test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <umsg/marshalling.hpp>

namespace
{
    static uint32_t float_bits(float v)
    {
        uint32_t u = 0;
        ::memcpy(&u, &v, sizeof(u));
        return u;
    }

    static uint64_t double_bits(double v)
    {
        uint64_t u = 0;
        ::memcpy(&u, &v, sizeof(u));
        return u;
    }

    void test_endian_u64(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "common: u64 big-endian helpers");

        uint8_t buf[8];
        const uint64_t v = 0x0102030405060708ull;
        umsg::write_u64_be(buf, v);

        const uint8_t expected[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        UMSG_TEST_EXPECT_BUF_EQ(ctx, expected, buf, sizeof(buf));

        const uint64_t r = umsg::read_u64_be(buf);
        UMSG_TEST_EXPECT_TRUE(ctx, r == v);
    }

    void test_writer_reader_roundtrip(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "common: Writer/Reader round-trip (scalars + arrays)");

        uint8_t buf[128];
        umsg::Writer w(umsg::bufferSpan{buf, sizeof(buf)});

        const uint8_t u8 = 0xA5;
        const int16_t i16 = -1234;
        const uint32_t u32 = 0x11223344u;
        const int32_t i32 = -12345678;
        const bool b = true;
        const float f = 3.1415926f;
        const double d = -0.0;
        const uint16_t arr[3] = {1u, 2u, 65535u};

        UMSG_TEST_EXPECT_TRUE(ctx, w.write(u8));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(i16));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(u32));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(i32));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(b));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(f));
        UMSG_TEST_EXPECT_TRUE(ctx, w.write(d));
        UMSG_TEST_EXPECT_TRUE(ctx, w.writeArray(arr, 3));

        const size_t n = w.bytesWritten();
        UMSG_TEST_EXPECT_TRUE(ctx, n > 0);

        umsg::Reader r(umsg::bufferSpan{buf, n});

        uint8_t u8_r = 0;
        int16_t i16_r = 0;
        uint32_t u32_r = 0;
        int32_t i32_r = 0;
        bool b_r = false;
        float f_r = 0.0f;
        double d_r = 0.0;
        uint16_t arr_r[3] = {0, 0, 0};

        UMSG_TEST_EXPECT_TRUE(ctx, r.read(u8_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(i16_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(u32_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(i32_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(b_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(f_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.read(d_r));
        UMSG_TEST_EXPECT_TRUE(ctx, r.readArray(arr_r, 3));

        UMSG_TEST_EXPECT_TRUE(ctx, r.fullyConsumed());

        UMSG_TEST_EXPECT_TRUE(ctx, u8_r == u8);
        UMSG_TEST_EXPECT_TRUE(ctx, i16_r == i16);
        UMSG_TEST_EXPECT_EQ_U32(ctx, u32, u32_r);
        UMSG_TEST_EXPECT_TRUE(ctx, i32_r == i32);
        UMSG_TEST_EXPECT_TRUE(ctx, b_r == b);
        UMSG_TEST_EXPECT_EQ_U32(ctx, float_bits(f), float_bits(f_r));
        UMSG_TEST_EXPECT_TRUE(ctx, double_bits(d) == double_bits(d_r));
        UMSG_TEST_EXPECT_TRUE(ctx, arr_r[0] == arr[0]);
        UMSG_TEST_EXPECT_TRUE(ctx, arr_r[1] == arr[1]);
        UMSG_TEST_EXPECT_TRUE(ctx, arr_r[2] == arr[2]);
    }

    void test_reader_rejects_invalid_bool(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "common: Reader rejects invalid bool");

        uint8_t buf[1] = {2};
        umsg::Reader r(umsg::bufferSpan{buf, sizeof(buf)});

        bool b = false;
        UMSG_TEST_EXPECT_TRUE(ctx, !r.read(b));
    }
}

void test_marshal(umsg_test::TestContext &ctx)
{
    test_endian_u64(ctx);
    test_writer_reader_roundtrip(ctx);
    test_reader_rejects_invalid_bool(ctx);
}
