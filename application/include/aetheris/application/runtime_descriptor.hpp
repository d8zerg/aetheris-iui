#pragma once

#include <cstdint>
#include <string_view>

namespace aetheris::application {

/**
 * Runtime metadata exposed by outer interface layers.
 */
struct RuntimeDescriptor final {
  std::string_view product_name;
  std::uint32_t abi_version;
};

inline constexpr std::uint32_t kStableAbiVersion = 1;

/**
 * Returns static runtime metadata for the current core build.
 */
[[nodiscard]] constexpr RuntimeDescriptor runtime_descriptor() noexcept {
  return RuntimeDescriptor{.product_name = "Aetheris-IUI", .abi_version = kStableAbiVersion};
}

} // namespace aetheris::application
