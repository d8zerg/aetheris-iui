#pragma once

#include <expected>
#include <utility>

#include "aetheris/domain/error.hpp"

namespace aetheris::domain {

/**
 * Standard fallible result used by the domain and application layers.
 */
template <typename TValue> using Result = std::expected<TValue, PlatformError>;

/**
 * Builds an unexpected PlatformError value while preserving std::expected ergonomics.
 */
[[nodiscard]] inline std::unexpected<PlatformError> fail(PlatformError error) {
  return std::unexpected<PlatformError>{std::move(error)};
}

} // namespace aetheris::domain
