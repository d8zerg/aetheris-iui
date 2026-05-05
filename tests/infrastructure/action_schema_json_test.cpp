#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/infrastructure/action_schema_json.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

constexpr std::string_view kValidWriteJson = R"({
  "id": "camera.disable",
  "version": "1.0.0",
  "parameters": {"type": "object", "required": ["cameraId"]},
  "reversibility": "reversible",
  "blastRadius": {"class": "bounded", "maxEntities": 1},
  "idempotencyKey": "id + cameraId",
  "dryRun": "mandatory",
  "sideEffects": "writes_system",
  "requiredScopes": ["camera.write"],
  "confirmation": "typed",
  "rollback": "rollback_api",
  "examples": [{"intent": "Disable camera 42", "parameters": {"cameraId": "42"}}],
  "validationRules": [{"name": "camera_exists", "expression": "cameraId in inventory"}]
})";

constexpr std::string_view kValidReadOnlyJson = R"({
  "id": "camera.search",
  "version": "1.0.0",
  "parameters": {"type": "object", "required": ["query"]},
  "reversibility": "reversible",
  "blastRadius": {"class": "scoped", "maxEntities": 0},
  "idempotencyKey": "id + query",
  "dryRun": "not_applicable",
  "sideEffects": "read_only",
  "requiredScopes": ["camera.read"],
  "confirmation": "automatic",
  "rollback": "rollback_api",
  "examples": [{"intent": "Find lobby cameras", "parameters": {"query": "lobby"}}],
  "validationRules": []
})";

TEST(ActionSchemaJsonTest, ParsesValidWriteSchema) {
  const auto schema = parse_action_schema_json(kValidWriteJson);

  ASSERT_TRUE(schema.has_value());
  EXPECT_EQ(schema->action_id().value(), "camera.disable");
  EXPECT_EQ(schema->version().value(), "1.0.0");
  EXPECT_EQ(schema->reversibility(), ReversibilityClass::reversible);
  EXPECT_EQ(schema->confirmation(), ConfirmationMode::typed);
  EXPECT_EQ(schema->blast_radius().limit.value(), 1U);
  EXPECT_EQ(schema->validation_rules().size(), 1U);
}

TEST(ActionSchemaJsonTest, ParsesValidReadOnlySchema) {
  const auto schema = parse_action_schema_json(kValidReadOnlyJson);

  ASSERT_TRUE(schema.has_value());
  EXPECT_EQ(schema->confirmation(), ConfirmationMode::automatic);
  EXPECT_EQ(schema->side_effect(), SideEffectClass::read_only);
  EXPECT_EQ(schema->dry_run(), DryRunRequirement::not_applicable);
  EXPECT_TRUE(schema->validation_rules().empty());
}

TEST(ActionSchemaJsonTest, RejectsMalformedJson) {
  const auto schema = parse_action_schema_json("{not valid json");

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "json.action_schema.parse_error");
}

TEST(ActionSchemaJsonTest, RejectsMissingRequiredField) {
  constexpr std::string_view json = R"({"id": "camera.disable", "version": "1.0.0"})";
  const auto schema = parse_action_schema_json(json);

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "json.action_schema.missing_field");
}

TEST(ActionSchemaJsonTest, RejectsInvalidReversibilityEnum) {
  constexpr std::string_view json = R"({
    "id": "camera.disable", "version": "1.0.0",
    "parameters": {},
    "reversibility": "unknown_value",
    "blastRadius": {"class": "bounded", "maxEntities": 1},
    "idempotencyKey": "x",
    "dryRun": "mandatory",
    "sideEffects": "writes_system",
    "requiredScopes": ["camera.write"],
    "confirmation": "typed",
    "rollback": "rollback_api",
    "examples": [{"intent": "X", "parameters": {}}],
    "validationRules": []
  })";
  const auto schema = parse_action_schema_json(json);

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "json.action_schema.reversibility.invalid");
}

TEST(ActionSchemaJsonTest, PropagatesToDomainInvariantViolation) {
  // irreversible + rollback_api violates domain invariant
  constexpr std::string_view json = R"({
    "id": "camera.delete",
    "version": "1.0.0",
    "parameters": {"type": "object"},
    "reversibility": "irreversible",
    "blastRadius": {"class": "bounded", "maxEntities": 1},
    "idempotencyKey": "id + cameraId",
    "dryRun": "mandatory",
    "sideEffects": "writes_system",
    "requiredScopes": ["camera.write"],
    "confirmation": "typed",
    "rollback": "rollback_api",
    "examples": [{"intent": "Delete camera", "parameters": {"cameraId": "42"}}],
    "validationRules": []
  })";
  const auto schema = parse_action_schema_json(json);

  ASSERT_FALSE(schema.has_value());
  EXPECT_EQ(error_code(schema.error()), "action_schema.rollback.irreversible_has_rollback");
}

TEST(ActionSchemaJsonTest, SerializesAndRoundTrips) {
  const auto original = parse_action_schema_json(kValidWriteJson);
  ASSERT_TRUE(original.has_value());

  const auto serialized = serialize_action_schema_json(*original);
  const auto reparsed = parse_action_schema_json(serialized);

  ASSERT_TRUE(reparsed.has_value());
  EXPECT_EQ(reparsed->action_id().value(), original->action_id().value());
  EXPECT_EQ(reparsed->version().value(), original->version().value());
  EXPECT_EQ(reparsed->reversibility(), original->reversibility());
  EXPECT_EQ(reparsed->blast_radius().classification, original->blast_radius().classification);
  EXPECT_EQ(reparsed->blast_radius().limit.value(), original->blast_radius().limit.value());
  EXPECT_EQ(reparsed->confirmation(), original->confirmation());
  EXPECT_EQ(reparsed->rollback(), original->rollback());
  EXPECT_EQ(reparsed->dry_run(), original->dry_run());
  EXPECT_EQ(reparsed->side_effect(), original->side_effect());
  EXPECT_EQ(reparsed->examples().values().size(), original->examples().values().size());
  EXPECT_EQ(reparsed->validation_rules().size(), original->validation_rules().size());
}

TEST(ActionSchemaJsonTest, SerializesReadOnlySchemaAndRoundTrips) {
  const auto original = parse_action_schema_json(kValidReadOnlyJson);
  ASSERT_TRUE(original.has_value());

  const auto serialized = serialize_action_schema_json(*original);
  const auto reparsed = parse_action_schema_json(serialized);

  ASSERT_TRUE(reparsed.has_value());
  EXPECT_EQ(reparsed->action_id().value(), original->action_id().value());
  EXPECT_EQ(reparsed->confirmation(), ConfirmationMode::automatic);
  EXPECT_EQ(reparsed->dry_run(), DryRunRequirement::not_applicable);
}

} // namespace
