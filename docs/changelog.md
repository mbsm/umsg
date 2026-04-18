# Changelog

## 0.2.0 (unreleased)

Architecture pass: the monolithic `Router` was split, the `Framer` was
simplified, and names were aligned with pub/sub vocabulary. Several bugs in
POSIX transports were fixed along the way.

### Breaking API changes

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

### `Error` enum renamed for consistency (`<Subject><Problem>`)

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

### Fixes

- POSIX `SerialPort`: no more `EAGAIN` busy-spin on write (uses `poll()`);
  `EINTR` is retried; RX is buffered so `read()` no longer issues one syscall
  per byte.
- POSIX `TcpClient`: same treatment as `SerialPort`.
- POSIX `UdpSocket`: buffer indices are now `size_t` (were `ssize_t`);
  `sendto` success check no longer truncates through `ssize_t`.
- `Framer`: after `FrameOverflow`, bytes are dropped until the next `0x00`
  delimiter instead of producing a spurious `CobsInvalid`.
- `Node::publish<Msg>`: dedicated `txEncode_` scratch so the typed overload
  no longer aliases `txPacket_` during encoding.

### New

- Opt-in CRC32 tables: define `UMSG_CRC32_NIBBLE_TABLE` (64 B, ~4× faster) or
  `UMSG_CRC32_BYTE_TABLE` (1 KB, ~8× faster). Placed in `PROGMEM` on AVR.
- New stateless codec layer `umsg::protocol::{encodeFrame, decodeFrame, Header}`.

## 0.1.0

Initial release.
