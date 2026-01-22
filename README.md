# umsg

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Standard: C++11](https://img.shields.io/badge/Standard-C%2B%2B11-blue.svg)
![Status: Beta](https://img.shields.io/badge/Status-Beta-orange.svg)

`umsg` is a minimal, header-only C++11 embedded communication library designed for constrained systems.

It provides a robust, zero-allocation pipeline for framing, routing, and marshaling simple message structs over arbitrary byte-stream transports (UART, TCP, etc.).

## Key Features

- **Freestanding-friendly**: Depends only on `<stdint.h>` and `<stddef.h>`. No OS calls, threads, or filesystem access.
- **Zero dynamic allocation**: All buffers are fixed-size and caller-provided or statically allocated.
- **Header-only**: Easy integration; just drop the headers into your project.
- **Robust Framing**: Uses **COBS** (Consistent Overhead Byte Stuffing) + **CRC32** to ensure message integrity and recovery from stream corruption.
- **Type-Safe Dispatch**: Dispatch messages to member function handlers based on IDs and Schema Hashes.
- **Canonical Marshaling**: Schema-defined serialization ensures endian-independence (Network Byte Order on wire) and consistent padding.

## Architecture

The library is composed of three main layers which can be used together or independently, plus an optional code generator:

1. **Framer** (`framer.hpp`): Handles the raw byte stream. Wraps frames with COBS encoding and verifies integrity with CRC32.
2. **Router** (`router.hpp`): Parses verified frames and dispatches them to registered handlers based on `msg_id`.
3. **Node** (`node.hpp`): The high-level integration point. Combines a Framer and Router with a user-provided Transport.
4. **Generator** (`umsg_gen.py`): (Optional) Python tool that generates type-safe C++ structs from `.umsg` schema files.

## Workflow with the Generator

Using the generator is the **highly recommended** way to use `umsg`. It automates the error-prone work of serializing/deserializing bytes and calculating schema hashes.

### 1. Define Protocol (`messages.umsg`)

Create a simple schema file defining your data structures.

```cpp
// messages.umsg
struct Telemetry
{
    uint64_t timestamp_us;
    double position[3];
    float battery_voltage;
    bool is_armed;
};

struct Command
{
    uint8_t mode;
    double target_pos[3];
};
```

### 2. Generate C++ Code

Run the generator script included in `tools/`.

```bash
python3 tools/umsg_gen/umsg_gen.py messages.umsg -o generated/
```

This acts as your "single source of truth". If you change a struct, the tool regenerates the code and updates the **Schema Hash**. This hash (FNV-1a 32-bit of the schema definition) is sent with every message, allowing receivers to reject mismatched versions automatically.

### 3. Use in Application

The generated headers integrate directly with `umsg::Node`.

```cpp
#include <umsg/umsg.h>
#include "generated/Telemetry.hpp"
#include "generated/Command.hpp"

// ... Transport definition (see below) ...

// Application logic
class Robot {
public:
    // Type-safe handler: receives deserialized message
    umsg::Error handleCommand(const Command& cmd) {
        // Schema verification and decoding are handled automatically by the Node.
        // If versions mismatch or decoding fails, this handler is NOT called.
        
        moveTo(cmd.target_pos);
        return umsg::Error::OK;
    }
};

int main() {
    MyTransport transport;
    umsg::Node<MyTransport, 256, 8> node(transport);

    Robot robot;
    
    // Register type-safe handler for Message ID 10
    // The node will automatically check Command::kMsgHash and decode the payload.
    if (node.registerHandler(10, &robot, &Robot::handleCommand) != umsg::Error::OK) {
        // Handle registration error
    }

    while (true) {
        node.poll(); // Drain RX buffer

        // Send Telemetry (Message ID 11)
        Telemetry telem;
        telem.timestamp_us = 123456789;
        telem.battery_voltage = 12.4f;
        telem.is_armed = true;
        
        // Serialize and publish in one step
        node.publish(11, telem);
    }
}
```

## Quick Start (Manual Usage)

If you prefer not to use the generator, you can use the raw Node API directly.

### 1. Define your transport
You need a class that implements `read` and `write`.

```cpp
struct MyTransport {
    // Return true if a byte was read, false if no data available (non-blocking)
    bool read(uint8_t& byte) { 
        if (uart_is_empty()) return false;
        byte = uart_read();
        return true;
    }

    // Return true if write succeeded
    bool write(const uint8_t* data, size_t len) {
        return uart_write_buffer(data, len);
    }
};
```

### 2. Setup the Node

```cpp
int main() {
    MyTransport transport;
    // Node<Transport, MaxPayloadSize, MaxHandlers>
    umsg::Node<MyTransport, 64, 8> node(transport);

    if (!node.ok()) return -1; // Initialization failed

    while (true) {
        // RX: Poll the transport periodically. Returns number of errors (if any).
        size_t errors = node.poll();
        (void)errors; // Ignore or log errors

        // TX: Send a raw byte buffer (ID 10, Hash 0x1234)
        uint8_t rawData[] = {0x01, 0x02, 0x03}; 
        if (node.publish(10, 0x12345678, umsg::bufferSpan{rawData, 3}) != umsg::Error::OK) {
             // Handle transmission error
        }
    }
}
```

## Design & Protocol Specs

**Constraints**: C++11, header-only, no dependencies, no dynamic memory allocation.

### Protocol Frame Format (Application Layer)
```
| version | msg_id | msg_hash |   len   | payload        |
| 1 byte  | 1 byte | 4 bytes  | 2 bytes | max_size bytes |
```
- **version**: Protocol version (currently 1).
- **msg_id**: 8-bit identifier for the message topic.
- **msg_hash**: 32-bit hash of the message schema (FNV-1a), used for type safety.
- **len**: 16-bit payload length.

### Wire Packet Format (Link Layer)
```
| COBS_Encode(Frame | crc32) | delimiter |
```
- **Framing**: [COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) ensures no delimiter byte (`0x00`) appears in the payload.
- **Integrity**: Standard CRC-32 (ISO-HDLC) covers the entire frame.
- **Endianness**: All multi-byte wire fields are Big-Endian (Network Byte Order).

### Buffer Lifetime
- **RX**: Data passed to callbacks is **zero-copy**. It points directly into the Framer's internal buffer. It is valid **only** for the duration of the callback.
- **TX**: The user provides the data; `node.publish` copies it into an internal transmit buffer before sending to avoid lifetime issues during transport write.

## Integration

Since `umsg` is header-only, integration is straightforward:

1. Copy the contents of the `src/` folder to your project's include path.
2. Include `<umsg/umsg.h>` in your source.

## Development & Testing

This repository includes a dependency-free test suite.

```bash
# Build and run all tests
mkdir -p build && cd build
cmake ..
make
./tests/umsg_tests
```

## License

MIT License. See [LICENSE](LICENSE) for details.
