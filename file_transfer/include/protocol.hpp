#pragma once

#include <cstdint>
#include <sstream>
#include <string>

namespace ftproto {

inline bool starts_with(const std::string& s, const std::string& pfx) {
  return s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0;
}

struct UploadRequest {
  std::string filename;
  uint64_t size = 0;
  std::string hash;
};

struct DownloadRequest {
  std::string filename;
  uint64_t offset = 0;
};

struct DownloadOk {
  uint64_t size = 0;
  std::string hash;
  uint64_t offset = 0;
};

inline std::string format_list_cmd() { return "LIST"; }

inline std::string format_quit_cmd() { return "QUIT"; }

inline std::string format_upload_cmd(const std::string& filename, uint64_t size, const std::string& hash) {
  std::ostringstream oss;
  oss << "UPLOAD " << filename << " " << size;
  if (!hash.empty()) oss << " " << hash;
  return oss.str();
}

inline std::string format_download_cmd(const std::string& filename, uint64_t offset) {
  std::ostringstream oss;
  oss << "DOWNLOAD " << filename << " " << offset;
  return oss.str();
}

inline bool parse_ok_offset(const std::string& resp, uint64_t& offset_out) {
  std::istringstream iss(resp);
  std::string ok, kw;
  uint64_t off = 0;
  iss >> ok >> kw >> off;
  if (ok != "OK" || kw != "OFFSET") return false;
  offset_out = off;
  return true;
}

inline bool parse_download_ok(const std::string& resp, DownloadOk& out) {
  std::istringstream iss(resp);
  std::string ok, kw;
  DownloadOk tmp;
  iss >> ok >> tmp.size >> tmp.hash >> kw >> tmp.offset;
  if (ok != "OK" || kw != "OFFSET") return false;
  out = tmp;
  return true;
}

}  // namespace ftproto
