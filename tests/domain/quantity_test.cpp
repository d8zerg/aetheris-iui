#include <cstdint>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/quantity.hpp"

namespace {

using aetheris::domain::BlastRadiusLimit;
using aetheris::domain::error_code;
using aetheris::domain::TokenBudget;

static_assert(!std::is_same_v<BlastRadiusLimit, TokenBudget>);

TEST(QuantityTest, CreatesTypedNonNegativeQuantities) {
  constexpr auto blast_radius = BlastRadiusLimit::from(10);
  constexpr auto token_budget = TokenBudget::from(10);

  EXPECT_EQ(blast_radius.value(), 10U);
  EXPECT_EQ(token_budget.value(), 10U);
}

TEST(QuantityTest, RejectsNegativeSignedInput) {
  const auto value = BlastRadiusLimit::parse_signed(-1);

  ASSERT_FALSE(value.has_value());
  EXPECT_EQ(error_code(value.error()), "quantity.negative");
}

TEST(QuantityTest, AddsValuesWithSameUnit) {
  const auto value = BlastRadiusLimit::add(BlastRadiusLimit::from(4), BlastRadiusLimit::from(6));

  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->value(), 10U);
}

TEST(QuantityTest, RejectsOverflow) {
  const auto value = BlastRadiusLimit::add(
      BlastRadiusLimit::from(std::numeric_limits<std::uint64_t>::max()), BlastRadiusLimit::from(1));

  ASSERT_FALSE(value.has_value());
  EXPECT_EQ(error_code(value.error()), "quantity.overflow");
}

} // namespace
