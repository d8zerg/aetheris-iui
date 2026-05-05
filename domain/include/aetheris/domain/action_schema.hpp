#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/quantity.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

enum class ReversibilityClass {
  reversible,
  compensable,
  irreversible,
};

enum class BlastRadiusClass {
  scoped,
  bounded,
  broad,
};

enum class DryRunRequirement {
  mandatory,
  optional,
  not_applicable,
};

enum class SideEffectClass {
  read_only,
  writes_system,
  external_calls,
  notifications,
};

enum class ConfirmationMode {
  automatic,
  single,
  typed,
  multi_party,
  cooling_off,
};

enum class RollbackStrategy {
  none,
  rollback_api,
  compensating_action,
  manual,
};

/**
 * JSON Schema payload that describes action parameters.
 */
class ParameterSignature final {
 public:
  ParameterSignature() = delete;

  [[nodiscard]] static Result<ParameterSignature> create(std::string json_schema) {
    if (json_schema.empty()) {
      return fail(make_input_error("action_schema.parameters.empty",
                                   "Parameter schema must not be empty."));
    }

    return ParameterSignature{std::move(json_schema)};
  }

  [[nodiscard]] const std::string& json_schema() const noexcept {
    return json_schema_;
  }

 private:
  explicit ParameterSignature(std::string json_schema) : json_schema_{std::move(json_schema)} {}

  std::string json_schema_;
};

/**
 * Idempotency key generation expression declared by an Action Schema.
 */
class IdempotencyKey final {
 public:
  IdempotencyKey() = delete;

  [[nodiscard]] static Result<IdempotencyKey> create(std::string expression) {
    if (expression.empty()) {
      return fail(make_input_error("action_schema.idempotency.empty",
                                   "Idempotency expression must not be empty."));
    }

    return IdempotencyKey{std::move(expression)};
  }

  [[nodiscard]] const std::string& expression() const noexcept {
    return expression_;
  }

 private:
  explicit IdempotencyKey(std::string expression) : expression_{std::move(expression)} {}

  std::string expression_;
};

struct BlastRadius final {
  BlastRadiusClass classification;
  BlastRadiusLimit limit;
};

struct ValidationRule final {
  std::string name;
  std::string expression;
};

struct ActionExample final {
  std::string intent;
  std::string parameters_json;
};

struct ActionSchemaDraft final {
  ActionId action_id;
  SchemaVersion version;
  ParameterSignature parameters;
  ReversibilityClass reversibility;
  BlastRadius blast_radius;
  IdempotencyKey idempotency_key;
  DryRunRequirement dry_run;
  SideEffectClass side_effect;
  NonEmptyVector<std::string> required_scopes;
  ConfirmationMode confirmation;
  RollbackStrategy rollback;
  NonEmptyVector<ActionExample> examples;
  std::vector<ValidationRule> validation_rules;
};

/**
 * Public contract of one executable domain action.
 */
class ActionSchema final {
 public:
  ActionSchema() = delete;

  [[nodiscard]] static Result<ActionSchema> create(ActionSchemaDraft draft) {
    if (const auto validation = validate(draft); !validation.has_value()) {
      return fail(validation.error());
    }

    return ActionSchema{std::move(draft)};
  }

  [[nodiscard]] const ActionId& action_id() const noexcept {
    return draft_.action_id;
  }

  [[nodiscard]] const SchemaVersion& version() const noexcept {
    return draft_.version;
  }

  [[nodiscard]] const ParameterSignature& parameters() const noexcept {
    return draft_.parameters;
  }

  [[nodiscard]] ReversibilityClass reversibility() const noexcept {
    return draft_.reversibility;
  }

  [[nodiscard]] BlastRadius blast_radius() const noexcept {
    return draft_.blast_radius;
  }

  [[nodiscard]] const IdempotencyKey& idempotency_key() const noexcept {
    return draft_.idempotency_key;
  }

  [[nodiscard]] DryRunRequirement dry_run() const noexcept {
    return draft_.dry_run;
  }

  [[nodiscard]] SideEffectClass side_effect() const noexcept {
    return draft_.side_effect;
  }

  [[nodiscard]] const NonEmptyVector<std::string>& required_scopes() const noexcept {
    return draft_.required_scopes;
  }

  [[nodiscard]] ConfirmationMode confirmation() const noexcept {
    return draft_.confirmation;
  }

  [[nodiscard]] RollbackStrategy rollback() const noexcept {
    return draft_.rollback;
  }

  [[nodiscard]] const NonEmptyVector<ActionExample>& examples() const noexcept {
    return draft_.examples;
  }

  [[nodiscard]] const std::vector<ValidationRule>& validation_rules() const noexcept {
    return draft_.validation_rules;
  }

 private:
  explicit ActionSchema(ActionSchemaDraft draft) : draft_{std::move(draft)} {}

  [[nodiscard]] static Result<void> validate(const ActionSchemaDraft& draft) {
    if (auto r = validate_rollback(draft); !r.has_value()) {
      return r;
    }

    if (auto r = validate_confirmation(draft); !r.has_value()) {
      return r;
    }

    if (auto r = validate_dry_run(draft); !r.has_value()) {
      return r;
    }

    for (const auto& example : draft.examples.values()) {
      if (example.intent.empty()) {
        return fail(make_input_error("action_schema.example.intent_empty",
                                     "Action examples must include a non-empty intent."));
      }
      if (example.parameters_json.empty()) {
        return fail(make_input_error("action_schema.example.parameters_empty",
                                     "Action examples must include parameter JSON."));
      }
    }

    return {};
  }

  [[nodiscard]] static Result<void> validate_rollback(const ActionSchemaDraft& draft) {
    if (draft.reversibility == ReversibilityClass::reversible &&
        draft.rollback != RollbackStrategy::rollback_api) {
      return fail(make_input_error("action_schema.rollback.reversible_requires_api",
                                   "Reversible actions must declare rollback API strategy."));
    }

    if (draft.reversibility == ReversibilityClass::compensable &&
        draft.rollback != RollbackStrategy::compensating_action) {
      return fail(make_input_error("action_schema.rollback.compensable_requires_action",
                                   "Compensable actions must declare a compensating action."));
    }

    if (draft.reversibility == ReversibilityClass::irreversible &&
        (draft.rollback == RollbackStrategy::rollback_api ||
         draft.rollback == RollbackStrategy::compensating_action)) {
      return fail(make_input_error("action_schema.rollback.irreversible_has_rollback",
                                   "Irreversible actions cannot declare automated rollback."));
    }

    return {};
  }

  [[nodiscard]] static Result<void> validate_confirmation(const ActionSchemaDraft& draft) {
    if (draft.confirmation == ConfirmationMode::automatic &&
        (draft.side_effect != SideEffectClass::read_only ||
         draft.blast_radius.classification != BlastRadiusClass::scoped)) {
      return fail(
          make_input_error("action_schema.confirmation.automatic_not_low_risk",
                           "Automatic confirmation is limited to scoped read-only actions."));
    }

    if (draft.blast_radius.classification == BlastRadiusClass::bounded &&
        draft.confirmation == ConfirmationMode::automatic) {
      return fail(make_input_error("action_schema.confirmation.bounded_requires_operator",
                                   "Bounded actions require operator confirmation."));
    }

    if (draft.blast_radius.classification == BlastRadiusClass::broad &&
        draft.confirmation != ConfirmationMode::multi_party &&
        draft.confirmation != ConfirmationMode::cooling_off) {
      return fail(
          make_input_error("action_schema.confirmation.broad_requires_strict_mode",
                           "Broad actions require multi-party or cooling-off confirmation."));
    }

    return {};
  }

  [[nodiscard]] static Result<void> validate_dry_run(const ActionSchemaDraft& draft) {
    if (draft.side_effect == SideEffectClass::read_only &&
        draft.dry_run != DryRunRequirement::not_applicable) {
      return fail(make_input_error("action_schema.dry_run.read_only_not_applicable",
                                   "Read-only actions must mark dry-run as not applicable."));
    }

    if (draft.reversibility == ReversibilityClass::irreversible &&
        draft.side_effect != SideEffectClass::read_only &&
        draft.dry_run != DryRunRequirement::mandatory) {
      return fail(make_input_error("action_schema.dry_run.irreversible_requires_dry_run",
                                   "Irreversible write actions require mandatory dry-run."));
    }

    return {};
  }

  ActionSchemaDraft draft_;
};

} // namespace aetheris::domain
