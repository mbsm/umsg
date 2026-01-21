#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

namespace umsg {
namespace posix {

/**
 * @brief Simple POSIX TCP Client Transport.
 * 
 * Non-blocking reads, blocking writes (by default, unless socket is tweaked).
 */
class TcpClient {
public:
    TcpClient() : fd_(-1) {}
    
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

        // Set non-blocking for reads
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            close();
            return false;
        }
        if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            close();
            return false;
        }

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
        if (n > 0) return true;
        
        // n == 0 means EOF (server closed). n < 0 means error or EAGAIN.
        // For simplicity, we just return false here (no byte).
        // Real logic might want to handle EOF differently, but Node.poll() 
        // essentially just wants "is there a byte now?".
        return false;
    }

    bool write(const uint8_t* data, size_t length) {
        if (fd_ < 0) return false;
        
        // Loop until all bytes written or error
        size_t total = 0;
        while (total < length) {
            ssize_t n = ::write(fd_, data + total, length - total);
            if (n < 0) {
                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     continue; // Spin or return false? 
                     // Blocking write behavior is requested by standard umsg semantics 
                     // ("returns true if all bytes written").
                     // But we set O_NONBLOCK. So we might need to select() properly.
                     // For this simple helper, maybe we should just fail or busy-wait.
                     // Let's retry lightly.
                     continue; 
                 }
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
