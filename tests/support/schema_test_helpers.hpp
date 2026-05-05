#pragma once

#include <string>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/quantity.hpp"

namespace aetheris::tests::helpers {

using namespace domain;

[[nodiscard]] inline ActionSchemaDraft
make_scoped_read_only_draft(std::string action_id = "sensor.read",
                            std::string scope = "sensor.read") {
  return ActionSchemaDraft{.action_id = *ActionId::parse(action_id),
                           .version = *SchemaVersion::parse("1.0.0"),
                           .parameters = *ParameterSignature::create(R"({"type":"object"})"),
                           .reversibility = ReversibilityClass::reversible,
                           .blast_radius = BlastRadius{.classification = BlastRadiusClass::scoped,
                                                       .limit = BlastRadiusLimit::from(0)},
                           .idempotency_key = *IdempotencyKey::create("action_id + sensorId"),
                           .dry_run = DryRunRequirement::not_applicable,
                           .side_effect = SideEffectClass::read_only,
                           .required_scopes = *NonEmptyVector<std::string>::create({scope}),
                           .confirmation = ConfirmationMode::automatic,
                           .rollback = RollbackStrategy::rollback_api,
                           .examples = *NonEmptyVector<ActionExample>::create({ActionExample{
                               .intent = "Read sensor", .parameters_json = R"({"id":"1"})"}}),
                           .validation_rules = {}};
}

[[nodiscard]] inline ActionSchemaDraft
make_bounded_write_draft(std::string action_id = "camera.disable",
                         std::string scope = "camera.write", std::uint64_t limit = 10) {
  return ActionSchemaDraft{
      .action_id = *ActionId::parse(action_id),
      .version = *SchemaVersion::parse("1.0.0"),
      .parameters = *ParameterSignature::create(R"({"type":"object","required":["cameraId"]})"),
      .reversibility = ReversibilityClass::reversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::bounded,
                                  .limit = BlastRadiusLimit::from(limit)},
      .idempotency_key = *IdempotencyKey::create("action_id + cameraId"),
      .dry_run = DryRunRequirement::optional,
      .side_effect = SideEffectClass::writes_system,
      .required_scopes = *NonEmptyVector<std::string>::create({scope}),
      .confirmation = ConfirmationMode::single,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create(
          {ActionExample{.intent = "Disable camera 1", .parameters_json = R"({"cameraId":"1"})"}}),
      .validation_rules = {}};
}

[[nodiscard]] inline ActionSchemaDraft
make_broad_irreversible_draft(std::string action_id = "facility.lockdown",
                              std::string scope = "facility.admin", std::uint64_t limit = 500) {
  return ActionSchemaDraft{
      .action_id = *ActionId::parse(action_id),
      .version = *SchemaVersion::parse("1.0.0"),
      .parameters = *ParameterSignature::create(R"({"type":"object","required":["facilityId"]})"),
      .reversibility = ReversibilityClass::irreversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::broad,
                                  .limit = BlastRadiusLimit::from(limit)},
      .idempotency_key = *IdempotencyKey::create("action_id + facilityId"),
      .dry_run = DryRunRequirement::mandatory,
      .side_effect = SideEffectClass::external_calls,
      .required_scopes = *NonEmptyVector<std::string>::create({scope}),
      .confirmation = ConfirmationMode::multi_party,
      .rollback = RollbackStrategy::manual,
      .examples = *NonEmptyVector<ActionExample>::create({ActionExample{
          .intent = "Lock down facility A", .parameters_json = R"({"facilityId":"A"})"}}),
      .validation_rules = {}};
}

} // namespace aetheris::tests::helpers
