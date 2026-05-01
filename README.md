# umsg

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Standard: C++11](https://img.shields.io/badge/Standard-C%2B%2B11-blue.svg)
![Status: Beta](https://img.shields.io/badge/Status-Beta-orange.svg)

**umsg** is a header-only C++11 messaging library for embedded systems and the
hosts that talk to them. It turns any reliable or unreliable byte stream — a
UART, a TCP socket, a UDP datagram channel — into a typed publish/subscribe bus
with framing, integrity checking, and schema validation built in.

It is designed for environments where dynamic allocation is unavailable or
undesirable: every buffer is sized at compile time, and the library depends
only on `<stdint.h>`, `<stddef.h>`, and `<string.h>`.

## Why umsg

- **Header-only and freestanding-friendly.** Add `src/` to your include path
  and `#include <umsg/umsg.h>`. No build step, no STL, no exceptions, no RTTI.
- **Zero runtime allocation.** RAM usage is fixed at compile time and visible
  through `sizeof(node)`.
- **Robust framing.** Packets are wrapped in COBS with a CRC-32/ISO-HDLC
  checksum; the framer automatically resyncs after corruption.
- **Typed dispatch.** Each message carries a 32-bit schema hash that is
  verified before your handler is called, so mismatched senders and receivers
  fail loudly instead of silently corrupting data.
- **Transport-agnostic, with first-class Arduino support.** `umsg::Node` speaks
  the Arduino `Stream` contract directly, so `Serial`, `SoftwareSerial`,
  `EthernetClient`, `WiFiClient`, and any other `Stream` subclass plug in
  without an adapter. POSIX serial / UDP / TCP and Arduino UDP transports are
  included for the cases that aren't `Stream`-shaped.
- **Optional code generation.** A small Python tool (`tools/umsg_gen`) turns
  `.umsg` schema files into C++ structs with `encode` / `decode` and a
  pre-computed schema hash.

## Installation

### CMake / generic C++

Add `src/` to your include path and include the umbrella header:

```cpp
#include <umsg/umsg.h>
```

The repository also ships a CMake project for building the test suite and
examples:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Arduino

The repository is a valid Arduino library (`library.properties` is at the
root). Copy it into `~/Arduino/libraries/umsg/`, or install it through the
Library Manager once published.

## Getting started

The example below shows a complete round trip: the device subscribes to a
`Command` topic, and publishes `Telemetry` on a timer. Both ends agree on a
channel id and on the schema of the payload — typically by including the same
generated header.

**1. Describe your messages in `.umsg` schema files.**

```
// Command.umsg
struct Command {
    int32_t target_pos;
};
```

```
// Telemetry.umsg
struct Telemetry {
    uint32_t timestamp_ms;
    int32_t  position;
};
```

**2. Generate the C++ headers.**

```bash
python3 tools/umsg_gen/umsg_gen.py Command.umsg Telemetry.umsg -o messages/
```

This produces `messages/Command.hpp` and `messages/Telemetry.hpp`, each
containing a struct with `kMsgHash`, `encode()`, and `decode()` already
filled in.

**3. Use them from your sketch.**

```cpp
#include <Arduino.h>
#include <umsg/umsg.h>
#include "messages/Command.hpp"
#include "messages/Telemetry.hpp"

// Channels (1 byte each) are agreed by sender and receiver.
constexpr uint8_t kCmdChannel       = 10;
constexpr uint8_t kTelemetryChannel = 11;

class Robot {
public:
    int32_t position = 0;

    umsg::Error onCommand(const Command& cmd) {
        // By the time we get here, the dispatcher has already:
        //   1. Matched msg_id 10 to this handler.
        //   2. Verified the incoming msg_hash equals Command::kMsgHash
        //      (so a sender with a stale/incompatible schema is silently
        //      dropped instead of being decoded as garbage).
        //   3. Called Command::decode() on the payload — `cmd` is fully
        //      populated and ready to use.
        // No defensive checks needed.
        position = cmd.target_pos;
        return umsg::Error::OK;
    }
};

umsg::Node<decltype(Serial), /*MaxPayloadSize*/ 256, /*MaxHandlers*/ 8> node(Serial);
Robot robot;

void setup() {
    Serial.begin(115200);
    node.subscribe(kCmdChannel, &robot, &Robot::onCommand);
}

void loop() {
    node.poll();                                         // drain + dispatch

    Telemetry t{ millis(), robot.position };
    // publish() does the inverse of the dispatcher's work:
    //   1. Calls Telemetry::encode() to serialize `t` into a payload buffer.
    //   2. Builds the frame: version | msg_id | Telemetry::kMsgHash | len | payload.
    //   3. Appends CRC-32 and COBS-encodes the result, terminated by 0x00.
    //   4. Hands the finished packet to Serial.write() in one call.
    node.publish(kTelemetryChannel, t);

    delay(20);                                           // ~50 Hz telemetry
}
```

A *transport* is any type implementing the Arduino `Stream` shape:

```cpp
int    read();                                   // byte (0..255), or -1 if empty
size_t write(const uint8_t* data, size_t len);   // bytes actually written
```

A short write (`returned < len`) surfaces as `Error::TransportError`. Every
class deriving from Arduino `Stream` already satisfies this; for non-Stream
sources (Arduino UDP, POSIX sockets, custom hardware) implement the two
methods on your own type.

Complete, compilable programs for Arduino and POSIX live in
[`examples/`](examples/).

## How it works

Internally umsg is a thin stack of independently testable layers:

| Layer        | Responsibility                                                |
| ------------ | ------------------------------------------------------------- |
| Transport    | Move bytes to and from the wire                               |
| Framer       | COBS encode/decode plus CRC-32 verification                   |
| Protocol     | Stateless codec for `version / msg_id / msg_hash / len / payload` |
| Dispatcher   | Map `msg_id` to a typed handler                               |
| Node         | Glue layer that ties the above to a user transport            |

Advanced users can use `Writer` / `Reader`, COBS, CRC32, or the `Framer`
directly without going through `Node` — see
[architecture → using components directly](docs/architecture.md#using-components-directly).

## Caveats

- **Lifetime.** `Node` holds non-owning references to its transport and to
  every subscribed handler object. Both must outlive the `Node`. Most common
  pitfall: a global `Node` with a handler declared as a local in `setup()`.
- **Thread safety.** `Node` is not thread-safe. Serialize `poll()` and
  `publish()` calls externally. A handler invoked from `poll()` may safely
  call `publish()` (single-threaded re-entry); recursive `poll()` is not
  supported.

## Documentation

- [Architecture](docs/architecture.md) — layering, file roles, wire protocol, memory model.
- [Usage guide](docs/usage.md) — transports, subscribe/publish, error codes, CRC options, build instructions.
- [Schema generator](tools/umsg_gen/README.md) — `.umsg` files to C++ structs with typed `encode` / `decode`.

## Status

umsg is pre-1.0 and actively evolving. The wire protocol is stable, but the
public C++ API may change between minor versions until 1.0. Production use is
encouraged with a pinned version.

## License

Released under the MIT License — see [LICENSE](LICENSE).

## Author

Matias Bustos SM
