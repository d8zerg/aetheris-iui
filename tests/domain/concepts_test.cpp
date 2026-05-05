#include <gtest/gtest.h>

#include "aetheris/domain/concepts.hpp"
#include "aetheris/infrastructure/noop_telemetry.hpp"
#include "aetheris/infrastructure/system_clock.hpp"
#include "aetheris/infrastructure/uuid_generator.hpp"

namespace {

static_assert(aetheris::domain::IdentifierLike<aetheris::domain::ActionId>);
static_assert(aetheris::domain::IdentifierLike<aetheris::domain::SessionId>);
static_assert(aetheris::domain::ClockPortLike<aetheris::infrastructure::SystemClock>);
static_assert(aetheris::domain::IdGeneratorPortLike<aetheris::infrastructure::UuidGenerator>);
static_assert(aetheris::domain::TelemetryPortLike<aetheris::infrastructure::NoopTelemetry>);

TEST(ConceptsTest, CompileTimeContractsAcceptDefaultImplementations) {
  SUCCEED();
}

} // namespace
