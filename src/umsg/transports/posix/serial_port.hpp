#pragma once

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

namespace umsg {
namespace posix {

/**
 * @brief POSIX Serial Port Transport.
 *
 * Configures a serial port (tty) for 8N1 communication.
 */
class SerialPort {
public:
    SerialPort() : fd_(-1), bufLen_(0), bufIdx_(0) {}

    ~SerialPort() {
        close();
    }

    bool open(const char* device, speed_t baudRate = B115200) {
        if (fd_ >= 0) close();

        // O_NOCTTY: do not become the controlling terminal for this process.
        // O_NONBLOCK: non-blocking I/O from the start; avoids races vs. a later fcntl.
        fd_ = ::open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) return false;

        struct termios options;
        if (::tcgetattr(fd_, &options) < 0) {
            close();
            return false;
        }

        ::cfsetispeed(&options, baudRate);
        ::cfsetospeed(&options, baudRate);

        // 8N1
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;

        // Disable hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // Local line, read enabled
        options.c_cflag |= (CLOCAL | CREAD);

        // Raw input/output
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag &= ~OPOST;

        if (::tcsetattr(fd_, TCSANOW, &options) < 0) {
            close();
            return false;
        }

        bufLen_ = 0;
        bufIdx_ = 0;
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        bufLen_ = 0;
        bufIdx_ = 0;
    }

    bool isOpen() const { return fd_ >= 0; }

    bool read(uint8_t& byte) {
        if (fd_ < 0) return false;

        if (bufIdx_ < bufLen_) {
            byte = rxBuffer_[bufIdx_++];
            return true;
        }

        ssize_t n;
        do {
            n = ::read(fd_, rxBuffer_, sizeof(rxBuffer_));
        } while (n < 0 && errno == EINTR);

        if (n <= 0) {
            return false;
        }

        bufLen_ = static_cast<size_t>(n);
        bufIdx_ = 0;
        byte = rxBuffer_[bufIdx_++];
        return true;
    }

    bool write(const uint8_t* data, size_t length) {
        if (fd_ < 0) return false;

        size_t total = 0;
        while (total < length) {
            ssize_t n = ::write(fd_, data + total, length - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Wait until the tty accepts more data instead of busy-spinning.
                    struct pollfd pfd;
                    pfd.fd = fd_;
                    pfd.events = POLLOUT;
                    int pr;
                    do {
                        pr = ::poll(&pfd, 1, -1);
                    } while (pr < 0 && errno == EINTR);
                    if (pr < 0) return false;
                    continue;
                }
                return false;
            }
            total += static_cast<size_t>(n);
        }
        return true;
    }

private:
    int fd_;

    // Buffer incoming bytes to avoid one syscall per byte.
    uint8_t rxBuffer_[256];
    size_t bufLen_;
    size_t bufIdx_;
};

} // namespace posix
} // namespace umsg
