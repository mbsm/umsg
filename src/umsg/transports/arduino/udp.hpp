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

    int read() {
        if (udp_.available() > 0) {
            return udp_.read();
        }
        if (udp_.parsePacket() > 0 && udp_.available() > 0) {
            return udp_.read();
        }
        return -1;
    }

    size_t write(const uint8_t* data, size_t length) {
        if (udp_.beginPacket(destIp_, destPort_) != 1) {
            return 0;
        }
        size_t written = udp_.write(data, length);
        if (udp_.endPacket() != 1) {
            return 0;
        }
        return written;
    }

private:
    UdpClass& udp_;
    IPAddress destIp_;
    uint16_t destPort_;
};

} // namespace arduino
} // namespace umsg

#endif // ARDUINO
