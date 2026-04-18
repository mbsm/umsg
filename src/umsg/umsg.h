#pragma once
/**
 * @file umsg.h
 * @brief Single public include for the umsg header-only library.
 *
 * Public surface:
 * - `ByteSpan`, `Error`, sizing helpers (`common.hpp`)
 * - Canonical marshalling (`Writer`/`Reader`) (`marshalling.hpp`)
 * - Byte-stream framing (COBS + CRC32) (`Framer`) (`framer.hpp`)
 * - Frame header codec (`protocol::encodeFrame` / `decodeFrame`) (`protocol.hpp`)
 * - Handler table (`Dispatcher`) (`dispatcher.hpp`)
 * - Integration (`Node`) (`node.hpp`)
 *
 * @defgroup umsg umsg
 * @brief Header-only embedded messaging library.
 *
 * Design constraints:
 * - C++11; freestanding/embedded friendly (depends only on `<stdint.h>`/`<stddef.h>`/`<string.h>`).
 * - No dynamic allocation; all buffers are fixed-size at compile time.
 *
 * Protocol overview:
 * - Frame: `version(1) | msg_id(1) | msg_hash(4) | len(2) | payload(len)`
 * - Packet: `COBS(frame || crc32) || 0x00`
 *
 * On-wire multi-byte fields are big-endian.
 */

#include "common.hpp"
#include "marshalling.hpp"
#include "cobs.hpp"
#include "crc32.hpp"
#include "protocol.hpp"
#include "framer.hpp"
#include "dispatcher.hpp"
#include "node.hpp"
