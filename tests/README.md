# umsg tests

A small, dependency-free C++11 test suite for the `umsg` header-only library.

## Running

From the repo root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or run the binary directly for the per-check output:

```bash
./build/tests/umsg_tests
```

Compiles with `-std=c++11 -Wall -Wextra -Werror -pedantic`.

## Output format

- `=== RUN <test>` — start of a logical test group
- `--- <section>` — a single check or sub-check within the group
- `=== OK <test>` / `=== FAIL <test>` — per-group summary (check count)
- Failures include `[test]` and `[section]` tags plus `file:line`

## Test inventory

### [test_crc32.cpp](test_crc32.cpp)
CRC-32/ISO-HDLC correctness against known vectors.

- `"123456789"` → `0xCBF43926`
- empty input → `0x00000000`

Why it matters: framing decisions depend on CRC. The same vectors run against
the default, `UMSG_CRC32_NIBBLE_TABLE`, and `UMSG_CRC32_BYTE_TABLE` builds.

### [test_cobs.cpp](test_cobs.cpp)
COBS encode/decode round-trips.

- `decode(encode(x)) == x` for empty, non-zero, embedded-zero, and long inputs (forces `0xFF` blocks)
- Encoded bytes never contain `0x00`

Why it matters: `0x00` is the packet delimiter, so COBS must never produce one.

### [test_framer.cpp](test_framer.cpp)
COBS + CRC framing pipeline via `Framer::encode` and `Framer::feed`.

- Round-trip: `encode(frame) → feed(byte)*` recovers the original frame via `Result{complete=true, frame}`
- CRC rejection: flipping one byte in the encoded packet produces `Error::CrcInvalid` and no `complete` result

Framer is frame-agnostic: it does not validate the inner protocol header.

### [test_dispatcher.cpp](test_dispatcher.cpp)
Frame codec (`protocol::encodeFrame` / `decodeFrame`) and handler dispatch (`Dispatcher`).

- `encodeFrame` emits `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload` in big-endian
- `decodeFrame` rejects `LengthMismatch`
- `Dispatcher::dispatch` routes by `msg_id` and returns `HandlerNotFound` for unknown ids
- Typed handlers verify `Msg::kMsgHash` (returning `HashMismatch` on mismatch) and auto-decode

### [test_node.cpp](test_node.cpp)
End-to-end integration with an in-memory duplex transport.

- `nodeA.publish(msg_id, msg_hash, payload)` writes a wire packet into the A→B ring
- `nodeB.poll()` drains the ring, runs the Framer + Dispatcher pipeline, invokes the handler
- The handler receives the exact payload (including an embedded `0x00` to exercise COBS) and the hash

### [test_marshal.cpp](test_marshal.cpp)
`Writer` / `Reader` round-trips for scalars and arrays; big-endian endian helpers.

### [test_main.cpp](test_main.cpp) / [test_harness.hpp](test_harness.hpp)
Minimal test runner, `EXPECT_*` macros, check counting, contextual failure reporting.
