# Architecture

**`Node` is the entry point.** The library is built as a stack of small layers
for separation of concerns, but in almost every application you instantiate a
`Node` and use its `subscribe` / `publish` / `poll` API. A short list of cases
for using the underlying building blocks directly is at the end of this page.

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

## Files

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

## Wire protocol

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

## Memory model and lifetimes

- **Compile-time allocation.** Every RX/TX buffer is sized by a template parameter.
  `Node` owns `txEncode_` (payload scratch), `txFrame_` (built frame),
  `txPacket_` (COBS-encoded packet), and `Framer`'s `rxBuffer_[MaxPacketSize]`.
- **RX zero-copy.** Handler `ByteSpan`s alias `Framer::rxBuffer_` — valid only
  for the duration of the dispatch call. Copy bytes out if you need to retain them.
- **TX.** `publish()` copies the payload into internal scratch before transmitting,
  so the caller's buffer can be reused immediately.
- **Reentrancy.** Do *not* call `poll()` or `publish()` recursively from a handler.

## Using components directly

Most users never need this. `Node` is the supported surface. The layers below
are separable, but only a few are genuinely useful in isolation:

- **`Writer` / `Reader` (`marshalling.hpp`)** — big-endian cursor serializers
  for hand-written message structs. The schema generator emits code that uses
  these; you can also write your own `encode` / `decode` with them.
- **`cobsEncode` / `cobsDecodeInPlace` (`cobs.hpp`)** — standalone COBS
  utilities for byte-stuffed protocols other than umsg's.
- **`crc32_iso_hdlc` (`crc32.hpp`)** — a plain CRC-32/ISO-HDLC function.
- **`Framer<MaxPacketSize>` (`framer.hpp`)** — stream framing (COBS + CRC) on
  arbitrary bytes, e.g. to tunnel a different protocol with reliable packet
  boundaries. `feed(byte)` returns `Result{status, complete, frame}`;
  `encode(frame, packet)` produces a wire packet.

`protocol::encodeFrame` / `decodeFrame` and `Dispatcher` are intentionally not
in the above list — they're implementation details of `Node` with no
compelling standalone use. If you need them without `Node`, you've probably
built something that should just be another transport under `Node` instead.
