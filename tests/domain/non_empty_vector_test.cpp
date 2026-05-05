#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/non_empty_vector.hpp"

namespace {

using aetheris::domain::error_code;
using aetheris::domain::NonEmptyVector;

TEST(NonEmptyVectorTest, AcceptsNonEmptyCollection) {
  const auto values = NonEmptyVector<int>::create({1, 2, 3});

  ASSERT_TRUE(values.has_value());
  EXPECT_EQ(values->size(), 3U);
  EXPECT_EQ(values->front(), 1);
}

TEST(NonEmptyVectorTest, RejectsEmptyCollection) {
  const auto values = NonEmptyVector<int>::create(std::vector<int>{});

  ASSERT_FALSE(values.has_value());
  EXPECT_EQ(error_code(values.error()), "non_empty_vector.empty");
}

} // namespace
