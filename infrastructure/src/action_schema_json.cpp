#include "aetheris/infrastructure/action_schema_json.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/quantity.hpp"

namespace aetheris::infrastructure {

using nlohmann::json;
using namespace domain;

namespace {

[[nodiscard]] Result<ReversibilityClass> parse_reversibility(const std::string& value) {
  if (value == "reversible")
    return ReversibilityClass::reversible;
  if (value == "compensable")
    return ReversibilityClass::compensable;
  if (value == "irreversible")
    return ReversibilityClass::irreversible;
  return fail(make_input_error("json.action_schema.reversibility.invalid",
                               "Unknown reversibility class.", {ErrorDetail{"value", value}}));
}

[[nodiscard]] Result<BlastRadiusClass> parse_blast_radius_class(const std::string& value) {
  if (value == "scoped")
    return BlastRadiusClass::scoped;
  if (value == "bounded")
    return BlastRadiusClass::bounded;
  if (value == "broad")
    return BlastRadiusClass::broad;
  return fail(make_input_error("json.action_schema.blast_radius_class.invalid",
                               "Unknown blast radius class.", {ErrorDetail{"value", value}}));
}

[[nodiscard]] Result<DryRunRequirement> parse_dry_run(const std::string& value) {
  if (value == "mandatory")
    return DryRunRequirement::mandatory;
  if (value == "optional")
    return DryRunRequirement::optional;
  if (value == "not_applicable")
    return DryRunRequirement::not_applicable;
  return fail(make_input_error("json.action_schema.dry_run.invalid", "Unknown dry-run requirement.",
                               {ErrorDetail{"value", value}}));
}

[[nodiscard]] Result<SideEffectClass> parse_side_effect(const std::string& value) {
  if (value == "read_only")
    return SideEffectClass::read_only;
  if (value == "writes_system")
    return SideEffectClass::writes_system;
  if (value == "external_calls")
    return SideEffectClass::external_calls;
  if (value == "notifications")
    return SideEffectClass::notifications;
  return fail(make_input_error("json.action_schema.side_effects.invalid",
                               "Unknown side effect class.", {ErrorDetail{"value", value}}));
}

[[nodiscard]] Result<ConfirmationMode> parse_confirmation(const std::string& value) {
  if (value == "automatic")
    return ConfirmationMode::automatic;
  if (value == "single")
    return ConfirmationMode::single;
  if (value == "typed")
    return ConfirmationMode::typed;
  if (value == "multi_party")
    return ConfirmationMode::multi_party;
  if (value == "cooling_off")
    return ConfirmationMode::cooling_off;
  return fail(make_input_error("json.action_schema.confirmation.invalid",
                               "Unknown confirmation mode.", {ErrorDetail{"value", value}}));
}

[[nodiscard]] Result<RollbackStrategy> parse_rollback(const std::string& value) {
  if (value == "none")
    return RollbackStrategy::none;
  if (value == "rollback_api")
    return RollbackStrategy::rollback_api;
  if (value == "compensating_action")
    return RollbackStrategy::compensating_action;
  if (value == "manual")
    return RollbackStrategy::manual;
  return fail(make_input_error("json.action_schema.rollback.invalid", "Unknown rollback strategy.",
                               {ErrorDetail{"value", value}}));
}

template <typename TResult, typename TParser>
[[nodiscard]] Result<TResult> require_string_field(const json& doc, const char* field,
                                                   TParser parse_fn) {
  if (!doc.contains(field) || !doc[field].is_string()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 std::string{"Required string field is missing: "} + field));
  }
  return parse_fn(doc[field].get<std::string>());
}

[[nodiscard]] Result<std::string> require_string(const json& doc, const char* field) {
  if (!doc.contains(field) || !doc[field].is_string()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 std::string{"Required string field is missing: "} + field));
  }
  return doc[field].get<std::string>();
}

[[nodiscard]] std::string reversibility_to_string(ReversibilityClass value) {
  switch (value) {
  case ReversibilityClass::reversible:
    return "reversible";
  case ReversibilityClass::compensable:
    return "compensable";
  case ReversibilityClass::irreversible:
    return "irreversible";
  }
  return "reversible";
}

[[nodiscard]] std::string blast_radius_class_to_string(BlastRadiusClass value) {
  switch (value) {
  case BlastRadiusClass::scoped:
    return "scoped";
  case BlastRadiusClass::bounded:
    return "bounded";
  case BlastRadiusClass::broad:
    return "broad";
  }
  return "scoped";
}

[[nodiscard]] std::string dry_run_to_string(DryRunRequirement value) {
  switch (value) {
  case DryRunRequirement::mandatory:
    return "mandatory";
  case DryRunRequirement::optional:
    return "optional";
  case DryRunRequirement::not_applicable:
    return "not_applicable";
  }
  return "not_applicable";
}

[[nodiscard]] std::string side_effect_to_string(SideEffectClass value) {
  switch (value) {
  case SideEffectClass::read_only:
    return "read_only";
  case SideEffectClass::writes_system:
    return "writes_system";
  case SideEffectClass::external_calls:
    return "external_calls";
  case SideEffectClass::notifications:
    return "notifications";
  }
  return "read_only";
}

[[nodiscard]] std::string confirmation_to_string(ConfirmationMode value) {
  switch (value) {
  case ConfirmationMode::automatic:
    return "automatic";
  case ConfirmationMode::single:
    return "single";
  case ConfirmationMode::typed:
    return "typed";
  case ConfirmationMode::multi_party:
    return "multi_party";
  case ConfirmationMode::cooling_off:
    return "cooling_off";
  }
  return "single";
}

[[nodiscard]] std::string rollback_to_string(RollbackStrategy value) {
  switch (value) {
  case RollbackStrategy::none:
    return "none";
  case RollbackStrategy::rollback_api:
    return "rollback_api";
  case RollbackStrategy::compensating_action:
    return "compensating_action";
  case RollbackStrategy::manual:
    return "manual";
  }
  return "none";
}

} // namespace

Result<ActionSchema> parse_action_schema_json(std::string_view json_text) {
  json doc;
  try {
    doc = json::parse(json_text);
  } catch (const json::parse_error& err) {
    return fail(make_input_error("json.action_schema.parse_error",
                                 std::string{"JSON parse error: "} + err.what()));
  }

  if (!doc.is_object()) {
    return fail(
        make_input_error("json.action_schema.not_object", "Action schema JSON must be an object."));
  }

  auto action_id_str = require_string(doc, "id");
  if (!action_id_str.has_value())
    return fail(action_id_str.error());

  auto action_id = ActionId::parse(*action_id_str);
  if (!action_id.has_value())
    return fail(action_id.error());

  auto version_str = require_string(doc, "version");
  if (!version_str.has_value())
    return fail(version_str.error());

  auto version = SchemaVersion::parse(*version_str);
  if (!version.has_value())
    return fail(version.error());

  if (!doc.contains("parameters") || !doc["parameters"].is_object()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 "Required object field is missing: parameters"));
  }
  auto parameters = ParameterSignature::create(doc["parameters"].dump());
  if (!parameters.has_value())
    return fail(parameters.error());

  auto reversibility =
      require_string_field<ReversibilityClass>(doc, "reversibility", parse_reversibility);
  if (!reversibility.has_value())
    return fail(reversibility.error());

  if (!doc.contains("blastRadius") || !doc["blastRadius"].is_object()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 "Required object field is missing: blastRadius"));
  }
  const auto& blast_obj = doc["blastRadius"];

  auto blast_class =
      require_string_field<BlastRadiusClass>(blast_obj, "class", parse_blast_radius_class);
  if (!blast_class.has_value())
    return fail(blast_class.error());

  if (!blast_obj.contains("maxEntities") || !blast_obj["maxEntities"].is_number_integer()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 "Required integer field is missing: blastRadius.maxEntities"));
  }
  auto blast_limit = BlastRadiusLimit::parse_signed(blast_obj["maxEntities"].get<std::int64_t>());
  if (!blast_limit.has_value())
    return fail(blast_limit.error());

  auto idempotency_str = require_string(doc, "idempotencyKey");
  if (!idempotency_str.has_value())
    return fail(idempotency_str.error());
  auto idempotency_key = IdempotencyKey::create(*idempotency_str);
  if (!idempotency_key.has_value())
    return fail(idempotency_key.error());

  auto dry_run = require_string_field<DryRunRequirement>(doc, "dryRun", parse_dry_run);
  if (!dry_run.has_value())
    return fail(dry_run.error());

  auto side_effect = require_string_field<SideEffectClass>(doc, "sideEffects", parse_side_effect);
  if (!side_effect.has_value())
    return fail(side_effect.error());

  if (!doc.contains("requiredScopes") || !doc["requiredScopes"].is_array()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 "Required array field is missing: requiredScopes"));
  }
  std::vector<std::string> scopes;
  for (const auto& scope : doc["requiredScopes"]) {
    if (!scope.is_string()) {
      return fail(make_input_error("json.action_schema.required_scopes.invalid",
                                   "All requiredScopes entries must be strings."));
    }
    scopes.push_back(scope.get<std::string>());
  }
  auto required_scopes = NonEmptyVector<std::string>::create(std::move(scopes));
  if (!required_scopes.has_value())
    return fail(required_scopes.error());

  auto confirmation =
      require_string_field<ConfirmationMode>(doc, "confirmation", parse_confirmation);
  if (!confirmation.has_value())
    return fail(confirmation.error());

  auto rollback = require_string_field<RollbackStrategy>(doc, "rollback", parse_rollback);
  if (!rollback.has_value())
    return fail(rollback.error());

  if (!doc.contains("examples") || !doc["examples"].is_array()) {
    return fail(make_input_error("json.action_schema.missing_field",
                                 "Required array field is missing: examples"));
  }
  std::vector<ActionExample> examples;
  for (const auto& example : doc["examples"]) {
    if (!example.is_object()) {
      return fail(make_input_error("json.action_schema.examples.invalid",
                                   "All examples entries must be objects."));
    }
    auto intent = require_string(example, "intent");
    if (!intent.has_value())
      return fail(intent.error());
    if (!example.contains("parameters") || !example["parameters"].is_object()) {
      return fail(make_input_error("json.action_schema.examples.missing_parameters",
                                   "Each example must have a 'parameters' object."));
    }
    examples.push_back(
        ActionExample{.intent = *intent, .parameters_json = example["parameters"].dump()});
  }
  auto examples_vec = NonEmptyVector<ActionExample>::create(std::move(examples));
  if (!examples_vec.has_value())
    return fail(examples_vec.error());

  std::vector<ValidationRule> validation_rules;
  if (doc.contains("validationRules") && doc["validationRules"].is_array()) {
    for (const auto& rule : doc["validationRules"]) {
      auto name = require_string(rule, "name");
      if (!name.has_value())
        return fail(name.error());
      auto expression = require_string(rule, "expression");
      if (!expression.has_value())
        return fail(expression.error());
      validation_rules.push_back(ValidationRule{.name = *name, .expression = *expression});
    }
  }

  return ActionSchema::create(ActionSchemaDraft{
      .action_id = std::move(*action_id),
      .version = std::move(*version),
      .parameters = std::move(*parameters),
      .reversibility = *reversibility,
      .blast_radius = BlastRadius{.classification = *blast_class, .limit = *blast_limit},
      .idempotency_key = std::move(*idempotency_key),
      .dry_run = *dry_run,
      .side_effect = *side_effect,
      .required_scopes = std::move(*required_scopes),
      .confirmation = *confirmation,
      .rollback = *rollback,
      .examples = std::move(*examples_vec),
      .validation_rules = std::move(validation_rules),
  });
}

std::string serialize_action_schema_json(const ActionSchema& schema) {
  json doc;

  doc["id"] = schema.action_id().value();
  doc["version"] = schema.version().value();

  // parameters.json_schema is already a JSON string; parse it back into the doc
  try {
    doc["parameters"] = json::parse(schema.parameters().json_schema());
  } catch (...) {
    doc["parameters"] = schema.parameters().json_schema();
  }

  doc["reversibility"] = reversibility_to_string(schema.reversibility());
  doc["blastRadius"] = {
      {"class", blast_radius_class_to_string(schema.blast_radius().classification)},
      {"maxEntities", schema.blast_radius().limit.value()}};
  doc["idempotencyKey"] = schema.idempotency_key().expression();
  doc["dryRun"] = dry_run_to_string(schema.dry_run());
  doc["sideEffects"] = side_effect_to_string(schema.side_effect());

  doc["requiredScopes"] = schema.required_scopes().values();

  doc["confirmation"] = confirmation_to_string(schema.confirmation());
  doc["rollback"] = rollback_to_string(schema.rollback());

  json examples = json::array();
  for (const auto& example : schema.examples().values()) {
    json ex;
    ex["intent"] = example.intent;
    try {
      ex["parameters"] = json::parse(example.parameters_json);
    } catch (...) {
      ex["parameters"] = json::object();
    }
    examples.push_back(std::move(ex));
  }
  doc["examples"] = std::move(examples);

  json rules = json::array();
  std::transform(
      schema.validation_rules().begin(), schema.validation_rules().end(), std::back_inserter(rules),
      [](const auto& rule) { return json{{"name", rule.name}, {"expression", rule.expression}}; });
  doc["validationRules"] = std::move(rules);

  return doc.dump(2);
}

} // namespace aetheris::infrastructure
