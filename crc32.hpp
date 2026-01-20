#pragma once
#include <stddef.h>
#include <stdint.h>

#include "umsg_common.hpp"

/**
 * @file crc32.hpp
 * @brief CRC-32/ISO-HDLC implementation used by the wire protocol.
 */

namespace umsg
{
    /**
     * @brief Compute CRC-32/ISO-HDLC (aka "CRC-32", Ethernet/PKZIP).
     *
     * Parameters: init=0xFFFFFFFF, refin/refout=true, xorout=0xFFFFFFFF.
     */
    inline uint32_t crc32_iso_hdlc(const uint8_t *data, size_t length)
    {
        // CRC-32/ISO-HDLC ("CRC-32", Ethernet/PKZIP)
        // refin/refout: true/true => reflected algorithm with poly 0xEDB88320
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < length; ++i)
        {
            crc ^= static_cast<uint32_t>(data[i]);
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (crc & 1u)
                {
                    crc = (crc >> 1) ^ 0xEDB88320u;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        return crc ^ 0xFFFFFFFFu;
    }

}
