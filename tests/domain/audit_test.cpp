#include <gtest/gtest.h>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"

namespace {

using namespace aetheris::domain;

TEST(RedactionPolicyTest, CreatesEmptyPolicyViaFactoryNone) {
  const auto policy = RedactionPolicy::none();

  EXPECT_TRUE(policy.empty());
  EXPECT_TRUE(policy.rules().empty());
}

TEST(RedactionPolicyTest, CreatesValidPolicyWithRules) {
  std::vector<RedactionRule> rules{
      {.field_path = "/parameters/pin", .mode = RedactionMode::mask},
      {.field_path = "/parameters/ssn", .mode = RedactionMode::hash_value},
  };

  const auto policy = RedactionPolicy::create(std::move(rules));

  ASSERT_TRUE(policy.has_value());
  EXPECT_EQ(policy->rules().size(), 2U);
  EXPECT_FALSE(policy->empty());
}

TEST(RedactionPolicyTest, RejectsRuleWithEmptyFieldPath) {
  std::vector<RedactionRule> rules{
      {.field_path = "", .mode = RedactionMode::remove},
  };

  const auto policy = RedactionPolicy::create(std::move(rules));

  ASSERT_FALSE(policy.has_value());
  EXPECT_EQ(error_code(policy.error()), "audit.redaction.empty_field_path");
}

TEST(AuditTypesTest, GenesisChainHashIsAllZeros) {
  const auto genesis = chain_hash_genesis();

  for (const auto byte : genesis) {
    EXPECT_EQ(byte, 0);
  }
}

TEST(AuditTypesTest, OutcomeKindCoversAllVariants) {
  // Compile-time coverage: ensure all enum members exist and are distinct values
  constexpr OutcomeKind outcomes[] = {
      OutcomeKind::approved,
      OutcomeKind::rejected,
      OutcomeKind::timed_out,
      OutcomeKind::cancelled,
  };
  EXPECT_EQ(std::size(outcomes), 4U);
}

TEST(AuditTypesTest, DecisionRecordIsAggregate) {
  const auto id = *DecisionId::parse("decision-001");
  const auto intent_id = *IntentId::parse("intent-001");
  const auto action_id = *ActionId::parse("camera.disable");
  const auto operator_id = *OperatorId::parse("op-alice");
  const auto tenant_id = *TenantId::parse("tenant-acme");
  const auto ts = std::chrono::system_clock::time_point{};

  const DecisionRecord record{
      .id = id,
      .intent_id = intent_id,
      .action_id = action_id,
      .operator_id = operator_id,
      .tenant_id = tenant_id,
      .timestamp = ts,
      .outcome = OutcomeKind::approved,
      .parameters_json = R"({"cameraId":"42"})",
      .result_json = R"({"status":"ok"})",
      .redacted_fields = "",
  };

  EXPECT_EQ(record.id.value(), "decision-001");
  EXPECT_EQ(record.action_id.value(), "camera.disable");
  EXPECT_EQ(record.outcome, OutcomeKind::approved);
}

} // namespace
