#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#include "detail/fd_stream.hpp"

namespace umsg {
namespace posix {

/**
 * @brief POSIX Serial Port Transport. Configures a tty for 8N1.
 *
 * Read/write/close/isOpen are inherited from `detail::FdStream` and provide
 * the umsg `Transport` contract.
 */
class SerialPort : public detail::FdStream<256> {
public:
    bool open(const char* device, speed_t baudRate = B115200) {
        // O_NOCTTY: don't become the controlling terminal.
        // O_NONBLOCK: non-blocking I/O from the start; avoids races vs. a later fcntl.
        int fd = ::open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) return false;

        struct termios options;
        if (::tcgetattr(fd, &options) < 0) {
            ::close(fd);
            return false;
        }

        ::cfsetispeed(&options, baudRate);
        ::cfsetospeed(&options, baudRate);

        // 8N1
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;

        // No hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // Local line, read enabled
        options.c_cflag |= (CLOCAL | CREAD);

        // Raw input/output
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag &= ~OPOST;

        if (::tcsetattr(fd, TCSANOW, &options) < 0) {
            ::close(fd);
            return false;
        }

        setFd(fd);
        return true;
    }
};

} // namespace posix
} // namespace umsg
