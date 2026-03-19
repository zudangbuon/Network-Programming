#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

namespace ft {

class RateLimiter {
  public:
    explicit RateLimiter(uint64_t bytes_per_sec) : bytes_per_sec_(bytes_per_sec) {
        last_ = std::chrono::steady_clock::now();
        available_ = static_cast<double>(bytes_per_sec_);
    }

    void consume(uint64_t bytes) {
        if (bytes_per_sec_ == 0) return;
        refill();
        available_ -= static_cast<double>(bytes);
        if (available_ >= 0) return;

        double need = -available_;
        double secs = need / static_cast<double>(bytes_per_sec_);
        if (secs > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(secs));
        }
        refill();
    }

  private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> dt = now - last_;
        if (dt.count() <= 0) return;
        available_ += dt.count() * static_cast<double>(bytes_per_sec_);
        double cap = static_cast<double>(bytes_per_sec_);
        if (available_ > cap) available_ = cap;
        last_ = now;
    }

    uint64_t bytes_per_sec_;
    std::chrono::steady_clock::time_point last_;
    double available_ = 0;
};

}  // namespace ft
