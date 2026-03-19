#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "sha256.hpp"

namespace ft {

inline std::string sha256_file(const std::filesystem::path& path, size_t chunk_size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";

    Sha256 sha;
    std::vector<char> buf(chunk_size);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        sha.update(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<size_t>(got));
    }
    return sha.final_hex();
}

}  // namespace ft
