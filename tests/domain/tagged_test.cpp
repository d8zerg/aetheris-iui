#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "aetheris/domain/tagged.hpp"

namespace {

struct OperatorQuotaTag final {};
struct TenantQuotaTag final {};

using OperatorQuota = aetheris::domain::Tagged<int, OperatorQuotaTag>;
using TenantQuota = aetheris::domain::Tagged<int, TenantQuotaTag>;

static_assert(!std::is_convertible_v<OperatorQuota, TenantQuota>);

TEST(TaggedTest, KeepsPrimitiveValuesInSeparateTypes) {
  const OperatorQuota operator_quota{7};
  const OperatorQuota same_operator_quota{7};
  const OperatorQuota larger_operator_quota{9};

  EXPECT_EQ(operator_quota.value(), 7);
  EXPECT_EQ(operator_quota, same_operator_quota);
  EXPECT_LT(operator_quota, larger_operator_quota);
}

TEST(TaggedTest, SupportsMoveOutForOwningValues) {
  using TaggedString = aetheris::domain::Tagged<std::string, OperatorQuotaTag>;

  TaggedString value{"intent.created"};

  EXPECT_EQ(std::move(value).value(), "intent.created");
}

} // namespace
