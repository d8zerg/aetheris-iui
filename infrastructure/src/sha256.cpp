#include "aetheris/infrastructure/sha256.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Pure C++ SHA-256 (FIPS 180-4).
// No external dependencies; constant-time on inputs of fixed length.

namespace aetheris::infrastructure {
namespace {

constexpr std::array<std::uint32_t, 64> kK{{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
}};

constexpr std::array<std::uint32_t, 8> kH0{{
    0x6a09e667,
    0xbb67ae85,
    0x3c6ef372,
    0xa54ff53a,
    0x510e527f,
    0x9b05688c,
    0x1f83d9ab,
    0x5be0cd19,
}};

[[nodiscard]] constexpr std::uint32_t rotr32(std::uint32_t x, unsigned n) noexcept {
  return std::rotr(x, static_cast<int>(n));
}

[[nodiscard]] constexpr std::uint32_t ch(std::uint32_t e, std::uint32_t f,
                                         std::uint32_t g) noexcept {
  return (e & f) ^ (~e & g);
}

[[nodiscard]] constexpr std::uint32_t maj(std::uint32_t a, std::uint32_t b,
                                          std::uint32_t c) noexcept {
  return (a & b) ^ (a & c) ^ (b & c);
}

[[nodiscard]] constexpr std::uint32_t sig0(std::uint32_t x) noexcept {
  return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

[[nodiscard]] constexpr std::uint32_t sig1(std::uint32_t x) noexcept {
  return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

[[nodiscard]] constexpr std::uint32_t gamma0(std::uint32_t x) noexcept {
  return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3U);
}

[[nodiscard]] constexpr std::uint32_t gamma1(std::uint32_t x) noexcept {
  return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10U);
}

void process_block(std::array<std::uint32_t, 8>& state,
                   const std::array<std::uint8_t, 64>& block) noexcept {
  std::array<std::uint32_t, 64> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24U) |
           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16U) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8U) |
           (static_cast<std::uint32_t>(block[i * 4 + 3]));
  }
  for (std::size_t i = 16; i < 64; ++i) {
    w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
  }

  auto [a, b, c, d, e, f, g, h] = state;

  for (std::size_t i = 0; i < 64; ++i) {
    const auto t1 = h + sig1(e) + ch(e, f, g) + kK[i] + w[i];
    const auto t2 = sig0(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

} // namespace

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> data) noexcept {
  auto state = kH0;
  const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8U;

  std::array<std::uint8_t, 64> block{};
  std::size_t block_pos = 0;

  auto flush_block = [&]() noexcept { process_block(state, block); };

  for (const auto byte : data) {
    block[block_pos++] = byte;
    if (block_pos == 64) {
      flush_block();
      block_pos = 0;
      block = {};
    }
  }

  // Append 0x80 padding byte
  block[block_pos++] = 0x80;
  if (block_pos == 64) {
    flush_block();
    block_pos = 0;
    block = {};
  }

  // If not enough room for the 8-byte length field, flush now
  if (block_pos > 56) {
    flush_block();
    block_pos = 0;
    block = {};
  }

  // Write 8-byte big-endian bit-length into the last 8 bytes of the final block
  for (std::size_t i = 0; i < 8; ++i) {
    block[56 + i] = static_cast<std::uint8_t>((bit_length >> (56U - i * 8U)) & 0xFFU);
  }
  flush_block();

  std::array<std::uint8_t, 32> digest{};
  for (std::size_t i = 0; i < 8; ++i) {
    digest[i * 4 + 0] = static_cast<std::uint8_t>(state[i] >> 24U);
    digest[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 16U);
    digest[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 8U);
    digest[i * 4 + 3] = static_cast<std::uint8_t>(state[i]);
  }
  return digest;
}

std::array<std::uint8_t, 32> sha256(std::string_view text) noexcept {
  return sha256(std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(text.data()),
                                              text.size()});
}

std::string hex_encode(std::span<const std::uint8_t, 32> digest) noexcept {
  constexpr std::string_view kHex{"0123456789abcdef"};
  std::string out;
  out.reserve(64);
  for (const auto byte : digest) {
    out.push_back(kHex[(byte >> 4U) & 0xFU]);
    out.push_back(kHex[byte & 0xFU]);
  }
  return out;
}

} // namespace aetheris::infrastructure
