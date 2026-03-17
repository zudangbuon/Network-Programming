#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "load_test.hpp"
#include "protocol.hpp"
#include "hash.hpp"
#include "net.hpp"
#include "transfer.hpp"

namespace fs = std::filesystem;

static constexpr size_t kChunkSize = 64 * 1024;

static std::mutex g_print_mutex;

static void show_progress(const std::string& prefix, uint64_t done, uint64_t total, int& last_pct) {
  if (total == 0) return;
  int pct = static_cast<int>((done * 100) / total);
  if (pct >= last_pct + 10 || pct == 100) {
    last_pct = pct;
    std::lock_guard<std::mutex> lk(g_print_mutex);
    std::cout << prefix << " " << pct << "% (" << done << "/" << total << " bytes)" << std::endl;
  }
}

static std::string basename_only(const std::string& path) {
  fs::path p(path);
  return p.filename().string();
}

static bool do_list(int fd) {
  if (!ft::send_frame(fd, "LIST")) return false;
  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;
  std::cout << resp;
  if (!resp.empty() && resp.back() != '\n') std::cout << "\n";
  return true;
}

static bool do_upload(int fd, const std::string& local_path) {
  fs::path p(local_path);
  if (!fs::exists(p) || !fs::is_regular_file(p)) {
    std::cerr << "Local file not found: " << local_path << "\n";
    return true;
  }

  uint64_t size = fs::file_size(p);
  std::string filename = basename_only(local_path);

  std::string md5_expected = ft::md5_file(p, kChunkSize);

  {
    if (!ft::send_frame(fd, ftproto::format_upload_cmd(filename, size, md5_expected))) return false;
  }

  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;
  if (!ftproto::starts_with(resp, "OK")) {
    std::cout << resp << std::endl;
    return true;
  }

  if (ftproto::starts_with(resp, "OK AlreadyHave")) {
    std::cout << resp << std::endl;
    return true;
  }

  uint64_t offset = 0;
  if (!ftproto::parse_ok_offset(resp, offset)) offset = 0;

  std::ifstream in(p, std::ios::binary);
  if (!in) {
    std::cerr << "Cannot open local file: " << local_path << "\n";
    return true;
  }

  if (offset > 0 && offset < size) {
    in.seekg(static_cast<std::streamoff>(offset));
  } else {
    offset = 0;
  }

  int last_pct = -10;
  if (!fttransfer::send_from_file(fd,
                                 in,
                                 size,
                                 offset,
                                 kChunkSize,
                                 nullptr,
                                 [&](uint64_t done, uint64_t total) { show_progress("UPLOAD", done, total, last_pct); })) {
    return false;
  }

  if (!ft::recv_frame(fd, resp)) return false;
  std::cout << resp << std::endl;
  if (ftproto::starts_with(resp, "OK") && resp.find("MISMATCH") != std::string::npos) {
    std::cerr << "Integrity check failed (MD5 mismatch)" << std::endl;
  }
  return true;
}

static bool do_download(int fd, const std::string& filename) {
  fs::path downloads_dir = fs::current_path() / "downloads";
  fs::create_directories(downloads_dir);
  fs::path out_path = downloads_dir / basename_only(filename);

  uint64_t offset = 0;
  if (fs::exists(out_path) && fs::is_regular_file(out_path)) {
    offset = fs::file_size(out_path);
  }

  {
    if (!ft::send_frame(fd, ftproto::format_download_cmd(filename, offset))) return false;
  }

  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;
  if (!ftproto::starts_with(resp, "OK")) {
    std::cout << resp << std::endl;
    return true;
  }

  ftproto::DownloadOk ok;
  if (!ftproto::parse_download_ok(resp, ok)) {
    std::cout << resp << std::endl;
    return true;
  }
  if (ok.size == 0) {
    std::cerr << "Server reported size=0\n";
    return true;
  }

  if (ok.offset != offset) offset = ok.offset;

  std::ofstream out;
  uint64_t write_offset = offset;
  if (!fttransfer::open_out_with_offset(out_path, write_offset, ok.size, out)) {
    std::cerr << "Cannot open output file: " << out_path.string() << "\n";
    return true;
  }
  offset = write_offset;

  int last_pct = -10;
  if (!fttransfer::recv_to_file(fd,
                               out,
                               ok.size,
                               offset,
                               kChunkSize,
                               nullptr,
                               [&](uint64_t done, uint64_t total) { show_progress("DOWNLOAD", done, total, last_pct); })) {
    return false;
  }

  out.close();
  std::string md5_local = ft::md5_file(out_path, kChunkSize);
  if (!ok.md5.empty() && md5_local == ok.md5) {
    std::cout << "OK Saved -> " << out_path.string() << " (MD5 MATCH)" << std::endl;
  } else if (!ok.md5.empty()) {
    std::cout << "OK Saved -> " << out_path.string() << " (MD5 MISMATCH)" << std::endl;
  } else {
    std::cout << "OK Saved -> " << out_path.string() << std::endl;
  }
  return true;
}

static int connect_to(const std::string& server_ip, int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr) != 1) {
    ::close(fd);
    return -1;
  }

  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <server_ip> <port> [--load N --loops L --max-kb K]\n";
    return 1;
  }

  std::string server_ip = argv[1];
  int port = std::stoi(argv[2]);
//load để test nhiều client
  int load_n = 0;
  int loops = 1;
  int max_kb = 256;
  for (int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--load" && i + 1 < argc) {
      load_n = std::stoi(argv[++i]);
    } else if (a == "--loops" && i + 1 < argc) {
      loops = std::stoi(argv[++i]);
    } else if (a == "--max-kb" && i + 1 < argc) {
      max_kb = std::stoi(argv[++i]);
    }
  }

  if (load_n > 0) {
    ftload::run_load_test(server_ip,
                          port,
                          load_n,
                          loops,
                          max_kb,
                          connect_to,
                          do_upload,
                          do_download,
                          g_print_mutex);
    return 0;
  }

  int fd = connect_to(server_ip, port);
  if (fd < 0) {
    std::cerr << "connect() failed: " << std::strerror(errno) << "\n";
    return 1;
  }

  std::cout << "Connected to file server." << std::endl;

  std::string line;
  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string op;
    iss >> op;

    if (op == "LIST") {
      if (!do_list(fd)) break;
      continue;
    }

    if (op == "UPLOAD") {
      std::string local_path;
      iss >> local_path;
      if (local_path.empty()) {
        std::cout << "Usage: UPLOAD <local_path>" << std::endl;
        continue;
      }
      if (!do_upload(fd, local_path)) break;
      continue;
    }

    if (op == "DOWNLOAD") {
      std::string filename;
      iss >> filename;
      if (filename.empty()) {
        std::cout << "Usage: DOWNLOAD <filename>" << std::endl;
        continue;
      }
      if (!do_download(fd, filename)) break;
      continue;
    }

    if (op == "QUIT" || op == "EXIT") {
      ft::send_frame(fd, "QUIT");
      std::string resp;
      ft::recv_frame(fd, resp);
      std::cout << resp << std::endl;
      break;
    }

    std::cout << "Commands supported: LIST, UPLOAD <local_path>, DOWNLOAD <filename>, QUIT" << std::endl;
  }

  ::close(fd);
  return 0;
}
