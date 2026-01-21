#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <Stream.h>

namespace umsg {
namespace arduino {

/**
 * @brief Generic Arduino Stream Transport.
 * 
 * Suitable for Serial (HardwareSerial, SoftwareSerial, USBSerial)
 * or any already-connected Stream.
 */
class StreamTransport {
public:
    explicit StreamTransport(Stream& stream) : stream_(stream) {}

    bool read(uint8_t& byte) {
        if (stream_.available() > 0) {
            byte = static_cast<uint8_t>(stream_.read());
            return true;
        }
        return false;
    }

    bool write(const uint8_t* data, size_t length) {
        return stream_.write(data, length) == length;
    }

private:
    Stream& stream_;
};

} // namespace arduino
} // namespace umsg

#endif // ARDUINO
