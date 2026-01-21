#pragma once
/**
 * @file umsg.h
 * @brief Single public include for the umsg header-only library.
 *
 * Include this header to get the full public surface:
 * - common types (`bufferSpan`) and sizing helpers
 * - canonical marshalling helpers (`Writer`/`Reader`)
 * - framing (`Framer`: COBS + CRC32)
 * - routing (`Router`: frame parse + dispatch)
 * - integration (`Node`: Router + Framer + user transport)
 *
 * @defgroup umsg umsg
 * @brief Header-only embedded messaging library.
 *
 * Design constraints:
 * - C++11; suitable for freestanding/embedded toolchains (minimal runtime assumptions; 
 *  depends only on `<stdint.h>`/`<stddef.h>` and avoids OS-only facilities)
 * - No dynamic allocation; all buffers are fixed-size
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
#include "framer.hpp"
#include "router.hpp"
#include "node.hpp"

