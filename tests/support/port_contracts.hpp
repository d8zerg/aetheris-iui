#pragma once

#include <chrono>

#include <gtest/gtest.h>

#include "aetheris/domain/concepts.hpp"
#include "aetheris/domain/ports/clock_port.hpp"
#include "aetheris/domain/ports/id_generator_port.hpp"
#include "aetheris/domain/ports/telemetry_port.hpp"

namespace aetheris::tests::contracts {

template <domain::ClockPortLike TClock>
void expect_clock_returns_time_inside_call_window(const TClock& clock) {
  const auto before = std::chrono::system_clock::now();
  const auto observed = clock.now();
  const auto after = std::chrono::system_clock::now();

  EXPECT_GE(observed, before);
  EXPECT_LE(observed, after);
}

template <domain::IdGeneratorPortLike TGenerator>
void expect_id_generator_produces_valid_unique_values(TGenerator& generator) {
  const auto first_intent = generator.next_intent_id();
  const auto second_intent = generator.next_intent_id();
  const auto session = generator.next_session_id();
  const auto decision = generator.next_decision_id();

  ASSERT_TRUE(first_intent.has_value());
  ASSERT_TRUE(second_intent.has_value());
  ASSERT_TRUE(session.has_value());
  ASSERT_TRUE(decision.has_value());

  EXPECT_FALSE(first_intent->value().empty());
  EXPECT_FALSE(session->value().empty());
  EXPECT_FALSE(decision->value().empty());
  EXPECT_NE(first_intent->value(), second_intent->value());
}

template <domain::TelemetryPortLike TTelemetry>
void expect_telemetry_accepts_minimal_point(TTelemetry& telemetry) {
  const domain::TelemetryPoint point{.name = "contract.telemetry", .value = 1.0, .attributes = {}};

  EXPECT_NO_THROW(telemetry.emit(point));
}

} // namespace aetheris::tests::contracts
