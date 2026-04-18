# umsg

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Standard: C++11](https://img.shields.io/badge/Standard-C%2B%2B11-blue.svg)
![Status: Beta](https://img.shields.io/badge/Status-Beta-orange.svg)

**Author:** Matias Bustos SM

Minimal, header-only C++11 messaging for embedded systems. Zero heap, COBS +
CRC32 framing, typed pub/sub over any byte-stream transport (UART, TCP, UDP).

`umsg::Node` is the entry point. You plug in a transport, subscribe handlers
to message ids, and call `poll()` / `publish()`.

- **Freestanding-friendly** — only `<stdint.h>`, `<stddef.h>`, `<string.h>`.
- **Zero runtime allocation** — every buffer sized at compile time.
- **Robust framing** — COBS + CRC-32/ISO-HDLC, resync on errors.
- **Typed dispatch** — schema-hash-checked handlers; no manual decoding.
- **Ready-to-use transports** — Arduino `Stream`/UDP/TCP and POSIX serial/UDP/TCP.

---

## Hello world

```cpp
#include <umsg/umsg.h>

struct Robot {
    umsg::Error onCommand(const Command& cmd) {
        moveTo(cmd.target_pos);          // schema + decode already checked
        return umsg::Error::OK;
    }
};

MyTransport transport;
umsg::Node<MyTransport, /*MaxPayloadSize*/ 256, /*MaxHandlers*/ 8> node(transport);

Robot robot;
node.subscribe(10, &robot, &Robot::onCommand);

while (true) {
    node.poll();                         // drain, decode, dispatch
    node.publish(11, Telemetry{now_us(), ...});
}
```

See [`examples/`](examples/) for complete Arduino and POSIX programs.

Advanced users can also use a handful of components on their own —
`Writer`/`Reader`, COBS, CRC32, and `Framer` — see
[architecture → using components directly](docs/architecture.md#using-components-directly).

---

## Documentation

- [Architecture](docs/architecture.md) — layering, file roles, wire protocol, memory model
- [Usage guide](docs/usage.md) — transports, subscribe/publish, error codes, CRC options, build
- [Schema generator](tools/umsg_gen/README.md) — `.umsg` → C++ structs with typed `encode`/`decode`
- [Changelog](docs/changelog.md)

---

## Quick install

1. Add `src/` to your include path and `#include <umsg/umsg.h>`.
2. Arduino: drop the repo into `Arduino/libraries/` (it's a valid library).

Build and run the tests:

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Status

Pre-1.0 and actively evolving — APIs may change between minor versions.
See the [changelog](docs/changelog.md) for migration notes.

## License

MIT — see [LICENSE](LICENSE).
