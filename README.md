# umsg

`umsg` is a small embedded messaging library.

- C++11
- Header-only
- No dynamic allocation (static buffers only)
- Freestanding-friendly (`<stdint.h>`, `<stddef.h>`)

The library is organized around:

- **Framer**: byte-stream framing/deframing using COBS + CRC32 (emits complete frames)
- **Router**: parses frames and dispatches payloads to handlers by `msg_id`
- **Node**: integrates `Framer + Router` with a user-provided transport (`read`/`write`)

The protocol specification lives in `design.md` (kept as an internal/design doc for now).

## Protocol (high level)

Logical frame format:

- `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`

Wire packet format:

- `COBS(frame || crc32) || 0x00`

Notes:

- COBS delimiter is `0x00`
- All multi-byte on-wire fields are big-endian: `msg_hash`, `len`, `crc32`
- CRC is CRC-32/ISO-HDLC (Ethernet/PKZIP)

## Public include

Include this single header:

```cpp
#include "umsg.h"
```

## Basic usage (Node)

A transport must provide:

- `bool read(uint8_t& byte)` (non-blocking; returns false when no more bytes)
- `bool write(const uint8_t* data, size_t length)`

Example:

```cpp
#include "umsg.h"

struct MyTransport {
    bool read(uint8_t& b);                 // implemented by you
    bool write(const uint8_t* d, size_t n); // implemented by you
};

struct App {
    void onMsg(umsg::bufferSpan payload, uint32_t msgHash) {
        (void)payload;
        (void)msgHash;
        // validate msgHash + deserialize payload here
    }
};

int main() {
    MyTransport tr;
    umsg::Node<MyTransport, 64, 8> node(tr, 1);

    App app;
    node.registerHandler(7, &app, &App::onMsg);

    // RX: call periodically to drain bytes from transport
    node.poll();

    // TX: publish a message
    uint8_t bytes[] = {1, 2, 3};
    node.publish(7, 0x12345678u, umsg::bufferSpan{bytes, sizeof(bytes)});
}
```

### Buffer lifetime (important)

- RX callbacks receive `bufferSpan` that aliases the Framer’s internal RX buffer.
- That data is only valid for the duration of the callback call stack.
- Handlers must copy bytes out if they need to keep them.

## Tests

A small dependency-free test suite is included:

- `./tests/run.sh`

See `tests/README.md` for what each test covers.
