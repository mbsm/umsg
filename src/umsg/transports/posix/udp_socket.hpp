#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

namespace umsg {
namespace posix {

/**
 * @brief Simple POSIX UDP Socket Transport.
 *
 * Datagrams are buffered internally so the byte-at-a-time `read()` contract
 * is preserved. UDP packet boundaries do not have to align with umsg frame
 * boundaries — COBS resyncs at the next `0x00`.
 */
class UdpSocket {
public:
    UdpSocket() : fd_(-1), bufLen_(0), bufIdx_(0) {}

    ~UdpSocket() {
        close();
    }

    // Non-copyable, non-movable: copying would alias `fd_` and double-close.
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) = delete;
    UdpSocket& operator=(UdpSocket&&) = delete;

    // Bind to a local port to receive packets
    bool bind(uint16_t port) {
        if (fd_ >= 0) close();

        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (::bind(fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        if (!makeNonBlocking()) {
            close();
            return false;
        }
        return true;
    }

    /**
     * @brief Set the default destination for write().
     * @return true if @p ip parsed as a valid IPv4 address; false otherwise
     *         (in which case the destination is left unset and write() will fail).
     */
    bool setDestination(const char* ip, uint16_t port) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            return false;
        }
        destAddr_ = addr;
        hasDest_ = true;
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int read() {
        if (fd_ < 0) return -1;

        if (bufIdx_ < bufLen_) {
            return rxBuffer_[bufIdx_++];
        }

        ssize_t len = ::recvfrom(fd_, rxBuffer_, sizeof(rxBuffer_), 0, nullptr, nullptr);
        if (len <= 0) return -1;

        bufLen_ = static_cast<size_t>(len);
        bufIdx_ = 0;
        return rxBuffer_[bufIdx_++];
    }

    size_t write(const uint8_t* data, size_t length) {
        if (fd_ < 0 || !hasDest_) return 0;

        ssize_t sent = ::sendto(fd_, data, length, 0, (struct sockaddr*)&destAddr_, sizeof(destAddr_));
        return (sent < 0) ? 0 : static_cast<size_t>(sent);
    }

private:
    bool makeNonBlocking() {
        if (fd_ < 0) return false;
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1) return false;
        return ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    int fd_;
    struct sockaddr_in destAddr_;
    bool hasDest_ = false;

    uint8_t rxBuffer_[4096];
    size_t bufLen_;
    size_t bufIdx_;
};

} // namespace posix
} // namespace umsg
