#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <string>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Non-negative integer quantity whose unit is encoded in the type.
 */
template <typename TUnit> class Quantity final {
 public:
  using value_type = std::uint64_t;
  using unit_type = TUnit;

  Quantity() = delete;

  /**
   * Creates a quantity from an already non-negative value.
   */
  [[nodiscard]] static constexpr Quantity from(value_type value) noexcept {
    return Quantity{value};
  }

  /**
   * Parses a signed value and rejects negative inputs with a typed InputError.
   */
  [[nodiscard]] static Result<Quantity> parse_signed(std::int64_t value) {
    if (value < 0) {
      return fail(make_input_error("quantity.negative", "Quantity must not be negative.",
                                   {ErrorDetail{"actual", std::to_string(value)}}));
    }

    return Quantity{static_cast<value_type>(value)};
  }

  /**
   * Adds two quantities of the same unit and rejects overflow.
   */
  [[nodiscard]] static Result<Quantity> add(Quantity lhs, Quantity rhs) {
    if (std::numeric_limits<value_type>::max() - lhs.value_ < rhs.value_) {
      return fail(make_input_error("quantity.overflow", "Quantity addition overflowed."));
    }

    return Quantity{lhs.value_ + rhs.value_};
  }

  [[nodiscard]] constexpr value_type value() const noexcept {
    return value_;
  }

  [[nodiscard]] friend constexpr bool operator==(const Quantity& lhs,
                                                 const Quantity& rhs) noexcept = default;
  [[nodiscard]] friend constexpr auto operator<=>(const Quantity& lhs,
                                                  const Quantity& rhs) noexcept = default;

 private:
  constexpr explicit Quantity(value_type value) noexcept : value_{value} {}

  value_type value_;
};

struct BlastRadiusLimitUnit final {};
struct TokenBudgetUnit final {};
struct RateLimitPerMinuteUnit final {};
struct ByteBudgetUnit final {};

using BlastRadiusLimit = Quantity<BlastRadiusLimitUnit>;
using TokenBudget = Quantity<TokenBudgetUnit>;
using RateLimitPerMinute = Quantity<RateLimitPerMinuteUnit>;
using ByteBudget = Quantity<ByteBudgetUnit>;

} // namespace aetheris::domain
