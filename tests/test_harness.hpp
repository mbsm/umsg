#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

namespace umsg_test
{
    struct TestContext
    {
        int failures;
        size_t checks;
        const char *currentTest;
        const char *currentSection;

        TestContext() : failures(0), checks(0), currentTest(0), currentSection(0) {}

        void logf(const char *fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            ::vfprintf(stdout, fmt, args);
            va_end(args);
        }

        void beginTest(const char *name, const char *description)
        {
            currentTest = name;
            currentSection = 0;
            if (description && description[0])
            {
                ::fprintf(stdout, "\n=== RUN  %s: %s\n", name, description);
            }
            else
            {
                ::fprintf(stdout, "\n=== RUN  %s\n", name);
            }
        }

        void section(const char *name)
        {
            currentSection = name;
            if (name && name[0])
            {
                ::fprintf(stdout, "---     %s\n", name);
            }
        }

        void endTest(const char *name, int failuresBefore, size_t checksBefore)
        {
            const int newFailures = failures - failuresBefore;
            const size_t newChecks = checks - checksBefore;
            if (newFailures == 0)
            {
                ::fprintf(stdout, "=== OK   %s (%zu checks)\n", name, newChecks);
            }
            else
            {
                ::fprintf(stdout, "=== FAIL %s (%d failures, %zu checks)\n", name, newFailures, newChecks);
            }
        }

        void noteCheck() { ++checks; }

        void fail(const char *file, int line, const char *expr)
        {
            ++failures;
            if (currentTest)
            {
                if (currentSection)
                {
                    ::fprintf(stderr, "%s:%d: FAIL [%s] [%s]: %s\n", file, line, currentTest, currentSection, expr);
                }
                else
                {
                    ::fprintf(stderr, "%s:%d: FAIL [%s]: %s\n", file, line, currentTest, expr);
                }
            }
            else
            {
                ::fprintf(stderr, "%s:%d: FAIL: %s\n", file, line, expr);
            }
        }

        void fail_eq_u32(const char *file, int line, const char *label, uint32_t expected, uint32_t actual)
        {
            ++failures;
            if (currentTest && currentSection)
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL [%s] [%s]: %s (expected=0x%08X actual=0x%08X)\n",
                          file,
                          line,
                          currentTest,
                          currentSection,
                          label,
                          static_cast<unsigned>(expected),
                          static_cast<unsigned>(actual));
            }
            else if (currentTest)
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL [%s]: %s (expected=0x%08X actual=0x%08X)\n",
                          file,
                          line,
                          currentTest,
                          label,
                          static_cast<unsigned>(expected),
                          static_cast<unsigned>(actual));
            }
            else
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL: %s (expected=0x%08X actual=0x%08X)\n",
                          file,
                          line,
                          label,
                          static_cast<unsigned>(expected),
                          static_cast<unsigned>(actual));
            }
        }

        void fail_eq_size(const char *file, int line, const char *label, size_t expected, size_t actual)
        {
            ++failures;
            if (currentTest && currentSection)
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL [%s] [%s]: %s (expected=%zu actual=%zu)\n",
                          file,
                          line,
                          currentTest,
                          currentSection,
                          label,
                          expected,
                          actual);
            }
            else if (currentTest)
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL [%s]: %s (expected=%zu actual=%zu)\n",
                          file,
                          line,
                          currentTest,
                          label,
                          expected,
                          actual);
            }
            else
            {
                ::fprintf(stderr,
                          "%s:%d: FAIL: %s (expected=%zu actual=%zu)\n",
                          file,
                          line,
                          label,
                          expected,
                          actual);
            }
        }

        void fail_buf(const char *file,
                      int line,
                      const char *label,
                      const uint8_t *expected,
                      const uint8_t *actual,
                      size_t length)
        {
            ++failures;
            if (currentTest && currentSection)
            {
                ::fprintf(stderr, "%s:%d: FAIL [%s] [%s]: %s (mismatch)\n", file, line, currentTest, currentSection, label);
            }
            else if (currentTest)
            {
                ::fprintf(stderr, "%s:%d: FAIL [%s]: %s (mismatch)\n", file, line, currentTest, label);
            }
            else
            {
                ::fprintf(stderr, "%s:%d: FAIL: %s (mismatch)\n", file, line, label);
            }
            ::fprintf(stderr, "  expected:");
            for (size_t i = 0; i < length; ++i)
            {
                ::fprintf(stderr, " %02X", static_cast<unsigned>(expected[i]));
            }
            ::fprintf(stderr, "\n  actual:  ");
            for (size_t i = 0; i < length; ++i)
            {
                ::fprintf(stderr, " %02X", static_cast<unsigned>(actual[i]));
            }
            ::fprintf(stderr, "\n");
        }
    };
}

#define UMSG_TEST_EXPECT_TRUE(ctx, expr) \
    do \
    { \
        (ctx).noteCheck(); \
        if (!(expr)) \
        { \
            (ctx).fail(__FILE__, __LINE__, #expr); \
        } \
    } while (0)

#define UMSG_TEST_EXPECT_EQ_U32(ctx, expected, actual) \
    do \
    { \
        (ctx).noteCheck(); \
        const uint32_t _e = static_cast<uint32_t>(expected); \
        const uint32_t _a = static_cast<uint32_t>(actual); \
        if (_e != _a) \
        { \
            (ctx).fail_eq_u32(__FILE__, __LINE__, #actual, _e, _a); \
        } \
    } while (0)

#define UMSG_TEST_EXPECT_EQ_SIZE(ctx, expected, actual) \
    do \
    { \
        (ctx).noteCheck(); \
        const size_t _e = static_cast<size_t>(expected); \
        const size_t _a = static_cast<size_t>(actual); \
        if (_e != _a) \
        { \
            (ctx).fail_eq_size(__FILE__, __LINE__, #actual, _e, _a); \
        } \
    } while (0)

#define UMSG_TEST_EXPECT_BUF_EQ(ctx, expectedPtr, actualPtr, len) \
    do \
    { \
        (ctx).noteCheck(); \
        const uint8_t *_e = reinterpret_cast<const uint8_t *>(expectedPtr); \
        const uint8_t *_a = reinterpret_cast<const uint8_t *>(actualPtr); \
        const size_t _n = static_cast<size_t>(len); \
        bool _ok = true; \
        for (size_t _i = 0; _i < _n; ++_i) \
        { \
            if (_e[_i] != _a[_i]) \
            { \
                _ok = false; \
                break; \
            } \
        } \
        if (!_ok) \
        { \
            (ctx).fail_buf(__FILE__, __LINE__, #actualPtr, _e, _a, _n); \
        } \
    } while (0)

#define UMSG_TEST_SECTION(ctx, name) \
    do \
    { \
        (ctx).section(name); \
    } while (0)
