#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <Client.h>

namespace umsg {
namespace arduino {

/**
 * @brief Arduino TCP Client Transport.
 * 
 * Wraps any Client-derived class (EthernetClient, WiFiClient).
 * Provides read/write and exposes connect/stop for lifecycle management.
 */
template <typename ClientClass>
class TcpClientTransport {
public:
    TcpClientTransport(ClientClass& client) : client_(client) {}

    bool connect(IPAddress ip, uint16_t port) {
        return client_.connect(ip, port);
    }
    
    bool connect(const char* host, uint16_t port) {
        return client_.connect(host, port);
    }

    void disconnect() {
        client_.stop();
    }

    bool isConnected() {
        return client_.connected();
    }

    bool read(uint8_t& byte) {
        if (client_.available() > 0) {
            byte = (uint8_t)client_.read();
            return true;
        }
        return false;
    }

    bool write(const uint8_t* data, size_t length) {
        if (!client_.connected()) return false;
        return (client_.write(data, length) == length);
    }

private:
    ClientClass& client_;
};

} // namespace arduino
} // namespace umsg

#endif // ARDUINO
