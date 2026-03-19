#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "net.hpp"
#include "rate_limiter.hpp"

namespace fttransfer {
namespace fs = std::filesystem;

using ProgressFn = std::function<void(uint64_t done, uint64_t total)>;

inline bool recv_to_file(int fd,
                                                  std::ofstream& out,
                                                  uint64_t total_size,
                                                  uint64_t offset,
                                                  size_t chunk_size,
                                                  ft::RateLimiter* limiter,
                                                  const ProgressFn& progress) {
    std::vector<char> buf(chunk_size);
    uint64_t received = offset;
    while (received < total_size) {
        size_t to_read = static_cast<size_t>(std::min<uint64_t>(chunk_size, total_size - received));
        if (!ft::read_n(fd, buf.data(), to_read)) return false;
        if (limiter) limiter->consume(to_read);
        out.write(buf.data(), static_cast<std::streamsize>(to_read));
        if (!out) return false;
        received += to_read;
        if (progress) progress(received, total_size);
    }
    return true;
}

inline bool send_from_file(int fd,
                                                      std::ifstream& in,
                                                      uint64_t total_size,
                                                      uint64_t offset,
                                                      size_t chunk_size,
                                                      ft::RateLimiter* limiter,
                                                      const ProgressFn& progress) {
    std::vector<char> buf(chunk_size);
    uint64_t sent = offset;
    while (sent < total_size) {
        size_t to_read = static_cast<size_t>(std::min<uint64_t>(chunk_size, total_size - sent));
        in.read(buf.data(), static_cast<std::streamsize>(to_read));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        if (limiter) limiter->consume(static_cast<uint64_t>(got));
        if (!ft::write_n(fd, buf.data(), static_cast<size_t>(got))) return false;
        sent += static_cast<uint64_t>(got);
        if (progress) progress(sent, total_size);
    }
    return true;
}

inline bool open_out_with_offset(const fs::path& path, uint64_t& offset_inout, uint64_t total_size, std::ofstream& out) {
    if (offset_inout > 0 && offset_inout < total_size) {
        out.open(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!out) out.open(path, std::ios::binary | std::ios::app);
        if (!out) return false;
        out.seekp(static_cast<std::streamoff>(offset_inout));
        return true;
    }

    offset_inout = 0;
    out.open(path, std::ios::binary | std::ios::trunc);
    return static_cast<bool>(out);
}

}  // namespace fttransfer
