#include "tests/test_harness.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "umsg.h"

namespace
{
    template <size_t Capacity>
    struct Ring
    {
        uint8_t buf[Capacity];
        size_t head;
        size_t tail;
        size_t count;

        Ring() : head(0), tail(0), count(0) {}

        bool push(uint8_t b)
        {
            if (count >= Capacity)
            {
                return false;
            }
            buf[tail] = b;
            tail = (tail + 1) % Capacity;
            ++count;
            return true;
        }

        bool pop(uint8_t &b)
        {
            if (count == 0)
            {
                return false;
            }
            b = buf[head];
            head = (head + 1) % Capacity;
            --count;
            return true;
        }
    };

    template <size_t Capacity>
    struct DuplexLink
    {
        Ring<Capacity> a2b;
        Ring<Capacity> b2a;

        struct Endpoint
        {
            Ring<Capacity> *in;
            Ring<Capacity> *out;

            bool read(uint8_t &byte)
            {
                return in->pop(byte);
            }

            bool write(const uint8_t *data, size_t length)
            {
                for (size_t i = 0; i < length; ++i)
                {
                    if (!out->push(data[i]))
                    {
                        return false;
                    }
                }
                return true;
            }
        };

        Endpoint endpointA()
        {
            Endpoint e;
            e.in = &b2a;
            e.out = &a2b;
            return e;
        }

        Endpoint endpointB()
        {
            Endpoint e;
            e.in = &a2b;
            e.out = &b2a;
            return e;
        }
    };

    struct Sink
    {
        bool called;
        uint32_t hash;
        uint8_t payload[16];
        size_t payloadLen;

        Sink() : called(false), hash(0), payloadLen(0) {}

        void onPayload(umsg::bufferSpan p, uint32_t h)
        {
            called = true;
            hash = h;
            payloadLen = p.length;
            if (payloadLen > sizeof(payload))
            {
                payloadLen = sizeof(payload);
            }
            if (payloadLen)
            {
                ::memcpy(payload, p.data, payloadLen);
            }
        }
    };

    void test_node_end_to_end(umsg_test::TestContext &ctx)
    {
        UMSG_TEST_SECTION(ctx, "node: publish() on A -> poll() on B -> handler called with payload+hash");
        DuplexLink<1024> link;
        DuplexLink<1024>::Endpoint a = link.endpointA();
        DuplexLink<1024>::Endpoint b = link.endpointB();

        umsg::Node<DuplexLink<1024>::Endpoint, 32, 4> nodeA(a, 1);
        umsg::Node<DuplexLink<1024>::Endpoint, 32, 4> nodeB(b, 1);

        UMSG_TEST_EXPECT_TRUE(ctx, nodeA.ok());
        UMSG_TEST_EXPECT_TRUE(ctx, nodeB.ok());

        Sink sink;
        UMSG_TEST_EXPECT_TRUE(ctx, nodeB.registerHandler(9, &sink, &Sink::onPayload));

        uint8_t payloadBytes[3] = {0x10, 0x00, 0x20};
        const bool sent = nodeA.publish(9, 0xAABBCCDDu, umsg::bufferSpan{payloadBytes, sizeof(payloadBytes)});
        UMSG_TEST_EXPECT_TRUE(ctx, sent);

        const bool polled = nodeB.poll();
        UMSG_TEST_EXPECT_TRUE(ctx, polled);

        UMSG_TEST_EXPECT_TRUE(ctx, sink.called);
        UMSG_TEST_EXPECT_EQ_U32(ctx, 0xAABBCCDDu, sink.hash);
        UMSG_TEST_EXPECT_EQ_SIZE(ctx, sizeof(payloadBytes), sink.payloadLen);
        UMSG_TEST_EXPECT_BUF_EQ(ctx, payloadBytes, sink.payload, sink.payloadLen);
    }
}

void test_node(umsg_test::TestContext &ctx)
{
    test_node_end_to_end(ctx);
}
