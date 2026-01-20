umsg

library for embedded messaging
c++11, header-only, no dependencies no dynamic memory allocation

Protocol Frame format:
|version | msg_id | msg_hash|   len   |     payload    | 
| 1 byte | 1 byte | 4 bytes | 2 bytes | max_size bytes |

Sizing (compile-time):
- Frame header size is 8 bytes, so `MaxFrameSize = 8 + MaxPayloadSize`.
- Packet input to COBS is `MaxFrameSize + 4` (CRC32). Worst-case COBS expansion is `+ceil(n/254)` and packets add a `0x00` delimiter, so:
	- `n = MaxFrameSize + 4`
	- `MaxPacketSize = n + ceil(n/254) + 1`


wire packet format:
|encode(|Frame | crc32) | delimiter | 

Notes:
- COBS delimiter is `0x00`.
- All multi-byte on-wire fields are network byte order (big-endian): `msg_hash(4)`, `len(2)`, and `crc32(4)`.
- CRC is CRC-32/ISO-HDLC ("CRC-32", Ethernet/PKZIP):
	- poly: 0x04C11DB7 (reflected form 0xEDB88320)
	- init: 0xFFFFFFFF
	- refin/refout: true/true
	- xorout: 0xFFFFFFFF

Buffer ownership / lifetime:
- `Framer` owns the receive buffer used for decoded frames. Any `bufferSpan` passed to callbacks (frame/payload) points into this internal buffer.
- No copies are made during receive/dispatch. Data is only valid for the duration of the callback call stack.
  - If a handler needs the bytes later, it must copy them into its own storage before returning.
- After a packet is emitted, the framer resets its internal receive index and will start overwriting the same buffer as new bytes are processed.
- Callbacks must not call `Framer::processByte()` re-entrantly; doing so can overwrite the current frame/payload while it is still being handled.
- TX buffers (`Router::buildFrame` output and `Framer::createPacket` output) are caller-owned; those functions write into the caller-provided buffers.

objets:

Framer, handle the framing and deframing of messages using cobs. 
uses crc32 to validate integrity of the frame encoded in the message.
provides callback mechanism to notify when a complete message is received.
provides a method to create a packet from a given frame. does not know anithing about the frame but its size.

Router, responsible for create a frame in network byte order from a serialized message object.
also responsible to provide a member function callback to the framer to receive complete frames.
when a complete frame is received the router will parse the frame, validate the frame content (version, hash)
and then dispatch the deseralized message to the registered message handler callback.
Provides a method to register message handlers for specific message ids.

Node: high level object that combines a framer and a router. templated on a io object 
that provides read and write methods to an arbitrary transport layer (serial, tcp/ip, etc).
Also templated on a max message size to allocate the necessary buffers statically.




Marshaling:

Goal: deterministic, cross-platform serialization for simple message structs, with no dynamic allocation.

Implementation note (library support):
- `common.hpp` provides `umsg::bufferSpan` and sizing helpers.
- `marshalling.hpp` provides big-endian helpers and the canonical `umsg::Writer` / `umsg::Reader` primitives described below.

### Supported fields

Messages are C++11 structs containing only:
- integers: `int8_t/uint8_t`, `int16_t/uint16_t`, `int32_t/uint32_t`, `int64_t/uint64_t`
- `bool`
- floats: `float` (IEEE-754 binary32), `double` (IEEE-754 binary64)
- fixed-size arrays of the above types (e.g. `double q[4];`)

Not supported (by design): pointers, references, `std::string`, dynamic arrays/vectors, variable-length fields.

### Canonical payload encoding (network byte order)

The payload encoding is independent from the host ABI (no struct `memcpy`).

- Field order: **declaration order** in the struct.
- No padding/alignment bytes are serialized.
- Fixed-size arrays are serialized as elements in increasing index order.

Scalar encodings:
- `uint8_t/int8_t`: 1 byte.
- `uint16_t/int16_t`: 2 bytes, big-endian.
- `uint32_t/int32_t`: 4 bytes, big-endian.
- `uint64_t/int64_t`: 8 bytes, big-endian.
- `bool`: 1 byte. Must be `0x00` (false) or `0x01` (true). Any other value is invalid on decode.
- `float` / `double`: encode IEEE-754 bits as an unsigned integer of the same width, in big-endian.
	- `float`: reinterpret as `uint32_t`, write big-endian.
	- `double`: reinterpret as `uint64_t`, write big-endian.
	- Implementation note: use `memcpy` between float and integer to avoid aliasing UB.

Resulting payload size is fully determined at compile time from the struct definition.

### Schema hash (msg_hash)

Each message definition has a 32-bit schema hash used for validation/deserialization routing.

Hash input is the **canonicalized** message definition text:
- Start from the original `.umsg` file text for the struct.
- Remove comments:
	- `// ...` to end-of-line
	- `/* ... */` block comments
- Remove all ASCII whitespace characters: space, tab, CR, LF.

Hash function (recommended and simple to implement in generator): **FNV-1a 32-bit**:
- offset basis: `2166136261`
- prime: `16777619`
- process bytes of the canonicalized text (UTF-8 assumed; for ASCII input this is trivial)

If a message schema changes, its `msg_hash` must change. Receivers can reject or version-gate by hash.

### `.umsg` generator output

Messages are defined in a `.umsg` file using a restricted C++-like syntax, e.g.:

```cpp
struct state_t
{
		uint64_t timestamp;
		double p[3];  // position
		double q[4];  // quaternion
		double v[3];  // velocity
		double w[3];  // angular velocity
};
```

The generator produces a C++11 header that contains:
- The struct with the defined members.
- A constant schema hash, e.g. `static const uint32_t kMsgHash = ...;`.
- `bool encode(umsg::bufferSpan& payload)`
	- `payload.data` points to caller-owned storage.
	- `payload.length` is **capacity-in / length-out**.
	- Returns false on overflow (insufficient capacity).
- `bool decode(umsg::bufferSpan payload)`
	- Reads from `payload` and updates struct members.
	- Returns false on underflow, invalid bool values, or leftover bytes (see below).

Recommended strictness: decode should return false if `payload.length` is not exactly the expected size.

### Edge cases & strictness

- **Trailing bytes**: means `payload.length` is larger than the expected encoded size for the struct.
	- **Strict decode (recommended default)**: `decode()` returns false if there are *any* trailing bytes.
		- In Reader terms: after reading all fields, `fullyConsumed()` must be true.
	- **Permissive decode (optional policy)**: `decode()` reads the expected fields and ignores any remaining bytes.
		- This is only safe if you explicitly want forward-compat behavior (e.g., newer senders append fields and older receivers ignore them).
		- If you choose permissive decode, it must still fail on underflow (not enough bytes to read required fields).

- **Underflow/overflow**:
	- Encode fails on overflow (insufficient output capacity).
	- Decode fails on underflow (insufficient input bytes).

- **Signed integers**: encoded as their two’s-complement bit pattern in big-endian (equivalent to casting to the same-width unsigned type before writing). This assumes typical two’s-complement representation.

- **Floating point**:
	- Encoding is bit-preserving (IEEE-754 bits are transported as integers). NaNs/Infs are allowed and round-trip by bit pattern.
	- Use `memcpy` between float/double and uint32_t/uint64_t to avoid aliasing UB.

- **Bool validity**:
	- Only `0x00` and `0x01` are valid encodings.
	- Any other value must cause decode to fail.

### Writer/Reader primitives

To implement encode/decode without allocations, use simple cursor-based helpers:

- `Writer(span)`
	- maintains `index` into `span.data[0..span.length)`
	- `bool write(T value)` writes one scalar in canonical encoding
	- `bool writeArray(const T* values, size_t count)` writes `count` elements
	- `size_t bytesWritten()`

- `Reader(span)`
	- maintains `index`
	- `bool read(T& out)` reads one scalar in canonical encoding
	- `bool readArray(T* outValues, size_t count)` reads `count` elements
	- `bool fullyConsumed()` (for strict decode)

The generator should use these helpers so user structs get correct, consistent behavior.






