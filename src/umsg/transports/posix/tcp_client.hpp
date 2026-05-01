#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "detail/fd_stream.hpp"

namespace umsg {
namespace posix {

/**
 * @brief Simple POSIX TCP Client Transport.
 *
 * Read/write/close/isOpen are inherited from `detail::FdStream`. Reads are
 * non-blocking (the rx buffer coalesces one syscall into many bytes); writes
 * wait on `poll()` when the send buffer is full instead of busy-spinning.
 */
class TcpClient : public detail::FdStream<512> {
public:
    bool connect(const char* ip, uint16_t port) {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;

        struct sockaddr_in serv_addr;
        ::memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (::inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
            ::close(fd);
            return false;
        }

        if (::connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            ::close(fd);
            return false;
        }

        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == -1 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            ::close(fd);
            return false;
        }

        setFd(fd);
        return true;
    }
};

} // namespace posix
} // namespace umsg
