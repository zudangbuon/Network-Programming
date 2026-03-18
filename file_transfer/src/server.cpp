#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "protocol.hpp"
#include "file_utils.hpp"
#include "hash.hpp"
#include "net.hpp"
#include "rate_limiter.hpp"
#include "transfer.hpp"

namespace fs = std::filesystem;

static constexpr int kDefaultPort = 8784;
static constexpr size_t kChunkSize = 64 * 1024;
static constexpr int kMaxThreads = 128;

static std::atomic<bool> g_running{true};
static int g_server_fd = -1;

static void signal_handler(int /*sig*/) {
  g_running.store(false);
  // Close the listening socket to unblock accept()
  if (g_server_fd >= 0) {
    ::shutdown(g_server_fd, SHUT_RDWR);
    ::close(g_server_fd);
    g_server_fd = -1;
  }
}

static std::mutex g_log_mutex;

static void log_line(const std::string& s) {
  std::lock_guard<std::mutex> lk(g_log_mutex);
  std::cout << s << std::endl;
}

static void show_progress(const std::string& prefix, uint64_t done, uint64_t total, int& last_pct) {
  if (total == 0) return;
  int pct = static_cast<int>((done * 100) / total);
  if (pct >= last_pct + 10 || pct == 100) {
    last_pct = pct;
    std::ostringstream oss;
    oss << prefix << " " << pct << "% (" << done << "/" << total << " bytes)";
    log_line(oss.str());
  }
}

static void handle_client(int client_fd, sockaddr_in client_addr, fs::path uploads_dir, uint64_t rate_limit_bps) {
  char ipbuf[INET_ADDRSTRLEN] = {0};
  inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
  int cport = ntohs(client_addr.sin_port);
  {
    std::ostringstream oss;
    oss << "Client connected: " << ipbuf << ":" << cport;
    log_line(oss.str());
  }

  ft::RateLimiter limiter(rate_limit_bps);
  for (;;) {
    std::string cmd;
    if (!ft::recv_frame(client_fd, cmd)) {
      break;
    }

    std::istringstream iss(cmd);
    std::string op;
    iss >> op;

    if (op == "LIST") {
      std::string listing = ft::list_files(uploads_dir);
      std::ostringstream resp;
      resp << "OK\n" << listing;
      if (!ft::send_frame(client_fd, resp.str())) break;
      continue;
    }

    if (op == "UPLOAD") {
      std::string filename;
      uint64_t size = 0;
      std::string hash_expected;
      iss >> filename >> size >> hash_expected;
      if (filename.empty() || size == 0) {
        if (!ft::send_frame(client_fd, "ERR Invalid UPLOAD. Use: UPLOAD <filename> <bytes>")) break;
        continue;
      }

      filename = ft::sanitize_filename(filename);
      fs::create_directories(uploads_dir);
      fs::path out_path = uploads_dir / filename;

      uint64_t offset = 0;
      if (fs::exists(out_path) && fs::is_regular_file(out_path)) {
        uint64_t existing = fs::file_size(out_path);
        if (existing < size) {
          offset = existing;
        } else if (existing == size) {
          std::string hash_now = ft::sha256_file(out_path, kChunkSize);
          if (!hash_expected.empty() && hash_now == hash_expected) {
            std::ostringstream done;
            done << "OK AlreadyHave SHA256 " << hash_now << " MATCH";
            ft::send_frame(client_fd, done.str());
            continue;
          }
          offset = 0;
        } else {
          offset = 0;
        }
      }

      std::ofstream out;
      if (offset > 0) {
        out.open(out_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!out) out.open(out_path, std::ios::binary | std::ios::app);
        out.seekp(static_cast<std::streamoff>(offset));
      } else {
        out.open(out_path, std::ios::binary | std::ios::trunc);
      }

      if (!out) {
        if (!ft::send_frame(client_fd, "ERR Cannot open file for writing")) break;
        continue;
      }

      {
        std::ostringstream ok;
        ok << "OK OFFSET " << offset;
        if (!ft::send_frame(client_fd, ok.str())) break;
      }

      int last_pct = -10;
      if (!fttransfer::recv_to_file(client_fd,
                                   out,
                                   size,
                                   offset,
                                   kChunkSize,
                                   &limiter,
                                   [&](uint64_t done, uint64_t total) { show_progress("UPLOAD", done, total, last_pct); })) {
        out.close();
        return;
      }

      out.close();
      {
        std::string hash_now = ft::sha256_file(out_path, kChunkSize);
        if (!hash_expected.empty()) {
          std::ostringstream fin;
          fin << "OK Saved SHA256 " << hash_now << " " << ((hash_now == hash_expected) ? "MATCH" : "MISMATCH");
          ft::send_frame(client_fd, fin.str());
        } else {
          std::ostringstream fin;
          fin << "OK Saved SHA256 " << hash_now;
          ft::send_frame(client_fd, fin.str());
        }
      }

    upload_done:
      continue;
    }

    if (op == "DOWNLOAD") {
      std::string filename;
      uint64_t offset = 0;
      iss >> filename;
      if (!(iss >> offset)) offset = 0;
      if (filename.empty()) {
        if (!ft::send_frame(client_fd, "ERR Invalid DOWNLOAD. Use: DOWNLOAD <filename>")) break;
        continue;
      }

      filename = ft::sanitize_filename(filename);
      fs::path in_path = uploads_dir / filename;
      if (!fs::exists(in_path) || !fs::is_regular_file(in_path)) {
        if (!ft::send_frame(client_fd, "ERR Not found")) break;
        continue;
      }

      uint64_t size = fs::file_size(in_path);
      if (offset > size) offset = 0;
      std::string hash_now = ft::sha256_file(in_path, kChunkSize);
      std::ostringstream resp;
      resp << "OK " << size << " " << hash_now << " OFFSET " << offset;
      if (!ft::send_frame(client_fd, resp.str())) break;

      std::ifstream in(in_path, std::ios::binary);
      if (!in) {
        ft::send_frame(client_fd, "ERR Cannot open file");
        continue;
      }
      in.seekg(static_cast<std::streamoff>(offset));

      int last_pct = -10;
      if (!fttransfer::send_from_file(client_fd,
                                     in,
                                     size,
                                     offset,
                                     kChunkSize,
                                     &limiter,
                                     [&](uint64_t done, uint64_t total) { show_progress("DOWNLOAD", done, total, last_pct); })) {
        return;
      }
      continue;
    }

    if (op == "QUIT" || op == "EXIT") {
      ft::send_frame(client_fd, "OK Bye");
      break;
    }

    if (!ft::send_frame(client_fd, "ERR Unknown command")) break;
  }

  ::close(client_fd);
  {
    std::ostringstream oss;
    oss << "Client disconnected: " << ipbuf << ":" << cport;
    log_line(oss.str());
  }
}

int main(int argc, char** argv) {
  int port = kDefaultPort;
  uint64_t rate_limit_bps = 0;
  if (argc >= 2) port = std::stoi(argv[1]);
  if (argc >= 3) {
    rate_limit_bps = static_cast<uint64_t>(std::stoull(argv[2]));
  } else {
    // Interactive rate limit configuration
    std::cout << "\n  Enable rate limiting per client? [y/N]: " << std::flush;
    std::string answer;
    std::getline(std::cin, answer);
    if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
      std::cout << "  Enter bandwidth limit (e.g. 1m = 1 MB/s, 500k = 500 KB/s): " << std::flush;
      std::string val;
      std::getline(std::cin, val);
      if (!val.empty()) {
        // Parse human-readable suffixes: 1m, 2M, 500k, 500K, or raw number
        char suffix = val.back();
        if (suffix == 'm' || suffix == 'M') {
          double num = std::stod(val.substr(0, val.size() - 1));
          rate_limit_bps = static_cast<uint64_t>(num * 1024 * 1024);
        } else if (suffix == 'k' || suffix == 'K') {
          double num = std::stod(val.substr(0, val.size() - 1));
          rate_limit_bps = static_cast<uint64_t>(num * 1024);
        } else {
          rate_limit_bps = static_cast<uint64_t>(std::stoull(val));
        }
      }
    }
    std::cout << std::endl;
  }

  fs::path uploads_dir = fs::current_path() / "uploads";
  fs::create_directories(uploads_dir);

  g_server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (g_server_fd < 0) {
    std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
    return 1;
  }

  int yes = 1;
  setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (::bind(g_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
    ::close(g_server_fd);
    return 1;
  }

  if (::listen(g_server_fd, 64) < 0) {
    std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
    ::close(g_server_fd);
    return 1;
  }

  // Register signal handlers for graceful shutdown
  struct sigaction sa{};
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  {
    std::ostringstream oss;
    oss << "File server listening on 0.0.0.0:" << port << " (uploads dir: " << uploads_dir.string() << ")";
    if (rate_limit_bps > 0) oss << " (rate limit: " << rate_limit_bps << " B/s per client)";
    log_line(oss.str());
  }

  // Managed thread list instead of detach()
  std::vector<std::thread> workers;

  while (g_running.load()) {
    sockaddr_in caddr{};
    socklen_t clen = sizeof(caddr);
    int cfd = ::accept(g_server_fd, reinterpret_cast<sockaddr*>(&caddr), &clen);
    if (cfd < 0) {
      if (errno == EINTR || !g_running.load()) break;
      std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
      continue;
    }

    // Clean up finished threads to avoid unbounded growth
    for (auto it = workers.begin(); it != workers.end(); ) {
      if (!it->joinable()) {
        it = workers.erase(it);
      } else {
        ++it;
      }
    }

    // Reject new connections if thread limit is reached
    if (static_cast<int>(workers.size()) >= kMaxThreads) {
      log_line("Max clients reached, rejecting connection");
      ::close(cfd);
      continue;
    }

    workers.emplace_back([cfd, caddr, uploads_dir, rate_limit_bps]() mutable {
      handle_client(cfd, caddr, uploads_dir, rate_limit_bps);
    });
  }

  log_line("Shutting down, waiting for active clients to finish...");

  // Wait for all client threads to finish
  for (auto& t : workers) {
    if (t.joinable()) t.join();
  }

  if (g_server_fd >= 0) {
    ::close(g_server_fd);
    g_server_fd = -1;
  }

  log_line("Server stopped.");
  return 0;
}
