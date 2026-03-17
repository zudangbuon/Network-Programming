#pragma once

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace ft {

inline uint32_t md5_left_rotate(uint32_t x, uint32_t c) {
  return (x << c) | (x >> (32 - c));
}

class Md5 {
 public:
  Md5() { reset(); }

  void reset() {
    a0_ = 0x67452301;
    b0_ = 0xefcdab89;
    c0_ = 0x98badcfe;
    d0_ = 0x10325476;
    total_len_ = 0;
    buffer_len_ = 0;
  }

  void update(const uint8_t* data, size_t len) {
    total_len_ += static_cast<uint64_t>(len);
    size_t i = 0;
    if (buffer_len_ > 0) {
      size_t need = 64 - buffer_len_;
      size_t take = (need < len) ? need : len;
      std::memcpy(buffer_ + buffer_len_, data, take);
      buffer_len_ += take;
      i += take;
      if (buffer_len_ == 64) {
        transform(buffer_);
        buffer_len_ = 0;
      }
    }

    for (; i + 64 <= len; i += 64) {
      transform(data + i);
    }

    if (i < len) {
      buffer_len_ = len - i;
      std::memcpy(buffer_, data + i, buffer_len_);
    }
  }

  std::string final_hex() {
    uint64_t bit_len = total_len_ * 8;

    uint8_t pad[64] = {0x80};
    size_t pad_len = (buffer_len_ < 56) ? (56 - buffer_len_) : (120 - buffer_len_);
    update(pad, pad_len);

    uint8_t len_le[8];
    for (int i = 0; i < 8; i++) len_le[i] = static_cast<uint8_t>((bit_len >> (8 * i)) & 0xff);
    update(len_le, 8);

    uint8_t digest[16];
    put32le(a0_, digest + 0);
    put32le(b0_, digest + 4);
    put32le(c0_, digest + 8);
    put32le(d0_, digest + 12);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; i++) oss << std::setw(2) << static_cast<int>(digest[i]);
    return oss.str();
  }

 private:
  static void put32le(uint32_t x, uint8_t* out) {
    out[0] = static_cast<uint8_t>((x >> 0) & 0xff);
    out[1] = static_cast<uint8_t>((x >> 8) & 0xff);
    out[2] = static_cast<uint8_t>((x >> 16) & 0xff);
    out[3] = static_cast<uint8_t>((x >> 24) & 0xff);
  }

  void transform(const uint8_t block[64]) {
    static const uint32_t s[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    static const uint32_t K[] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
        0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
        0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
        0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
        0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
        0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
        0xeb86d391};

    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
      M[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 0) |
             (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
             (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
             (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    uint32_t A = a0_, B = b0_, C = c0_, D = d0_;
    for (uint32_t i = 0; i < 64; i++) {
      uint32_t F = 0;
      uint32_t g = 0;
      if (i < 16) {
        F = (B & C) | ((~B) & D);
        g = i;
      } else if (i < 32) {
        F = (D & B) | ((~D) & C);
        g = (5 * i + 1) % 16;
      } else if (i < 48) {
        F = B ^ C ^ D;
        g = (3 * i + 5) % 16;
      } else {
        F = C ^ (B | (~D));
        g = (7 * i) % 16;
      }
      uint32_t tmp = D;
      D = C;
      C = B;
      B = B + md5_left_rotate(A + F + K[i] + M[g], s[i]);
      A = tmp;
    }

    a0_ += A;
    b0_ += B;
    c0_ += C;
    d0_ += D;
  }

  uint32_t a0_ = 0;
  uint32_t b0_ = 0;
  uint32_t c0_ = 0;
  uint32_t d0_ = 0;
  uint64_t total_len_ = 0;
  uint8_t buffer_[64] = {0};
  size_t buffer_len_ = 0;
};

}  // namespace ft
