# umsg tests

This folder contains a small C++11 test suite for the `umsg` header-only library.

## Running

From the repo root:

```bash
mkdir -p build && cd build
cmake ..
make
./tests/umsg_tests
```

This compiles with:

- `-std=c++11 -Wall -Wextra -Werror -pedantic`

and runs a single test binary.

## Output format

The harness prints:

- `=== RUN <test>`: start of a logical test group
- `--- <section>`: a short description of a sub-check inside the test group
- `=== OK <test>` / `=== FAIL <test>`: summary including number of checks

Failures include `[test]` and `[section]` tags plus file/line.

## Test inventory

### [test_crc32.cpp](test_crc32.cpp)
Validates the CRC implementation used by the wire protocol.

- CRC-32/ISO-HDLC known vector: the ASCII string `"123456789"` must produce `0xCBF43926`.
- Empty input must produce `0x00000000`.

Why this matters: the Framer uses CRC to accept/reject packets; if this is wrong, no messages will decode reliably.

### [test_cobs.cpp](test_cobs.cpp)
Validates COBS encoding/decoding behavior.

- Round-trip: `decode(encode(x)) == x` for:
  - empty input
  - non-zero data
  - data containing embedded zeros
  - larger payloads that force `0xFF` code blocks
- Additional invariant: encoded bytes contain no `0x00` (the delimiter value).

Why this matters: the wire packet uses `0x00` as the delimiter, so COBS must ensure the payload never contains zeros.

### [test_framer.cpp](test_framer.cpp)
Validates the Framer framing/deframing pipeline (COBS + CRC + delimiter).

- Round-trip:
  - `createPacket(frame)` generates a valid wire packet (ends with delimiter `0x00`).
  - Feeding that packet byte-by-byte into `processByte()` eventually emits the original frame.
- CRC rejection:
  - Flipping a byte in the encoded packet causes `processByte()` to fail and the callback is not invoked.

Why this matters: this is the core integrity and packet-boundary logic.

Notes:
- The Framer is frame-agnostic: it treats the frame as bytes and does not validate `version/msg_id/len`.

### [test_router.cpp](test_router.cpp)
Validates protocol frame construction and parsing/dispatch.

- `buildFrame()` produces:
  - `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`
  - network byte order (big-endian) for `msg_hash` and `len`
- `onPacket(frame)` parsing and dispatch:
  - dispatches to the handler registered for `msg_id`
  - passes `(payloadSpan, msgHash)` to the handler
- Rejection behavior:
  - frames with unexpected `version` are ignored
  - frames where the header `len` does not match `frame.length` are ignored

Why this matters: Router is responsible for protocol correctness and for delivering payload/hash to the correct handler.

### [test_node.cpp](test_node.cpp)
Validates end-to-end integration of Transport + Framer + Router.

It constructs a fake in-memory duplex transport (two fixed-size ring buffers) and connects two `Node` instances:

- `nodeA.publish(msgId, msgHash, payload)` writes a wire packet into the A→B ring.
- `nodeB.poll()` drains bytes from its transport, feeding them into its Framer.
- When a full packet is received and CRC passes, the Framer calls `Router::onPacket()`, and Router dispatches to the registered handler.

Assertions:
- `nodeA.ok()` / `nodeB.ok()` are true (Framer→Router callback wiring succeeded).
- `nodeB` handler is called exactly once.
- Handler receives the expected `msgHash` and the exact payload bytes (including an embedded `0x00` to exercise COBS).

Why this matters: Node is the “user-facing” integration point; this test proves the pieces work together without OS I/O or third-party dependencies.

### [test_main.cpp](test_main.cpp) and [test_harness.hpp](test_harness.hpp)
Infrastructure only.

- Defines the minimal test runner and verbose output format.
- Provides simple `EXPECT_*` macros, check counting, and contextual failure reporting.
