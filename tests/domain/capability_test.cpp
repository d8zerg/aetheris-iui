#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/capability.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "support/schema_test_helpers.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::tests::helpers;

[[nodiscard]] OperatorCapabilitySet make_full_operator() {
  return OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-alice"),
      .permitted_action_ids = {"camera.disable", "sensor.read"},
      .granted_scopes = {"camera.write", "sensor.read"},
      .max_blast_radius = BlastRadiusClass::bounded,
  };
}

// ---- check_permission ----

TEST(CapabilityCheckPermissionTest, PermitsActionInAllowList) {
  const auto schema =
      *ActionSchema::create(make_bounded_write_draft("camera.disable", "camera.write"));
  const auto result = check_permission(make_full_operator(), schema);

  EXPECT_TRUE(result.has_value());
}

TEST(CapabilityCheckPermissionTest, RejectsActionNotInAllowList) {
  const auto schema =
      *ActionSchema::create(make_scoped_read_only_draft("unknown.action", "sensor.read"));
  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-bob"),
      .permitted_action_ids = {"sensor.read"},
      .granted_scopes = {"sensor.read"},
      .max_blast_radius = BlastRadiusClass::scoped,
  };

  const auto result = check_permission(op, schema);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "capability.action_not_permitted");
}

TEST(CapabilityCheckPermissionTest, RejectsWhenRequiredScopeIsMissing) {
  const auto schema =
      *ActionSchema::create(make_bounded_write_draft("camera.disable", "camera.write"));
  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-charlie"),
      .permitted_action_ids = {"camera.disable"},
      .granted_scopes = {"sensor.read"}, // missing camera.write
      .max_blast_radius = BlastRadiusClass::bounded,
  };

  const auto result = check_permission(op, schema);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "capability.scope_missing");
  const auto& details = error_details(result.error());
  const auto it = std::find_if(details.begin(), details.end(),
                               [](const ErrorDetail& d) { return d.key == "missing_scope"; });
  ASSERT_NE(it, details.end());
  EXPECT_EQ(it->value, "camera.write");
}

TEST(CapabilityCheckPermissionTest, RejectsWhenBlastRadiusExceedsOperatorCeiling) {
  const auto schema =
      *ActionSchema::create(make_broad_irreversible_draft("facility.lockdown", "facility.admin"));
  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-dave"),
      .permitted_action_ids = {"facility.lockdown"},
      .granted_scopes = {"facility.admin"},
      .max_blast_radius = BlastRadiusClass::bounded, // broad > bounded -> denied
  };

  const auto result = check_permission(op, schema);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "capability.blast_radius_exceeds_operator_ceiling");
}

TEST(CapabilityCheckPermissionTest, PermitsExactlyAtOperatorBlastCeiling) {
  const auto schema =
      *ActionSchema::create(make_bounded_write_draft("camera.disable", "camera.write"));
  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-eve"),
      .permitted_action_ids = {"camera.disable"},
      .granted_scopes = {"camera.write"},
      .max_blast_radius = BlastRadiusClass::bounded, // bounded == bounded -> ok
  };

  const auto result = check_permission(op, schema);

  EXPECT_TRUE(result.has_value());
}

TEST(CapabilityCheckPermissionTest, RejectsEmptyPermittedActionsSet) {
  const auto schema =
      *ActionSchema::create(make_scoped_read_only_draft("sensor.read", "sensor.read"));
  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-none"),
      .permitted_action_ids = {}, // empty = no actions
      .granted_scopes = {"sensor.read"},
      .max_blast_radius = BlastRadiusClass::scoped,
  };

  const auto result = check_permission(op, schema);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "capability.action_not_permitted");
}

// ---- filter_permitted_schemas ----

TEST(FilterPermittedSchemasTest, ReturnsOnlyPermittedSubset) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry
                  .register_schema(*ActionSchema::create(
                      make_bounded_write_draft("camera.disable", "camera.write")))
                  .has_value());
  ASSERT_TRUE(registry
                  .register_schema(*ActionSchema::create(
                      make_scoped_read_only_draft("sensor.read", "sensor.read")))
                  .has_value());
  ASSERT_TRUE(registry
                  .register_schema(*ActionSchema::create(
                      make_broad_irreversible_draft("facility.lockdown", "facility.admin")))
                  .has_value());

  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-alice"),
      .permitted_action_ids = {"camera.disable", "sensor.read"},
      .granted_scopes = {"camera.write", "sensor.read"},
      .max_blast_radius = BlastRadiusClass::bounded,
  };

  const auto permitted = filter_permitted_schemas(op, registry);

  ASSERT_EQ(permitted.size(), 2U);
  std::vector<std::string> ids;
  for (const auto* s : permitted) {
    ids.push_back(s->action_id().value());
  }
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), "camera.disable") != ids.end());
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), "sensor.read") != ids.end());
}

TEST(FilterPermittedSchemasTest, ExcludesActionsNotInRegistry) {
  ActionSchemaRegistry registry;

  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-ghost"),
      .permitted_action_ids = {"nonexistent.action"},
      .granted_scopes = {"some.scope"},
      .max_blast_radius = BlastRadiusClass::broad,
  };

  const auto permitted = filter_permitted_schemas(op, registry);

  EXPECT_TRUE(permitted.empty());
}

TEST(FilterPermittedSchemasTest, ExcludesActionsWhereScopesMissing) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry
                  .register_schema(*ActionSchema::create(
                      make_bounded_write_draft("camera.disable", "camera.write")))
                  .has_value());

  const auto op = OperatorCapabilitySet{
      .operator_id = *OperatorId::parse("op-noscope"),
      .permitted_action_ids = {"camera.disable"},
      .granted_scopes = {}, // no scopes at all
      .max_blast_radius = BlastRadiusClass::bounded,
  };

  const auto permitted = filter_permitted_schemas(op, registry);

  EXPECT_TRUE(permitted.empty());
}

} // namespace
