#pragma once

#include <compare>
#include <type_traits>
#include <utility>

namespace aetheris::domain {

/**
 * Lightweight strong wrapper for primitive values that must not be mixed by accident.
 */
template <typename TValue, typename TTag> class Tagged final {
 public:
  using value_type = TValue;
  using tag_type = TTag;

  constexpr Tagged() = delete;

  /**
   * Wraps a trusted value in a distinct type.
   */
  constexpr explicit Tagged(TValue value) noexcept(std::is_nothrow_move_constructible_v<TValue>)
      : value_{std::move(value)} {}

  /**
   * Returns the wrapped value.
   */
  [[nodiscard]] constexpr const TValue& value() const& noexcept {
    return value_;
  }

  /**
   * Moves the wrapped value out of the tag.
   */
  [[nodiscard]] constexpr TValue value() && noexcept(std::is_nothrow_move_constructible_v<TValue>) {
    return std::move(value_);
  }

  [[nodiscard]] friend constexpr bool operator==(const Tagged& lhs,
                                                 const Tagged& rhs) noexcept = default;
  [[nodiscard]] friend constexpr auto operator<=>(const Tagged& lhs,
                                                  const Tagged& rhs) noexcept = default;

 private:
  TValue value_;
};

} // namespace aetheris::domain
