#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <tuple>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/budget_controller_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Cumulative blast unit budgets per blast radius class.
 *
 * A budget tracks total units consumed across a session lifetime.
 * Unlike the rate limiter there is no automatic reset - the budget
 * is drained until reset_operator is called.
 */
struct BudgetConfig final {
  std::uint64_t scoped_total{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t bounded_total{10'000};
  std::uint64_t broad_total{100};
};

/**
 * Thread-safe in-memory budget controller.
 *
 * Tracks cumulative consumed units per (operator_id, session_id, blast_class).
 * When the cumulative total exceeds the configured ceiling, further consume
 * calls return PolicyError("validation.budget_exhausted").
 */
class InMemoryBudgetController final : public domain::BudgetControllerPort {
 public:
  explicit InMemoryBudgetController(BudgetConfig config = {}) noexcept : config_{config} {}

  [[nodiscard]] domain::Result<void> consume(const domain::OperatorId& operator_id,
                                             const domain::SessionId& session_id,
                                             domain::BlastRadiusClass blast_class,
                                             std::uint64_t units) override {
    const std::uint64_t ceiling = ceiling_for(blast_class);
    const Key key{operator_id.value(), session_id.value(), blast_class};

    std::lock_guard lock{mutex_};

    auto& consumed = ledger_[key];

    // Check overflow before adding
    if (units > ceiling || consumed > ceiling - units) {
      return domain::fail(
          domain::make_policy_error("validation.budget_exhausted",
                                    "Operator's blast unit budget for this class is exhausted.",
                                    {domain::ErrorDetail{"operator_id", operator_id.value()},
                                     domain::ErrorDetail{"session_id", session_id.value()},
                                     domain::ErrorDetail{"ceiling", std::to_string(ceiling)},
                                     domain::ErrorDetail{"consumed", std::to_string(consumed)},
                                     domain::ErrorDetail{"requested", std::to_string(units)}}));
    }

    consumed += units;
    return {};
  }

  void reset_operator(const domain::OperatorId& operator_id) noexcept override {
    std::lock_guard lock{mutex_};
    for (auto it = ledger_.begin(); it != ledger_.end();) {
      if (it->first.operator_id == operator_id.value()) {
        it = ledger_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  [[nodiscard]] std::uint64_t ceiling_for(domain::BlastRadiusClass blast_class) const noexcept {
    switch (blast_class) {
    case domain::BlastRadiusClass::scoped:
      return config_.scoped_total;
    case domain::BlastRadiusClass::bounded:
      return config_.bounded_total;
    case domain::BlastRadiusClass::broad:
      return config_.broad_total;
    }
    return 0;
  }

  struct Key final {
    std::string operator_id;
    std::string session_id;
    domain::BlastRadiusClass blast_class;

    [[nodiscard]] friend bool operator<(const Key& a, const Key& b) noexcept {
      return std::tie(a.operator_id, a.session_id, a.blast_class) <
             std::tie(b.operator_id, b.session_id, b.blast_class);
    }
  };

  BudgetConfig config_;
  mutable std::mutex mutex_;
  std::map<Key, std::uint64_t> ledger_;
};

} // namespace aetheris::infrastructure
