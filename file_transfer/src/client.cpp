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
#include "terminal_ui.hpp"
#include "transfer.hpp"

namespace fs = std::filesystem;

static constexpr size_t kChunkSize = 64 * 1024;

static std::mutex g_print_mutex;

static void show_progress(const std::string& prefix, uint64_t done, uint64_t total,
                          const ui::TimePoint& start_time) {
  if (total == 0) return;
  std::lock_guard<std::mutex> lk(g_print_mutex);
  ui::render_progress(prefix, done, total, start_time);
}

static std::string basename_only(const std::string& path) {
  fs::path p(path);
  return p.filename().string();
}

static bool do_list(int fd) {
  if (!ft::send_frame(fd, "LIST")) return false;
  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;

  if (!ftproto::starts_with(resp, "OK")) {
    ui::print_error(resp);
    return true;
  }

  // Parse the OK\n<listing> response
  ui::print_file_list_header();
  std::istringstream stream(resp);
  std::string line;
  std::getline(stream, line); // skip "OK" line
  bool has_files = false;
  while (std::getline(stream, line)) {
    if (line.empty()) continue;
    has_files = true;
    // Format: filename\tsize
    auto tab = line.find('\t');
    if (tab != std::string::npos) {
      std::string name = line.substr(0, tab);
      uint64_t sz = std::stoull(line.substr(tab + 1));
      ui::print_file_entry(name, ui::format_size(sz));
    } else {
      ui::print_file_entry(line, "");
    }
  }
  if (!has_files) {
    std::cout << "  " << ui::color::dim << "  (empty)" << ui::color::reset << "\n";
  }
  ui::print_separator();
  std::cout << std::endl;
  return true;
}

static bool do_upload(int fd, const std::string& local_path) {
  fs::path p(local_path);
  if (!fs::exists(p) || !fs::is_regular_file(p)) {
    ui::print_error("Local file not found: " + local_path);
    return true;
  }

  uint64_t size = fs::file_size(p);
  std::string filename = basename_only(local_path);

  ui::print_info("Uploading " + filename + " (" + ui::format_size(size) + ") ...");

  std::string hash_expected = ft::sha256_file(p, kChunkSize);

  if (!ft::send_frame(fd, ftproto::format_upload_cmd(filename, size, hash_expected))) return false;

  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;
  if (!ftproto::starts_with(resp, "OK")) {
    ui::print_error(resp);
    return true;
  }

  if (ftproto::starts_with(resp, "OK AlreadyHave")) {
    ui::print_success("File already exists on server (SHA-256 verified)");
    return true;
  }

  uint64_t offset = 0;
  if (!ftproto::parse_ok_offset(resp, offset)) offset = 0;

  if (offset > 0) {
    ui::print_info("Resuming from offset " + ui::format_size(offset));
  }

  std::ifstream in(p, std::ios::binary);
  if (!in) {
    ui::print_error("Cannot open local file: " + local_path);
    return true;
  }

  if (offset > 0 && offset < size) {
    in.seekg(static_cast<std::streamoff>(offset));
  } else {
    offset = 0;
  }

  std::string label = std::string(ui::sym::upload) + " UPLOAD";
  auto start_time = ui::SteadyClock::now();
  if (!fttransfer::send_from_file(fd,
                                 in,
                                 size,
                                 offset,
                                 kChunkSize,
                                 nullptr,
                                 [&](uint64_t done, uint64_t total) { show_progress(label, done, total, start_time); })) {
    return false;
  }

  ui::finish_progress(label);

  if (!ft::recv_frame(fd, resp)) return false;
  if (ftproto::starts_with(resp, "OK") && resp.find("MISMATCH") != std::string::npos) {
    ui::print_error("Integrity check failed (SHA-256 mismatch)");
  } else {
    ui::print_success("Server: " + filename + " saved successfully.");
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

  if (!ft::send_frame(fd, ftproto::format_download_cmd(filename, offset))) return false;

  std::string resp;
  if (!ft::recv_frame(fd, resp)) return false;
  if (!ftproto::starts_with(resp, "OK")) {
    ui::print_error(resp);
    return true;
  }

  ftproto::DownloadOk ok;
  if (!ftproto::parse_download_ok(resp, ok)) {
    ui::print_error(resp);
    return true;
  }
  if (ok.size == 0) {
    ui::print_error("Server reported size=0");
    return true;
  }

  ui::print_info("Downloading " + filename + " (" + ui::format_size(ok.size) + ") ...");

  if (ok.offset != offset) offset = ok.offset;

  if (offset > 0) {
    ui::print_info("Resuming from offset " + ui::format_size(offset));
  }

  std::ofstream out;
  uint64_t write_offset = offset;
  if (!fttransfer::open_out_with_offset(out_path, write_offset, ok.size, out)) {
    ui::print_error("Cannot open output file: " + out_path.string());
    return true;
  }
  offset = write_offset;

  std::string label = std::string(ui::sym::download) + " DOWNLOAD";
  auto start_time = ui::SteadyClock::now();
  if (!fttransfer::recv_to_file(fd,
                               out,
                               ok.size,
                               offset,
                               kChunkSize,
                               nullptr,
                               [&](uint64_t done, uint64_t total) { show_progress(label, done, total, start_time); })) {
    return false;
  }

  ui::finish_progress(label);

  out.close();
  std::string hash_local = ft::sha256_file(out_path, kChunkSize);
  if (!ok.hash.empty() && hash_local == ok.hash) {
    ui::print_success("Saved -> " + out_path.string() + " (SHA-256 MATCH)");
  } else if (!ok.hash.empty()) {
    ui::print_error("Saved -> " + out_path.string() + " (SHA-256 MISMATCH)");
  } else {
    ui::print_success("Saved -> " + out_path.string());
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
    ui::print_error(std::string("connect() failed: ") + std::strerror(errno));
    return 1;
  }

  ui::print_banner();
  ui::print_success("Connected to " + server_ip + ":" + std::to_string(port));
  std::cout << std::endl;

  std::string line;
  while (true) {
    ui::print_prompt();
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string op;
    iss >> op;

    // Support lowercase commands too
    for (auto& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (op == "LIST") {
      if (!do_list(fd)) break;
      continue;
    }

    if (op == "UPLOAD") {
      std::string local_path;
      iss >> local_path;
      if (local_path.empty()) {
        ui::print_info("Usage: UPLOAD <local_path>");
        continue;
      }
      if (!do_upload(fd, local_path)) break;
      std::cout << std::endl;
      continue;
    }

    if (op == "DOWNLOAD") {
      std::string filename;
      iss >> filename;
      if (filename.empty()) {
        ui::print_info("Usage: DOWNLOAD <filename>");
        continue;
      }
      if (!do_download(fd, filename)) break;
      std::cout << std::endl;
      continue;
    }

    if (op == "QUIT" || op == "EXIT") {
      ft::send_frame(fd, "QUIT");
      std::string resp;
      ft::recv_frame(fd, resp);
      ui::print_info("Disconnected. Goodbye!");
      break;
    }

    if (op == "HELP") {
      ui::print_help();
      continue;
    }

    ui::print_help();
  }

  ::close(fd);
  return 0;
}
