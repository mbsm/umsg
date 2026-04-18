# umsg

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Standard: C++11](https://img.shields.io/badge/Standard-C%2B%2B11-blue.svg)
![Status: Beta](https://img.shields.io/badge/Status-Beta-orange.svg)

`umsg` is a minimal, header-only C++11 messaging library for embedded systems.
It provides a zero-allocation pipeline for framing, dispatching, and marshalling
message structs over any byte-stream transport (UART, TCP, UDP, …).

---

## Key features

- **Freestanding-friendly** — depends only on `<stdint.h>`, `<stddef.h>`, and `<string.h>`. No OS calls, threads, or filesystem access.
- **Zero runtime allocation** — every buffer is sized at compile time via template parameters. No `malloc`, no `new`, no heap.
- **Header-only** — drop `src/umsg` into your include path and `#include <umsg/umsg.h>`.
- **Robust framing** — [COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) + CRC-32 (ISO-HDLC) for integrity and resync.
- **Typed dispatch** — handlers receive fully decoded messages with a schema-hash check.
- **Canonical marshalling** — big-endian wire format; endian-independent on any host.
- **Ready-to-use transports** — Arduino `Stream`/UDP/TCP and POSIX serial/UDP/TCP, or bring your own.

---

## Architecture

`umsg` is four small, decoupled layers:

```
┌──────────────────────────────────────────────────────────────┐
│  Node<Transport, MaxPayloadSize, MaxHandlers>   node.hpp     │
│  — composes the layers below with a user transport           │
└────────────┬──────────────────────────────────────┬──────────┘
             │                                      │
 ┌───────────▼───────────┐          ┌───────────────▼─────────┐
 │  Framer<MaxPacketSize>│          │  Dispatcher<MaxHandlers>│
 │  framer.hpp           │          │  dispatcher.hpp         │
 │  — COBS + CRC32       │          │  — msg_id → handler     │
 │  — feed() / encode()  │          │                         │
 └───────────┬───────────┘          └─────────────▲───────────┘
             │                                    │
             │         ┌──────────────────────────┴──────────┐
             └────────►│  protocol::encodeFrame / decodeFrame │
                       │  protocol.hpp (stateless codec)     │
                       │  version|msg_id|msg_hash|len|payload│
                       └─────────────────────────────────────┘

                   Writer / Reader                (marshalling.hpp)
                   — big-endian cursor helpers
```

Each layer is usable on its own. `Node` is the convenient default.

| File | Role |
| --- | --- |
| `common.hpp` | `ByteSpan`, `Error`, size helpers |
| `marshalling.hpp` | `Writer` / `Reader` for big-endian payloads |
| `cobs.hpp` | COBS encode / decode |
| `crc32.hpp` | CRC-32/ISO-HDLC (opt-in lookup tables) |
| `framer.hpp` | Byte-stream framing: `feed(byte)` / `encode(frame, packet)` |
| `protocol.hpp` | Pure functions: `encodeFrame` / `decodeFrame` |
| `dispatcher.hpp` | Handler table keyed by `msg_id` |
| `node.hpp` | Transport + Framer + Protocol + Dispatcher, glued |

---

## Template-based zero allocation

All buffer sizes are template parameters:

```cpp
umsg::Node<MyTransport, /*MaxPayloadSize*/ 256, /*MaxHandlers*/ 8> node(transport);
```

`sizeof(node)` tells you the exact RAM footprint — no heap, no fragmentation,
no runtime surprises. Payloads larger than `MaxPayloadSize` fail at compile time
or produce `Error::InvalidArgument` at runtime; they never corrupt memory.

---

## Quick start

### 1. Implement a transport (or use one of the built-ins)

```cpp
struct MyTransport {
    // Non-blocking byte read: true if a byte was available.
    bool read(uint8_t& byte) {
        if (uart_is_empty()) return false;
        byte = uart_read();
        return true;
    }

    // Blocking write: true iff all bytes were written.
    bool write(const uint8_t* data, size_t len) {
        return uart_write_buffer(data, len);
    }
};
```

Ready-to-use transports (included):

| Platform | Header | Class |
| --- | --- | --- |
| Arduino | `umsg/transports/arduino/stream.hpp` | `umsg::arduino::StreamTransport` |
| Arduino | `umsg/transports/arduino/udp.hpp` | `umsg::arduino::UdpTransport` |
| Arduino | `umsg/transports/arduino/tcp_client.hpp` | `umsg::arduino::TcpClientTransport` |
| POSIX | `umsg/transports/posix/serial_port.hpp` | `umsg::posix::SerialPort` |
| POSIX | `umsg/transports/posix/udp_socket.hpp` | `umsg::posix::UdpSocket` |
| POSIX | `umsg/transports/posix/tcp_client.hpp` | `umsg::posix::TcpClient` |

### 2. Set up a Node and register handlers

```cpp
#include <umsg/umsg.h>

class Robot {
public:
    umsg::Error onCommand(const Command& cmd) {
        // Dispatcher already verified Command::kMsgHash and called Command::decode.
        moveTo(cmd.target_pos);
        return umsg::Error::OK;
    }
};

int main() {
    MyTransport transport;
    umsg::Node<MyTransport, /*MaxPayloadSize*/ 256, /*MaxHandlers*/ 8> node(transport);

    Robot robot;
    node.subscribe(/*msg_id*/ 10, &robot, &Robot::onCommand);

    while (true) {
        node.poll(); // drain transport, decode, dispatch

        Telemetry t;
        t.timestamp_us = now_us();
        node.publish(/*msg_id*/ 11, t);
    }
}
```

### 3. Raw (no generated messages)

```cpp
// TX: raw payload + caller-supplied schema hash
uint8_t bytes[] = {0x01, 0x02, 0x03};
node.publish(10, /*msg_hash*/ 0x12345678u, umsg::ByteSpan{bytes, sizeof(bytes)});

// RX: raw handler receives (payload, msg_hash)
struct Raw {
    umsg::Error onMsg(umsg::ByteSpan payload, uint32_t msgHash) { /* … */ }
};
Raw r;
node.subscribe(10, &r, &Raw::onMsg);
```

---

## Using the message generator (recommended)

The generator under `tools/umsg_gen/` turns a schema file into C++ structs
with `encode` / `decode` and a deterministic `kMsgHash`.

### Define a schema

```cpp
// messages.umsg
struct Telemetry {
    uint64_t timestamp_us;
    double   position[3];
    float    battery_voltage;
    bool     is_armed;
};

struct Command {
    uint8_t mode;
    double  target_pos[3];
};
```

### Generate

```bash
python3 tools/umsg_gen/umsg_gen.py messages.umsg -o generated/
```

Each regeneration refreshes `kMsgHash` so senders and receivers must be in sync
for `publish`/`on` typed overloads to dispatch.

---

## Protocol

### Frame (application layer)

```
| version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len) |
```

- `version` — protocol version, currently `1`.
- `msg_id` — 8-bit topic id.
- `msg_hash` — 32-bit schema hash; opaque to `umsg` (typically FNV-1a of the schema).
- `len` — big-endian `uint16_t` payload length.

### Wire packet (link layer)

```
| COBS(frame || crc32) | 0x00 |
```

- Framing: [COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) guarantees no `0x00` in the encoded bytes.
- Integrity: CRC-32/ISO-HDLC (reflected, poly `0xEDB88320`) over the entire frame.
- All multi-byte fields are big-endian.

---

## Memory model & lifetimes

- **Compile-time allocation.** Every RX/TX buffer is sized by a template parameter.
  `Node` owns `txEncode_` (payload scratch), `txFrame_` (built frame), `txPacket_`
  (COBS-encoded packet), and `Framer`'s `rxBuffer_[MaxPacketSize]`.
- **RX zero-copy.** Handler `ByteSpan`s alias `Framer::rxBuffer_` — valid only for
  the duration of the dispatch call. Copy bytes out if you need to retain them.
- **TX.** `publish()` copies the payload into internal scratch before transmitting,
  so the caller's buffer can be reused immediately.
- **Reentrancy.** Do *not* call `poll()` or `publish()` recursively from a handler.

---

## Error handling

Every fallible operation returns `umsg::Error`. The values are organised by layer:

| Category | Values |
| --- | --- |
| Framing  | `FrameOverflow`, `CobsInvalid`, `CrcInvalid`, `FrameTooShort` |
| Protocol | `VersionMismatch`, `HashMismatch`, `LengthMismatch`, `HandlerNotFound` |
| Generic  | `InvalidArgument`, `TransportError`, `OK` |

After `FrameOverflow` the framer automatically resyncs on the next `0x00`.

---

## CRC-32 implementations

`crc32.hpp` offers three opt-in implementations to trade flash for CPU:

| Macro | Table | Speed | Flash |
| --- | --- | --- | --- |
| *(default)* | none | bit-by-bit | smallest |
| `UMSG_CRC32_NIBBLE_TABLE` | 16 × u32 (64 B) | ~4× faster | +64 B |
| `UMSG_CRC32_BYTE_TABLE` | 256 × u32 (1 KB) | ~8× faster | +1 KB |

On AVR the table is placed in `PROGMEM` automatically.

---

## Building & testing

Header-only library, nothing to build for consumers. For the test suite:

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

---

## Integration

1. Add `src/` to your include path.
2. `#include <umsg/umsg.h>`.

For Arduino: the repo is a valid Arduino library (`library.properties` at the root);
install it by copying the folder into your `Arduino/libraries/` directory, or via
the Library Manager once published.

---

## Status

Pre-1.0 and actively evolving — APIs may change between minor versions.

---

## Changelog

### 0.2.0 (unreleased)

Architecture pass: the monolithic `Router` was split, the `Framer` was simplified,
and names were aligned with pub/sub vocabulary. Several bugs in POSIX transports
were fixed along the way.

**Breaking API changes**

| Before | After |
| --- | --- |
| `umsg::bufferSpan` | `umsg::ByteSpan` |
| `umsg::Router<N>` | `umsg::Dispatcher<N>` + free functions in `umsg::protocol` |
| `Router::buildFrame` | `protocol::encodeFrame` |
| `Router::onPacket` | `protocol::decodeFrame` + `Dispatcher::dispatch` |
| `Framer::createPacket(frame, packet)` | `Framer::encode(frame, packet)` |
| `Framer::processByte(byte)` + `registerOnPacketCallback` | `Framer::feed(byte) -> Result{status, complete, frame}` |
| `Node::registerHandler(id, obj, &T::m)` | `Node::subscribe(id, obj, &T::m)` |
| `Node::ok()` | *removed* (nothing can fail at wire-up anymore) |
| `Node::poll()` returned error count | now returns bytes consumed |
| `CobsEncoder` struct | moved to `umsg::detail` (was never meant to be public) |

**`Error` enum renamed for consistency** (`<Subject><Problem>` pattern):

| Before | After |
| --- | --- |
| `FrameTooLarge` | `FrameOverflow` |
| `CobsDecodeFailed` | `CobsInvalid` |
| `CrcMismatch` | `CrcInvalid` |
| `FrameHeaderSize` | `FrameTooShort` |
| `MsgVersionMismatch` | `VersionMismatch` |
| `MsgIdUnknown` | `HandlerNotFound` |
| `MsgLengthMismatch` | `LengthMismatch` |
| `InvalidParameter` | `InvalidArgument` |
| *(reused `MsgVersionMismatch`)* | `HashMismatch` (new, for typed handlers) |

**Fixes**

- POSIX `SerialPort`: no more `EAGAIN` busy-spin on write (uses `poll()`); `EINTR`
  is retried; RX is buffered so `read()` no longer issues one syscall per byte.
- POSIX `TcpClient`: same treatment as `SerialPort`.
- POSIX `UdpSocket`: buffer indices are now `size_t` (was `ssize_t`); `sendto`
  success check no longer truncates through `ssize_t`.
- `Framer`: after `FrameOverflow`, bytes are dropped until the next `0x00`
  delimiter instead of producing a spurious `CobsInvalid`.
- `Node::publish<Msg>`: dedicated `txEncode_` scratch so the typed overload no
  longer aliases `txPacket_` during encoding.

**New features**

- Opt-in CRC32 tables: define `UMSG_CRC32_NIBBLE_TABLE` (64 B, ~4× faster) or
  `UMSG_CRC32_BYTE_TABLE` (1 KB, ~8× faster). Placed in `PROGMEM` on AVR.
- New stateless codec layer `umsg::protocol::{encodeFrame, decodeFrame, Header}`.

### 0.1.0

Initial release.

---

## License

MIT — see [LICENSE](LICENSE).
