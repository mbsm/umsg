# Usage guide

## Transports

A transport is any type with the following two methods:

```cpp
bool read(uint8_t& byte);                       // non-blocking
bool write(const uint8_t* data, size_t len);    // true iff all bytes written
```

Ready-to-use transports are included:

| Platform | Header | Class |
| --- | --- | --- |
| Arduino | `umsg/transports/arduino/stream.hpp` | `umsg::arduino::StreamTransport` |
| Arduino | `umsg/transports/arduino/udp.hpp` | `umsg::arduino::UdpTransport` |
| Arduino | `umsg/transports/arduino/tcp_client.hpp` | `umsg::arduino::TcpClientTransport` |
| POSIX | `umsg/transports/posix/serial_port.hpp` | `umsg::posix::SerialPort` |
| POSIX | `umsg/transports/posix/udp_socket.hpp` | `umsg::posix::UdpSocket` |
| POSIX | `umsg/transports/posix/tcp_client.hpp` | `umsg::posix::TcpClient` |

Or write your own:

```cpp
struct MyTransport {
    bool read(uint8_t& byte) {
        if (uart_is_empty()) return false;
        byte = uart_read();
        return true;
    }
    bool write(const uint8_t* data, size_t len) {
        return uart_write_buffer(data, len);
    }
};
```

## Node

```cpp
#include <umsg/umsg.h>

MyTransport transport;
umsg::Node<MyTransport, /*MaxPayloadSize*/ 256, /*MaxHandlers*/ 8> node(transport);
```

`sizeof(node)` tells you the exact RAM footprint — no heap, no fragmentation.

### Typed subscribe / publish (recommended)

Use the generator (`tools/umsg_gen/`) or hand-roll a struct that exposes
`static const uint32_t kMsgHash`, `bool encode(ByteSpan& out) const`, and
`bool decode(ByteSpan in)`:

```cpp
class Robot {
public:
    umsg::Error onCommand(const Command& cmd) {
        // Dispatcher already verified Command::kMsgHash and called Command::decode.
        moveTo(cmd.target_pos);
        return umsg::Error::OK;
    }
};

Robot robot;
node.subscribe(/*msg_id*/ 10, &robot, &Robot::onCommand);

while (true) {
    node.poll();        // drain transport, decode, dispatch
    Telemetry t;
    t.timestamp_us = now_us();
    node.publish(/*msg_id*/ 11, t);
}
```

### Raw subscribe / publish (no generator)

```cpp
struct Raw {
    umsg::Error onMsg(umsg::ByteSpan payload, uint32_t msgHash) { /* … */ }
};
Raw r;
node.subscribe(10, &r, &Raw::onMsg);

uint8_t bytes[] = {0x01, 0x02, 0x03};
node.publish(10, /*msg_hash*/ 0x12345678u, umsg::ByteSpan{bytes, sizeof(bytes)});
```

Only one handler per `msg_id`; re-subscribing replaces the previous binding.

## Error codes

Every fallible operation returns `umsg::Error`:

| Category | Values |
| --- | --- |
| Framing  | `FrameOverflow`, `CobsInvalid`, `CrcInvalid`, `FrameTooShort` |
| Protocol | `VersionMismatch`, `HashMismatch`, `LengthMismatch`, `HandlerNotFound` |
| Generic  | `InvalidArgument`, `TransportError`, `OK` |

After `FrameOverflow` the framer automatically resyncs on the next `0x00` delimiter.

`Node::poll()` intentionally discards framing/protocol errors and handler return
values — your handler is the right place to observe message-level outcomes.
Use `Framer::feed()` directly if you need per-byte diagnostics.

## CRC-32 implementations

Three opt-in variants trade flash for CPU:

| Macro | Table | Speed | Flash |
| --- | --- | --- | --- |
| *(default)* | none | bit-by-bit | smallest |
| `UMSG_CRC32_NIBBLE_TABLE` | 16 × u32 (64 B) | ~4× faster | +64 B |
| `UMSG_CRC32_BYTE_TABLE` | 256 × u32 (1 KB) | ~8× faster | +1 KB |

On AVR the table is placed in `PROGMEM` automatically.

## Building and testing

Header-only — nothing to build for consumers. For the test suite:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests compile with `-std=c++11 -Wall -Wextra -Werror -pedantic`.

POSIX examples:

```bash
cmake -S examples -B build-examples
cmake --build build-examples
```

## Integration

1. Add `src/` to your include path.
2. `#include <umsg/umsg.h>`.

For Arduino, the repo is a valid Arduino library (`library.properties` at the root):
copy it into your `Arduino/libraries/` directory, or install from the Library
Manager once published.
