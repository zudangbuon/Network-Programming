#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdint>
#include <string>

namespace ft {

inline bool read_n(int fd, void* buf, size_t n) {
    char* p = static_cast<char*>(buf);
    size_t off = 0;
    while (off < n) {
        ssize_t r = ::recv(fd, p + off, n - off, 0);
        if (r == 0) return false;
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<size_t>(r);
    }
    return true;
}

inline bool write_n(int fd, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    size_t off = 0;
    while (off < n) {
        ssize_t w = ::send(fd, p + off, n - off, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<size_t>(w);
    }
    return true;
}

inline bool send_frame(int fd, const std::string& msg) {
    uint32_t len = static_cast<uint32_t>(msg.size());
    uint32_t nlen = htonl(len);
    if (!write_n(fd, &nlen, sizeof(nlen))) return false;
    if (len == 0) return true;
    return write_n(fd, msg.data(), len);
}

inline bool recv_frame(int fd, std::string& out) {
    uint32_t nlen = 0;
    if (!read_n(fd, &nlen, sizeof(nlen))) return false;
    uint32_t len = ntohl(nlen);
    
    // Limit maximum frame size to 10MB to prevent DoS via memory exhaustion
    constexpr uint32_t kMaxFrameSize = 10 * 1024 * 1024;
    if (len > kMaxFrameSize) return false;

    out.clear();
    out.resize(len);
    if (len == 0) return true;
    return read_n(fd, out.data(), len);
}

}  // namespace ft
