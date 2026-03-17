#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "md5.hpp"

namespace ft {

inline std::string md5_file(const std::filesystem::path& path, size_t chunk_size) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";

  Md5 md5;
  std::vector<char> buf(chunk_size);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    std::streamsize got = in.gcount();
    if (got <= 0) break;
    md5.update(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<size_t>(got));
  }
  return md5.final_hex();
}

}  // namespace ft
