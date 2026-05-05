#pragma once

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Per-operator, per-blast-class rate limiter.
 *
 * Implementations enforce a sliding-window request count limit.
 * The port is injected into the ValidationPipeline so that tests
 * can substitute a stub that never rejects.
 */
class RateLimiterPort {
 public:
  RateLimiterPort() = default;
  RateLimiterPort(const RateLimiterPort&) = delete;
  RateLimiterPort& operator=(const RateLimiterPort&) = delete;
  RateLimiterPort(RateLimiterPort&&) = delete;
  RateLimiterPort& operator=(RateLimiterPort&&) = delete;
  virtual ~RateLimiterPort() = default;

  /**
   * Checks whether the operator is within their rate limit for the given
   * blast radius class and, if so, records the request.
   *
   * Returns PolicyError("validation.rate_limit_exceeded") when the limit is breached.
   */
  [[nodiscard]] virtual Result<void> check_and_record(const OperatorId& operator_id,
                                                      const ActionId& action_id,
                                                      BlastRadiusClass blast_class) = 0;

  /**
   * Resets all counters for the given operator.
   * Primarily used in tests and operator-session tear-down.
   */
  virtual void reset(const OperatorId& operator_id) noexcept = 0;
};

} // namespace aetheris::domain
