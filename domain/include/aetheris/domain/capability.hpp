#pragma once

#include <set>
#include <string>
#include <vector>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/schema_registry.hpp"

namespace aetheris::domain {

/**
 * The set of permissions granted to a single operator for one request context.
 *
 * Capability sets are constructed by the operator authentication layer and
 * passed into the validation pipeline.  They are intentionally immutable after
 * construction so that no downstream component can escalate its own privileges.
 */
struct OperatorCapabilitySet final {
  OperatorId operator_id;

  // Explicit allow-list of action IDs the operator may invoke.
  // An empty set means the operator has no permitted actions.
  std::set<std::string> permitted_action_ids;

  // OAuth-style scopes the operator currently holds.
  // The action schema's required_scopes must be a subset of this set.
  std::set<std::string> granted_scopes;

  // Upper bound on the blast radius class the operator may trigger.
  // Actions with a higher classification are rejected.
  BlastRadiusClass max_blast_radius{BlastRadiusClass::scoped};
};

/**
 * Checks whether an operator's capability set permits invoking a specific action.
 *
 * Returns PolicyError if:
 *   - action_id is not in permitted_action_ids
 *   - any of the schema's required_scopes is absent from granted_scopes
 *   - the schema's blast_radius classification exceeds the operator's max_blast_radius
 */
[[nodiscard]] inline Result<void> check_permission(const OperatorCapabilitySet& capabilities,
                                                   const ActionSchema& schema) noexcept {
  const std::string& action_id = schema.action_id().value();

  if (!capabilities.permitted_action_ids.contains(action_id)) {
    return fail(make_policy_error("capability.action_not_permitted",
                                  "Operator is not permitted to invoke this action.",
                                  {ErrorDetail{"action_id", action_id},
                                   ErrorDetail{"operator_id", capabilities.operator_id.value()}}));
  }

  for (const auto& required_scope : schema.required_scopes().values()) {
    if (!capabilities.granted_scopes.contains(required_scope)) {
      return fail(make_policy_error(
          "capability.scope_missing", "Operator does not hold a required scope for this action.",
          {ErrorDetail{"action_id", action_id}, ErrorDetail{"missing_scope", required_scope},
           ErrorDetail{"operator_id", capabilities.operator_id.value()}}));
    }
  }

  const auto schema_class = static_cast<int>(schema.blast_radius().classification);
  const auto max_class = static_cast<int>(capabilities.max_blast_radius);
  if (schema_class > max_class) {
    return fail(make_policy_error(
        "capability.blast_radius_exceeds_operator_ceiling",
        "Action blast radius class exceeds the operator's maximum permitted class.",
        {ErrorDetail{"action_id", action_id},
         ErrorDetail{"operator_id", capabilities.operator_id.value()}}));
  }

  return {};
}

/**
 * Returns all schemas in the registry that the operator is permitted to invoke.
 *
 * Only the latest version of each action is considered.
 * The returned pointers are non-owning and valid as long as `registry` is alive.
 */
[[nodiscard]] inline std::vector<const ActionSchema*>
filter_permitted_schemas(const OperatorCapabilitySet& capabilities,
                         const ActionSchemaRegistry& registry) noexcept {
  std::vector<const ActionSchema*> result;

  for (const auto& action_id_str : capabilities.permitted_action_ids) {
    // Safe: Identifier::parse validates but we already know these come from
    // a validated capability set.  If somehow invalid, skip gracefully.
    const auto action_id = ActionId::parse(action_id_str);
    if (!action_id.has_value()) {
      continue;
    }

    const ActionSchema* schema = registry.latest_for(*action_id);
    if (schema == nullptr) {
      continue;
    }

    if (check_permission(capabilities, *schema).has_value()) {
      result.push_back(schema);
    }
  }

  return result;
}

} // namespace aetheris::domain
