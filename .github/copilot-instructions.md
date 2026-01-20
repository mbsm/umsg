# Copilot instructions for this repo (umsg)

## Repo purpose + constraints
- `umsg` is a small embedded messaging library.
- Target: C++11, header-only, no dependencies, **no dynamic allocation** (static buffers only). See [design.md](../design.md).

## Big picture (from design spec)
- Protocol frame (logical): `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`.
- Wire packet: `COBS( Frame | crc32 ) | delimiter` (delimiter is `0x00`).
- Intended components:
  - `Framer`: byte-stream framing/deframing using COBS + CRC32; emits complete frames.
  - `Router`: validates frame fields and dispatches to message handlers.
  - `Node`: composes `Framer` + `Router` with a transport providing `read`/`write`.

Note: `Framer`, `Router`, and `Node` are header-only and implemented in this repo.

Update: `Router` is now implemented in `router.hpp`.

## Codebase conventions (what to preserve)
- Keep everything header-only and freestanding-friendly (`<stdint.h>`, `<stddef.h>`).
- `umsg.h` is the primary public include; it pulls in `Framer`, `Router`, `Node`, and helpers.
- Prefer explicit sizes (`uint8_t`, `size_t`) and fixed-size buffers templated by max size.
- Use spans instead of owning containers: `umsg::bufferSpan { uint8_t* data; size_t length; }`.
- Common utilities live in `common.hpp` (e.g., `bufferSpan`, size helpers).
- Endian helpers and marshaling primitives live in `marshalling.hpp`.

## `Framer` API + callback pattern
- `template <size_t MaxPacketSize> class Framer` owns `rxBuffer_[MaxPacketSize]` and `rxIndex_`.
- Incoming bytes flow through `processByte(uint8_t byte)`; when a packet/frame is complete, it calls an internal `emitPacket(umsg::bufferSpan frame)`.
- Callback registration is **member-function only** using type erasure + a thunk:
  - Use `registerOnPacketCallback(obj, &T::method)` where method signature is `void (T::*)(umsg::bufferSpan frame)`.
  - Implementation stores the member-function pointer bytes (no `void*` casts); registration can fail if the platform’s member-pointer representation exceeds the fixed buffer.
  - Don’t change the callback mechanism unless necessary; it’s designed to avoid allocations and `std::function`.

## Buffer ownership + lifetime
- RX: `Framer` owns the decoded-frame buffer; `bufferSpan` arguments in `Framer`/`Router` callbacks alias this internal storage.
- Spans passed to callbacks are only valid **during the callback call stack**; copy out if you need to keep data.
- Avoid calling `Framer::processByte()` from inside an `onPacket`/handler callback (re-entrancy would overwrite the same RX buffer).
- TX: output buffers passed into `Router::buildFrame` and `Framer::createPacket` are caller-owned (capacity-in/length-out convention).

## When adding missing pieces
- Implementations should stay consistent with the design spec in [design.md](../design.md): COBS framing, CRC32 integrity, network byte order for on-wire fields.
- CRC parameters are fixed: CRC-32/ISO-HDLC (poly `0x04C11DB7`, init `0xFFFFFFFF`, refin/refout true/true, xorout `0xFFFFFFFF`); encode `crc32(4)` in network byte order (big-endian).
- Any new buffers should be sized at compile time (templated max sizes) and stored on the object, not heap-allocated.
- Prefer returning `bool` for parse/build success like the existing `Framer` API.

## `Router` (frame parsing + dispatch)
- `router.hpp` parses `version|msg_id|msg_hash|len|payload` and dispatches by `msg_id`.
- Handler signature is `void (T::*)(umsg::bufferSpan payload, uint32_t msgHash)`; handler is responsible for validating `msgHash` and deserializing payload.

## `Node` (Framer + Router + transport)
- `node.hpp` composes `Framer` + `Router` and wires `Framer` -> `Router::onPacket`.
- Expects a transport with `read(uint8_t& byte) -> bool` and `write(const uint8_t* data, size_t len) -> bool`.
- TX is typically `Node::publish(msgId, msgHash, payload)` (build frame -> create packet -> write).
