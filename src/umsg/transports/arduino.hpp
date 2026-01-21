#pragma once

// This header is intended for Arduino environments only.
#ifdef ARDUINO
#include <Arduino.h>
#include <Stream.h>

namespace umsg {

/**
 * @brief Adapter to use any Arduino Stream (HardwareSerial, SoftwareSerial, etc.) as a umsg Transport.
 *
 * Usage:
 *   umsg::ArduinoStream transport(Serial);
 *   umsg::Node<umsg::ArduinoStream, ...> node(transport);
 */
class ArduinoStream {
public:
    explicit ArduinoStream(Stream& stream) : stream_(stream) {}

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

} // namespace umsg

#endif
