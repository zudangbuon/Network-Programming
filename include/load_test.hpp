#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "net.hpp"

namespace ftload {
namespace fs = std::filesystem;

inline void run_load_test(const std::string& server_ip,
                          int port,
                          int load_n,
                          int loops,
                          int max_kb,
                          const std::function<int(const std::string&, int)>& connect_to,
                          const std::function<bool(int, const std::string&)>& do_upload,
                          const std::function<bool(int, const std::string&)>& do_download,
                          std::mutex& print_mutex) {
  auto worker = [&](int id) {
    std::mt19937 rng(static_cast<uint32_t>(std::random_device{}()));
    std::uniform_int_distribution<int> dist_kb(1, std::max(1, max_kb));

    int fd = connect_to(server_ip, port);
    if (fd < 0) {
      std::lock_guard<std::mutex> lk(print_mutex);
      std::cerr << "worker " << id << ": connect failed\n";
      return;
    }

    for (int i = 0; i < loops; i++) {
      int kb = dist_kb(rng);
      size_t bytes = static_cast<size_t>(kb) * 1024;
      std::string name = "load_" + std::to_string(id) + "_" + std::to_string(i) + ".bin";
      fs::path tmp = fs::temp_directory_path() / name;

      {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        std::vector<char> buf(4096);
        for (size_t k = 0; k < buf.size(); k++) buf[k] = static_cast<char>(rng() & 0xff);
        size_t written = 0;
        while (written < bytes) {
          size_t n = std::min(buf.size(), bytes - written);
          out.write(buf.data(), static_cast<std::streamsize>(n));
          written += n;
        }
      }

      {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "worker " << id << ": UPLOAD " << tmp.string() << std::endl;
      }

      if (!do_upload(fd, tmp.string())) {
        ::close(fd);
        return;
      }

      {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "worker " << id << ": DOWNLOAD " << name << std::endl;
      }

      if (!do_download(fd, name)) {
        ::close(fd);
        return;
      }
    }

    ft::send_frame(fd, "QUIT");
    std::string resp;
    ft::recv_frame(fd, resp);
    ::close(fd);
  };

  std::vector<std::thread> ts;
  ts.reserve(static_cast<size_t>(load_n));
  for (int i = 0; i < load_n; i++) {
    ts.emplace_back([=]() { worker(i); });
  }
  for (auto& t : ts) t.join();
}

}  // namespace ftload
