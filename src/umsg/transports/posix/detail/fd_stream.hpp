#pragma once

#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

namespace umsg {
namespace posix {
namespace detail {

/**
 * @brief Shared umsg-Transport implementation over a non-blocking POSIX file
 *        descriptor. Used by `SerialPort` and `TcpClient`.
 *
 * Owns the fd, an RX ring big enough to coalesce one syscall into many bytes,
 * and the EAGAIN/EINTR retry logic. Derived classes only have to acquire the
 * fd (via `open()` / `connect()` / etc.) and pass it to `setFd()`.
 *
 * @tparam RxBufferSize Bytes coalesced per `::read` syscall.
 */
template <size_t RxBufferSize>
class FdStream {
public:
    FdStream() : fd_(-1), bufLen_(0), bufIdx_(0) {}

    ~FdStream() { closeFd(); }

    bool isOpen() const { return fd_ >= 0; }

    void close() { closeFd(); }

    int read() {
        if (fd_ < 0) return -1;

        if (bufIdx_ < bufLen_) {
            return rxBuffer_[bufIdx_++];
        }

        ssize_t n;
        do {
            n = ::read(fd_, rxBuffer_, sizeof(rxBuffer_));
        } while (n < 0 && errno == EINTR);

        if (n <= 0) {
            return -1;
        }

        bufLen_ = static_cast<size_t>(n);
        bufIdx_ = 0;
        return rxBuffer_[bufIdx_++];
    }

    size_t write(const uint8_t* data, size_t length) {
        if (fd_ < 0) return 0;

        size_t total = 0;
        while (total < length) {
            ssize_t n = ::write(fd_, data + total, length - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    struct pollfd pfd;
                    pfd.fd = fd_;
                    pfd.events = POLLOUT;
                    int pr;
                    do {
                        pr = ::poll(&pfd, 1, -1);
                    } while (pr < 0 && errno == EINTR);
                    if (pr < 0) return total;
                    continue;
                }
                return total;
            }
            total += static_cast<size_t>(n);
        }
        return total;
    }

protected:
    void setFd(int fd) {
        closeFd();
        fd_ = fd;
    }

    int fd() const { return fd_; }

private:
    void closeFd() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        bufLen_ = 0;
        bufIdx_ = 0;
    }

    int fd_;
    uint8_t rxBuffer_[RxBufferSize];
    size_t bufLen_;
    size_t bufIdx_;
};

} // namespace detail
} // namespace posix
} // namespace umsg
