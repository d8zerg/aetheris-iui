#pragma once

#include <cstdint>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Per-operator, per-session, per-blast-class blast unit budget.
 *
 * Each invocation of an action consumes `units` from the operator's
 * budget for that blast class.  When the budget is exhausted, further
 * actions in that class are blocked until the budget is reset.
 *
 * The budget tracks cumulative consumption - it does not recover over time.
 * A background lifecycle manager (e.g., session expiry) is responsible for
 * calling reset_operator.
 */
class BudgetControllerPort {
 public:
  BudgetControllerPort() = default;
  BudgetControllerPort(const BudgetControllerPort&) = delete;
  BudgetControllerPort& operator=(const BudgetControllerPort&) = delete;
  BudgetControllerPort(BudgetControllerPort&&) = delete;
  BudgetControllerPort& operator=(BudgetControllerPort&&) = delete;
  virtual ~BudgetControllerPort() = default;

  /**
   * Records consumption of `units` against the operator's blast-class budget.
   *
   * Returns PolicyError("validation.budget_exhausted") when the cumulative
   * total exceeds the configured ceiling.
   *
   * `units` corresponds to estimated_affected_entities for the action.
   */
  [[nodiscard]] virtual Result<void> consume(const OperatorId& operator_id,
                                             const SessionId& session_id,
                                             BlastRadiusClass blast_class, std::uint64_t units) = 0;

  /**
   * Resets all budget counters for the given operator across all sessions.
   */
  virtual void reset_operator(const OperatorId& operator_id) noexcept = 0;
};

} // namespace aetheris::domain
