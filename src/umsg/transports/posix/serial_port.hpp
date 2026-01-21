#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <errno.h>

namespace umsg {
namespace posix {

/**
 * @brief POSIX Serial Port Transport.
 * 
 * Configures a serial port (tty) for 8N1 communication.
 */
class SerialPort {
public:
    SerialPort() : fd_(-1) {}
    
    ~SerialPort() {
        close();
    }

    bool open(const char* device, speed_t baudRate = B115200) {
        if (fd_ >= 0) close();

        // O_NOCTTY: This terminal device will not become the controlling terminal for the process
        // O_NDELAY: Ignore DCD signal info
        fd_ = ::open(device, O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ < 0) return false;

        // Configure standard blocking behavior for now, will toggle off later for read
        ::fcntl(fd_, F_SETFL, 0);

        struct termios options;
        if (::tcgetattr(fd_, &options) < 0) {
            close();
            return false;
        }

        // Set baud rate
        ::cfsetispeed(&options, baudRate);
        ::cfsetospeed(&options, baudRate);

        // 8N1
        options.c_cflag &= ~PARENB; // No Parity
        options.c_cflag &= ~CSTOPB; // 1 Stop Bit
        options.c_cflag &= ~CSIZE;  // Mask character size bits
        options.c_cflag |= CS8;     // 8 Data Bits

        // Disable hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // Local line, read enabled
        options.c_cflag |= (CLOCAL | CREAD);

        // Raw input (no canonical mode, no echo, etc.)
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        // Raw output
        options.c_oflag &= ~OPOST;

        // Apply
        if (::tcsetattr(fd_, TCSANOW, &options) < 0) {
            close();
            return false;
        }

        // Set non-blocking for polling
        int flags = ::fcntl(fd_, F_GETFL, 0);
        ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool isOpen() const { return fd_ >= 0; }

    bool read(uint8_t& byte) {
        if (fd_ < 0) return false;
        ssize_t n = ::read(fd_, &byte, 1);
        return (n > 0);
    }

    bool write(const uint8_t* data, size_t length) {
        if (fd_ < 0) return false;
        
        size_t total = 0;
        while (total < length) {
            ssize_t n = ::write(fd_, data + total, length - total);
            if (n < 0) {
                if (errno == EAGAIN) continue;
                return false;
            }
            total += n;
        }
        return true;
    }

private:
    int fd_;
};

} // namespace posix
} // namespace umsg
