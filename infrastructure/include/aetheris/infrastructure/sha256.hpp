#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace aetheris::infrastructure {

/** Computes SHA-256 over an arbitrary byte span. */
[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> data) noexcept;

/** Convenience overload for string data. */
[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::string_view text) noexcept;

/** Returns the lower-case hex encoding of a 32-byte digest. */
[[nodiscard]] std::string hex_encode(std::span<const std::uint8_t, 32> digest) noexcept;

} // namespace aetheris::infrastructure
