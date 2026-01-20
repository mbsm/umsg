#include "tests/test_harness.hpp"

#include <stdio.h>

void test_crc32(umsg_test::TestContext &ctx);
void test_cobs(umsg_test::TestContext &ctx);
void test_framer(umsg_test::TestContext &ctx);
void test_router(umsg_test::TestContext &ctx);
void test_node(umsg_test::TestContext &ctx);
void test_marshal(umsg_test::TestContext &ctx);

int main()
{
    umsg_test::TestContext ctx;

    struct TestDef
    {
        const char *name;
        const char *description;
        void (*fn)(umsg_test::TestContext &ctx);
    };

    static const TestDef tests[] = {
        {"crc32", "CRC-32/ISO-HDLC known vectors", &test_crc32},
        {"cobs", "COBS encode/decode round-trips", &test_cobs},
        {"framer", "COBS+CRC framing/deframing", &test_framer},
        {"router", "Frame parsing + handler dispatch", &test_router},
        {"node", "Transport integration end-to-end", &test_node},
        {"marshal", "Canonical payload Writer/Reader", &test_marshal},
    };

    const size_t testCount = sizeof(tests) / sizeof(tests[0]);
    for (size_t i = 0; i < testCount; ++i)
    {
        const int failuresBefore = ctx.failures;
        const size_t checksBefore = ctx.checks;

        ctx.beginTest(tests[i].name, tests[i].description);
        tests[i].fn(ctx);
        ctx.endTest(tests[i].name, failuresBefore, checksBefore);
    }

    if (ctx.failures == 0)
    {
        ::fprintf(stdout, "\nALL TESTS PASSED (%zu checks)\n", ctx.checks);
        return 0;
    }

    ::fprintf(stderr, "\nTEST FAILURES: %d (out of %zu checks)\n", ctx.failures, ctx.checks);
    return 1;
}
