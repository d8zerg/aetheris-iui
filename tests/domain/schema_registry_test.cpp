#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/schema_registry.hpp"

namespace {

using namespace aetheris::domain;

[[nodiscard]] ActionSchema make_schema(std::string version) {
  auto draft = ActionSchemaDraft{
      .action_id = *ActionId::parse("camera.disable"),
      .version = *SchemaVersion::parse(version),
      .parameters = *ParameterSignature::create(R"({"type":"object"})"),
      .reversibility = ReversibilityClass::reversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::bounded,
                                  .limit = BlastRadiusLimit::from(1)},
      .idempotency_key = *IdempotencyKey::create("action_id + cameraId"),
      .dry_run = DryRunRequirement::mandatory,
      .side_effect = SideEffectClass::writes_system,
      .required_scopes = *NonEmptyVector<std::string>::create({"camera.write"}),
      .confirmation = ConfirmationMode::typed,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create(
          {ActionExample{.intent = "Disable camera", .parameters_json = R"({"cameraId":"42"})"}}),
      .validation_rules = {}};

  auto schema = ActionSchema::create(std::move(draft));
  EXPECT_TRUE(schema.has_value());
  return std::move(*schema);
}

TEST(ActionSchemaRegistryTest, RegistersAndFindsSchemaByIdAndVersion) {
  ActionSchemaRegistry registry;
  auto schema = make_schema("1.0.0");
  const auto action_id = schema.action_id();
  const auto version = schema.version();

  const auto registered = registry.register_schema(std::move(schema));

  ASSERT_TRUE(registered.has_value());
  ASSERT_NE(registry.find(action_id, version), nullptr);
  EXPECT_EQ(registry.find(action_id, version)->version().value(), "1.0.0");
}

TEST(ActionSchemaRegistryTest, RejectsDuplicateVersion) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(make_schema("1.0.0")).has_value());

  const auto duplicate = registry.register_schema(make_schema("1.0.0"));

  ASSERT_FALSE(duplicate.has_value());
  EXPECT_EQ(error_code(duplicate.error()), "schema_registry.duplicate_version");
}

TEST(ActionSchemaRegistryTest, ReturnsLatestRegisteredSchemaForAction) {
  ActionSchemaRegistry registry;
  const auto action_id = *ActionId::parse("camera.disable");
  ASSERT_TRUE(registry.register_schema(make_schema("1.0.0")).has_value());
  ASSERT_TRUE(registry.register_schema(make_schema("1.1.0")).has_value());

  const auto* latest = registry.latest_for(action_id);

  ASSERT_NE(latest, nullptr);
  EXPECT_EQ(latest->version().value(), "1.1.0");
}

TEST(ActionSchemaRegistryTest, ListsRegisteredVersionsForReflection) {
  ActionSchemaRegistry registry;
  const auto action_id = *ActionId::parse("camera.disable");
  ASSERT_TRUE(registry.register_schema(make_schema("1.0.0")).has_value());
  ASSERT_TRUE(registry.register_schema(make_schema("1.1.0")).has_value());

  const auto versions = registry.versions_for(action_id);

  ASSERT_EQ(versions.size(), 2U);
  EXPECT_EQ(versions[0].value(), "1.0.0");
  EXPECT_EQ(versions[1].value(), "1.1.0");
}

} // namespace
