#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

namespace umsg {
namespace posix {

/**
 * @brief Simple POSIX TCP Client Transport.
 *
 * Non-blocking reads (buffered to avoid one syscall per byte); blocking writes
 * that wait on `poll()` when the send buffer is full instead of busy-spinning.
 */
class TcpClient {
public:
    TcpClient() : fd_(-1), bufLen_(0), bufIdx_(0) {}

    ~TcpClient() {
        close();
    }

    bool connect(const char* ip, uint16_t port) {
        if (fd_ >= 0) close();

        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        struct sockaddr_in serv_addr;
        ::memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (::inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
            close();
            return false;
        }

        if (::connect(fd_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            close();
            return false;
        }

        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            close();
            return false;
        }
        if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
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
            // n == 0: peer closed; n < 0 with EAGAIN: nothing to read now.
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
                    // Wait for send buffer space instead of busy-spinning.
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

    // Buffer incoming bytes so read() doesn't syscall per byte.
    uint8_t rxBuffer_[512];
    size_t bufLen_;
    size_t bufIdx_;
};

} // namespace posix
} // namespace umsg
