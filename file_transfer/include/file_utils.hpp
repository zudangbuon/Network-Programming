#pragma once

#include <filesystem>
#include <sstream>
#include <string>

namespace ft {
namespace fs = std::filesystem;

inline std::string sanitize_filename(const std::string& name) {
  std::string out;
  out.reserve(name.size());
  for (char c : name) {
    if (c == '/' || c == '\\') continue;
    out.push_back(c);
  }
  if (out.empty()) out = "file";
  return out;
}

inline std::string list_files(const fs::path& dir) {
  std::ostringstream oss;
  if (!fs::exists(dir)) return "";
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file()) continue;
    auto sz = entry.file_size();
    oss << entry.path().filename().string() << "\t" << sz << "\n";
  }
  return oss.str();
}

}  // namespace ft
