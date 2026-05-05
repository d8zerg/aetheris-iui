#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Type-level tag for content that originates from an untrusted external source
 * (e.g., domain-system data, user-supplied strings, LLM output).
 *
 * Wrapping a value in Untrusted<T> forces every use site to make an explicit
 * decision before passing the value into a security-sensitive context such as
 * an LLM prompt.  The type system prevents accidental forwarding of raw
 * domain data into prompts, which is the primary injection vector.
 *
 * Usage:
 *   Untrusted<std::string> name = receive_from_domain_system();
 *   // Compiler error if you pass `name` where std::string is expected.
 *   // Must explicitly call .release() to acknowledge the trust decision.
 *   std::string safe = sanitize(name.release());
 */
template <typename T> class Untrusted final {
 public:
  explicit Untrusted(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
      : value_{std::move(value)} {}

  Untrusted(const Untrusted&) = default;
  Untrusted(Untrusted&&) noexcept = default;
  Untrusted& operator=(const Untrusted&) = default;
  Untrusted& operator=(Untrusted&&) noexcept = default;

  /**
   * Returns the raw value, acknowledging that the caller accepts responsibility
   * for preventing injection.  The call site becomes an explicit trust boundary.
   */
  [[nodiscard]] const T& release() const noexcept {
    return value_;
  }

  /**
   * Moves out the raw value.  After this call the Untrusted wrapper is consumed.
   */
  [[nodiscard]] T release_move() noexcept {
    return std::move(value_);
  }

 private:
  T value_;
};

// Convenience factory, mirrors std::make_unique ergonomics.
template <typename T>
[[nodiscard]] Untrusted<T>
make_untrusted(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
  return Untrusted<T>{std::move(value)};
}

/**
 * Enforces that the number of entities affected by an action does not exceed
 * the schema's declared blast radius limit.
 *
 * Returns PolicyError if `estimated_affected_entities` exceeds the limit.
 */
[[nodiscard]] inline Result<void>
enforce_blast_radius(const ActionSchema& schema,
                     std::uint64_t estimated_affected_entities) noexcept {
  const std::uint64_t limit = schema.blast_radius().limit.value();

  if (estimated_affected_entities > limit) {
    return fail(make_policy_error(
        "safety.blast_radius_exceeded",
        "Estimated affected entities exceeds the action's declared blast radius limit.",
        {ErrorDetail{"action_id", schema.action_id().value()},
         ErrorDetail{"limit", std::to_string(limit)},
         ErrorDetail{"estimated", std::to_string(estimated_affected_entities)}}));
  }

  return {};
}

} // namespace aetheris::domain
