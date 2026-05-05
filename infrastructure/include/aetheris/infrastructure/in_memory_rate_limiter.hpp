#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/clock_port.hpp"
#include "aetheris/domain/ports/rate_limiter_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Per-blast-class request limits for the rate limiter.
 *
 * Limits apply per operator per sliding window (default: 1 minute).
 * Set a limit to std::numeric_limits<std::uint64_t>::max() to disable it.
 */
struct RateLimitConfig final {
  std::uint64_t scoped_per_minute{1'000};
  std::uint64_t bounded_per_minute{100};
  std::uint64_t broad_per_minute{10};
  std::chrono::seconds window_duration{60};
};

/**
 * Thread-safe in-memory rate limiter with a sliding fixed-window strategy.
 *
 * For each (operator_id, blast_class) pair, the limiter tracks the request
 * count and the window start time.  When the current time exceeds
 * window_start + window_duration, the counter resets automatically.
 */
class InMemoryRateLimiter final : public domain::RateLimiterPort {
 public:
  explicit InMemoryRateLimiter(const domain::ClockPort& clock, RateLimitConfig config = {}) noexcept
      : clock_{clock}, config_{config} {}

  [[nodiscard]] domain::Result<void>
  check_and_record(const domain::OperatorId& operator_id, const domain::ActionId& action_id,
                   domain::BlastRadiusClass blast_class) override {
    const auto now = clock_.now();
    const std::uint64_t limit = limit_for(blast_class);

    const Key key{operator_id.value(), blast_class};

    std::lock_guard lock{mutex_};

    auto& bucket = buckets_[key];

    // Reset window if expired
    if (now >= bucket.window_start + config_.window_duration) {
      bucket.count = 0;
      bucket.window_start = now;
    }

    if (bucket.count >= limit) {
      return domain::fail(domain::make_policy_error(
          "validation.rate_limit_exceeded",
          "Operator has exceeded the rate limit for this blast radius class.",
          {domain::ErrorDetail{"operator_id", operator_id.value()},
           domain::ErrorDetail{"action_id", action_id.value()},
           domain::ErrorDetail{"limit", std::to_string(limit)}}));
    }

    ++bucket.count;
    return {};
  }

  void reset(const domain::OperatorId& operator_id) noexcept override {
    std::lock_guard lock{mutex_};
    for (auto it = buckets_.begin(); it != buckets_.end();) {
      if (it->first.operator_id == operator_id.value()) {
        it = buckets_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  [[nodiscard]] std::uint64_t limit_for(domain::BlastRadiusClass blast_class) const noexcept {
    switch (blast_class) {
    case domain::BlastRadiusClass::scoped:
      return config_.scoped_per_minute;
    case domain::BlastRadiusClass::bounded:
      return config_.bounded_per_minute;
    case domain::BlastRadiusClass::broad:
      return config_.broad_per_minute;
    }
    return 0;
  }

  struct Key final {
    std::string operator_id;
    domain::BlastRadiusClass blast_class;

    [[nodiscard]] friend bool operator<(const Key& a, const Key& b) noexcept {
      if (a.operator_id != b.operator_id)
        return a.operator_id < b.operator_id;
      return static_cast<int>(a.blast_class) < static_cast<int>(b.blast_class);
    }
  };

  struct Bucket final {
    std::uint64_t count{0};
    std::chrono::system_clock::time_point window_start{};
  };

  const domain::ClockPort& clock_;
  RateLimitConfig config_;
  mutable std::mutex mutex_;
  std::map<Key, Bucket> buckets_;
};

} // namespace aetheris::infrastructure
