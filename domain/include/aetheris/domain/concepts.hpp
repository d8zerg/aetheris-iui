#pragma once

#include <chrono>
#include <concepts>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/clock_port.hpp"
#include "aetheris/domain/ports/id_generator_port.hpp"
#include "aetheris/domain/ports/telemetry_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Compile-time contract for identifier-like domain types.
 */
template <typename TIdentifier>
concept IdentifierLike = requires(TIdentifier identifier) {
  { TIdentifier::parse(identifier.view()) } -> std::same_as<Result<TIdentifier>>;
  { identifier.value() } -> std::same_as<const std::string&>;
  { identifier.view() } -> std::same_as<std::string_view>;
};

/**
 * Compile-time contract for ClockPort implementations used in public templates.
 */
template <typename TClock>
concept ClockPortLike = std::derived_from<TClock, ClockPort> && requires(const TClock& clock) {
  { clock.now() } noexcept -> std::same_as<std::chrono::system_clock::time_point>;
};

/**
 * Compile-time contract for IdGeneratorPort implementations.
 */
template <typename TGenerator>
concept IdGeneratorPortLike =
    std::derived_from<TGenerator, IdGeneratorPort> && requires(TGenerator& generator) {
      { generator.next_intent_id() } -> std::same_as<Result<IntentId>>;
      { generator.next_session_id() } -> std::same_as<Result<SessionId>>;
      { generator.next_decision_id() } -> std::same_as<Result<DecisionId>>;
    };

/**
 * Compile-time contract for TelemetryPort implementations.
 */
template <typename TTelemetry>
concept TelemetryPortLike = std::derived_from<TTelemetry, TelemetryPort> &&
                            requires(TTelemetry& telemetry, const TelemetryPoint& point) {
                              { telemetry.emit(point) } noexcept -> std::same_as<void>;
                            };

} // namespace aetheris::domain
