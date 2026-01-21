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
 * Supports reading/writing datagrams. 
 * Note: umsg is a stream protocol (COBS framed), so it technically survives
 * fragmentation, but UDP packet boundaries do not necessarily map 1:1 to umsg frames.
 * However, `read(byte)` interface abstracts that away.
 */
class UdpSocket {
public:
    UdpSocket() : fd_(-1), bufLen_(0), bufIdx_(0) {}
    
    ~UdpSocket() {
        close();
    }

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

        makeNonBlocking();
        return true;
    }

    // Set a default destination for write()
    void setDestination(const char* ip, uint16_t port) {
        memset(&destAddr_, 0, sizeof(destAddr_));
        destAddr_.sin_family = AF_INET;
        destAddr_.sin_port = htons(port);
        inet_pton(AF_INET, ip, &destAddr_.sin_addr);
        hasDest_ = true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // Buffer incoming datagrams to satisfy the byte-by-byte read interface
    bool read(uint8_t& byte) {
        if (fd_ < 0) return false;

        // If we have data buffered from the last packet, serve it
        if (bufIdx_ < bufLen_) {
            byte = rxBuffer_[bufIdx_++];
            return true;
        }

        // Try to read a new packet
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);
        ssize_t len = ::recvfrom(fd_, rxBuffer_, sizeof(rxBuffer_), 0, (struct sockaddr*)&sender, &slen);
        
        if (len > 0) {
            // We got a packet
            bufLen_ = len;
            bufIdx_ = 0;
            byte = rxBuffer_[bufIdx_++];
            // Optional: capture sender addr if we want to reply? 
            // For now, simple fixed dest topology.
            return true;
        }

        return false;
    }

    bool write(const uint8_t* data, size_t length) {
        if (fd_ < 0 || !hasDest_) return false;
        
        ssize_t sent = ::sendto(fd_, data, length, 0, (struct sockaddr*)&destAddr_, sizeof(destAddr_));
        return (sent == (ssize_t)length);
    }

private:
    void makeNonBlocking() {
        if (fd_ < 0) return;
        int flags = ::fcntl(fd_, F_GETFL, 0);
        ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    }

    int fd_;
    struct sockaddr_in destAddr_;
    bool hasDest_ = false;

    // UDP is datagram based, but Node expects a stream of bytes.
    // We must buffer the current datagram.
    uint8_t rxBuffer_[4096];
    ssize_t bufLen_;
    ssize_t bufIdx_;
};

} // namespace posix
} // namespace umsg
