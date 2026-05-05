#include <gtest/gtest.h>

#include "aetheris/infrastructure/noop_telemetry.hpp"
#include "aetheris/infrastructure/system_clock.hpp"
#include "aetheris/infrastructure/uuid_generator.hpp"
#include "support/port_contracts.hpp"

namespace {

TEST(SystemClockTest, ReturnsCurrentTime) {
  const aetheris::infrastructure::SystemClock clock;

  aetheris::tests::contracts::expect_clock_returns_time_inside_call_window(clock);
}

TEST(UuidGeneratorTest, ProducesValidUniqueIdentifiers) {
  aetheris::infrastructure::UuidGenerator generator{7};

  aetheris::tests::contracts::expect_id_generator_produces_valid_unique_values(generator);
}

TEST(NoopTelemetryTest, DropsTelemetryWithoutThrowing) {
  aetheris::infrastructure::NoopTelemetry telemetry;

  aetheris::tests::contracts::expect_telemetry_accepts_minimal_point(telemetry);
}

} // namespace
