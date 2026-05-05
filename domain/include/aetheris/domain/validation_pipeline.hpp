#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/capability.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/budget_controller_port.hpp"
#include "aetheris/domain/ports/dry_run_port.hpp"
#include "aetheris/domain/ports/rate_limiter_port.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/safety.hpp"
#include "aetheris/domain/schema_registry.hpp"

namespace aetheris::domain {

/**
 * Input to the validation pipeline for one intent-to-action mapping.
 *
 * Constructed after the LLM resolves an intent to a candidate action.
 * All fields come from trusted internal sources except `parameters_json`,
 * which is LLM-generated and must be treated with caution.
 */
struct ValidationRequest final {
  OperatorCapabilitySet capabilities;
  TenantId tenant_id;
  SessionId session_id;
  ActionId action_id;

  // Estimated number of entities this action will affect.
  // Used for blast radius enforcement.  Set to 0 if unknown.
  std::uint64_t estimated_affected_entities{0};

  // LLM-generated parameter payload (JSON).  Must match the schema's
  // ParameterSignature.  Validated by the schema stage.
  std::string parameters_json;
};

/**
 * Output produced by a successful pipeline run.
 */
struct ValidationResult final {
  // Non-owning pointer to the schema entry in the registry.
  // Valid for the lifetime of the registry.
  const ActionSchema* schema{nullptr};

  // Populated when the schema required a dry-run and a DryRunPort was provided.
  std::optional<std::string> dry_run_result_json;
};

/**
 * Chain-of-Responsibility validation pipeline.
 *
 * Stages (in order):
 *   1. Schema lookup        - InputError  if action not registered
 *   2. Permission check     - PolicyError if operator lacks capability
 *   3. Blast radius         - PolicyError if estimated entities exceed limit
 *   4. Rate limit           - PolicyError if operator exceeds per-minute quota
 *   5. Budget               - PolicyError if operator's session budget exhausted
 *   6. Dry-run              - PolicyError if mandatory dry-run but no adapter;
 *                             DomainError  if dry-run execution fails
 *
 * The pipeline short-circuits on the first failure.  On success it returns
 * a ValidationResult with the resolved schema and optional dry-run output.
 *
 * All injected references must outlive the pipeline instance.
 * `dry_run` may be nullptr; if so, schemas that require mandatory dry-run
 * will produce a PolicyError("validation.dry_run_unavailable").
 */
class ValidationPipeline final {
 public:
  ValidationPipeline(const ActionSchemaRegistry& registry, RateLimiterPort& rate_limiter,
                     BudgetControllerPort& budget_controller,
                     DryRunPort* dry_run = nullptr) noexcept
      : registry_{registry}, rate_limiter_{rate_limiter}, budget_controller_{budget_controller},
        dry_run_{dry_run} {}

  [[nodiscard]] Result<ValidationResult> validate(const ValidationRequest& req) const {
    // Stage 1: schema lookup
    const ActionSchema* schema = registry_.latest_for(req.action_id);
    if (schema == nullptr) {
      return fail(make_input_error("validation.schema_not_found",
                                   "No schema registered for the requested action.",
                                   {ErrorDetail{"action_id", req.action_id.value()}}));
    }

    // Stage 2: permission check
    if (auto perm = check_permission(req.capabilities, *schema); !perm.has_value()) {
      return fail(perm.error());
    }

    // Stage 3: blast radius enforcement
    if (auto blast = enforce_blast_radius(*schema, req.estimated_affected_entities);
        !blast.has_value()) {
      return fail(blast.error());
    }

    // Stage 4: rate limit
    if (auto rate = rate_limiter_.check_and_record(req.capabilities.operator_id, req.action_id,
                                                   schema->blast_radius().classification);
        !rate.has_value()) {
      return fail(rate.error());
    }

    // Stage 5: budget
    if (auto budget = budget_controller_.consume(req.capabilities.operator_id, req.session_id,
                                                 schema->blast_radius().classification,
                                                 req.estimated_affected_entities);
        !budget.has_value()) {
      return fail(budget.error());
    }

    // Stage 6: dry-run (only when schema mandates it)
    std::optional<std::string> dry_run_result;
    if (schema->dry_run() == DryRunRequirement::mandatory) {
      if (dry_run_ == nullptr) {
        return fail(make_policy_error(
            "validation.dry_run_unavailable",
            "Action requires a mandatory dry-run but no DryRunPort is configured.",
            {ErrorDetail{"action_id", req.action_id.value()}}));
      }

      auto result = dry_run_->execute(req.action_id, req.parameters_json);
      if (!result.has_value()) {
        return fail(result.error());
      }
      dry_run_result = std::move(*result);
    }

    return ValidationResult{.schema = schema, .dry_run_result_json = std::move(dry_run_result)};
  }

 private:
  const ActionSchemaRegistry& registry_;
  RateLimiterPort& rate_limiter_;
  BudgetControllerPort& budget_controller_;
  DryRunPort* dry_run_;
};

} // namespace aetheris::domain
