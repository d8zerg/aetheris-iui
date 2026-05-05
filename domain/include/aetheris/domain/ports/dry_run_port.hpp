#pragma once

#include <string>
#include <string_view>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Sandboxed dry-run execution contract.
 *
 * A dry-run executes the action in an isolated environment without causing
 * real-world side effects, returning a prediction of what would happen.
 *
 * The port is implemented by domain-system adapters.  The ValidationPipeline
 * calls it when the action schema mandates a dry-run before committing.
 *
 * The result_json contains the simulated outcome - format is action-specific
 * and defined by the adapter.
 */
class DryRunPort {
 public:
  DryRunPort() = default;
  DryRunPort(const DryRunPort&) = delete;
  DryRunPort& operator=(const DryRunPort&) = delete;
  DryRunPort(DryRunPort&&) = delete;
  DryRunPort& operator=(DryRunPort&&) = delete;
  virtual ~DryRunPort() = default;

  /**
   * Executes the action in sandbox mode.
   *
   * Returns the simulated result as a JSON string, or a DomainError if the
   * dry-run itself fails (e.g., sandbox unavailable, validation rejected).
   */
  [[nodiscard]] virtual Result<std::string> execute(const ActionId& action_id,
                                                    std::string_view parameters_json) = 0;
};

} // namespace aetheris::domain
