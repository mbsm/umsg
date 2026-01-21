#pragma once

#ifdef ARDUINO
#include <Arduino.h>

namespace umsg {
namespace arduino {

/**
 * @brief Arduino UDP Transport.
 * 
 * Adapts an Arduino UDP object (EthernetUDP, WiFiUDP, etc.) to umsg.
 * 
 * Note: umsg is stream-oriented. Mapping to UDP packets requires decisions:
 * - Writes are wrapped in beginPacket/endPacket immediately (one write call = one UDP packet).
 * - Reads consume the current packet buffer.
 */
template <typename UdpClass>
class UdpTransport {
public:
    UdpTransport(UdpClass& udp, IPAddress destIp, uint16_t destPort)
        : udp_(udp), destIp_(destIp), destPort_(destPort) {}

    // Update destination if needed
    void setDestination(IPAddress ip, uint16_t port) {
        destIp_ = ip;
        destPort_ = port;
    }

    bool read(uint8_t& byte) {
        // If we have bytes left in the current packet, return one
        if (udp_.available() > 0) {
            byte = (uint8_t)udp_.read();
            return true;
        }

        // Try to parse a new packet
        if (udp_.parsePacket() > 0) {
            if (udp_.available() > 0) {
                byte = (uint8_t)udp_.read();
                return true;
            }
        }

        return false;
    }

    bool write(const uint8_t* data, size_t length) {
        if (udp_.beginPacket(destIp_, destPort_) != 1) {
            return false;
        }
        size_t written = udp_.write(data, length);
        if (udp_.endPacket() != 1) {
            return false;
        }
        return (written == length);
    }

private:
    UdpClass& udp_;
    IPAddress destIp_;
    uint16_t destPort_;
};

} // namespace arduino
} // namespace umsg

#endif // ARDUINO
