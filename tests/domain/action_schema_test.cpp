#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"

namespace {

using namespace aetheris::domain;

[[nodiscard]] ActionSchemaDraft valid_write_draft(std::string action = "camera.disable",
                                                  std::string version = "1.0.0") {
  return ActionSchemaDraft{
      .action_id = *ActionId::parse(action),
      .version = *SchemaVersion::parse(version),
      .parameters = *ParameterSignature::create(R"({"type":"object","required":["cameraId"]})"),
      .reversibility = ReversibilityClass::reversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::bounded,
                                  .limit = BlastRadiusLimit::from(1)},
      .idempotency_key = *IdempotencyKey::create("action_id + cameraId"),
      .dry_run = DryRunRequirement::mandatory,
      .side_effect = SideEffectClass::writes_system,
      .required_scopes = *NonEmptyVector<std::string>::create({"camera.write"}),
      .confirmation = ConfirmationMode::typed,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create({ActionExample{
          .intent = "Disable camera 42", .parameters_json = R"({"cameraId":"42"})"}}),
      .validation_rules = {ValidationRule{.name = "camera_exists",
                                          .expression = "cameraId exists in live inventory"}}};
}

[[nodiscard]] ActionSchemaDraft valid_read_only_draft() {
  return ActionSchemaDraft{
      .action_id = *ActionId::parse("camera.search"),
      .version = *SchemaVersion::parse("1.0.0"),
      .parameters = *ParameterSignature::create(R"({"type":"object","required":["query"]})"),
      .reversibility = ReversibilityClass::reversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::scoped,
                                  .limit = BlastRadiusLimit::from(0)},
      .idempotency_key = *IdempotencyKey::create("action_id + query"),
      .dry_run = DryRunRequirement::not_applicable,
      .side_effect = SideEffectClass::read_only,
      .required_scopes = *NonEmptyVector<std::string>::create({"camera.read"}),
      .confirmation = ConfirmationMode::automatic,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create({ActionExample{
          .intent = "Find lobby cameras", .parameters_json = R"({"query":"lobby"})"}}),
      .validation_rules = {}};
}

TEST(ParameterSignatureTest, RejectsEmptySchemaPayload) {
  const auto signature = ParameterSignature::create("");

  ASSERT_FALSE(signature.has_value());
  EXPECT_EQ(error_code(signature.error()), "action_schema.parameters.empty");
}

TEST(IdempotencyKeyTest, RejectsEmptyExpression) {
  const auto key = IdempotencyKey::create("");

  ASSERT_FALSE(key.has_value());
  EXPECT_EQ(error_code(key.error()), "action_schema.idempotency.empty");
}

TEST(ActionSchemaTest, CreatesValidWriteActionSchema) {
  const auto schema = ActionSchema::create(valid_write_draft());

  ASSERT_TRUE(schema.has_value());
  EXPECT_EQ(schema->action_id().value(), "camera.disable");
  EXPECT_EQ(schema->confirmation(), ConfirmationMode::typed);
  EXPECT_EQ(schema->blast_radius().limit.value(), 1U);
}

TEST(ActionSchemaTest, CreatesValidAutomaticReadOnlySchema) {
  const auto schema = ActionSchema::create(valid_read_only_draft());

  ASSERT_TRUE(schema.has_value());
  EXPECT_EQ(schema->confirmation(), ConfirmationMode::automatic);
  EXPECT_EQ(schema->side_effect(), SideEffectClass::read_only);
}

TEST(ActionSchemaTest, RejectsReversibleActionWithoutRollbackApi) {
  auto draft = valid_write_draft();
  draft.rollback = RollbackStrategy::manual;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.rollback.reversible_requires_api");
}

TEST(ActionSchemaTest, RejectsCompensableActionWithoutCompensatingAction) {
  auto draft = valid_write_draft();
  draft.reversibility = ReversibilityClass::compensable;
  draft.rollback = RollbackStrategy::rollback_api;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.rollback.compensable_requires_action");
}

TEST(ActionSchemaTest, RejectsIrreversibleActionWithAutomatedRollback) {
  auto draft = valid_write_draft();
  draft.reversibility = ReversibilityClass::irreversible;
  draft.rollback = RollbackStrategy::rollback_api;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.rollback.irreversible_has_rollback");
}

TEST(ActionSchemaTest, RejectsAutomaticWriteAction) {
  auto draft = valid_write_draft();
  draft.confirmation = ConfirmationMode::automatic;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.confirmation.automatic_not_low_risk");
}

TEST(ActionSchemaTest, RejectsBroadActionWithoutStrictConfirmation) {
  auto draft = valid_write_draft();
  draft.blast_radius =
      BlastRadius{.classification = BlastRadiusClass::broad, .limit = BlastRadiusLimit::from(500)};
  draft.confirmation = ConfirmationMode::typed;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.confirmation.broad_requires_strict_mode");
}

TEST(ActionSchemaTest, RejectsReadOnlyActionWithDryRun) {
  auto draft = valid_read_only_draft();
  draft.dry_run = DryRunRequirement::optional;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.dry_run.read_only_not_applicable");
}

TEST(ActionSchemaTest, RejectsIrreversibleWriteWithoutMandatoryDryRun) {
  auto draft = valid_write_draft();
  draft.reversibility = ReversibilityClass::irreversible;
  draft.rollback = RollbackStrategy::manual;
  draft.dry_run = DryRunRequirement::optional;

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.dry_run.irreversible_requires_dry_run");
}

TEST(ActionSchemaTest, RejectsExampleWithoutIntent) {
  auto draft = valid_write_draft();
  draft.examples = *NonEmptyVector<ActionExample>::create(
      {ActionExample{.intent = "", .parameters_json = R"({"cameraId":"42"})"}});

  const auto schema = ActionSchema::create(std::move(draft));

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.example.intent_empty");
}

} // namespace
