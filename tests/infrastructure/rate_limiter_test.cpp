#include <chrono>

#include <gtest/gtest.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/clock_port.hpp"
#include "aetheris/infrastructure/in_memory_budget_controller.hpp"
#include "aetheris/infrastructure/in_memory_rate_limiter.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

// Controllable clock for deterministic rate limiter tests.
class FakeClock final : public ClockPort {
 public:
  explicit FakeClock(std::chrono::system_clock::time_point t) noexcept : now_{t} {}

  [[nodiscard]] std::chrono::system_clock::time_point now() const noexcept override {
    return now_;
  }

  void advance(std::chrono::seconds delta) noexcept {
    now_ += delta;
  }

 private:
  std::chrono::system_clock::time_point now_;
};

const auto kEpoch = std::chrono::system_clock::time_point{std::chrono::seconds{0}};
const auto kActionId = *ActionId::parse("camera.disable");
const auto kOpAlice = *OperatorId::parse("op-alice");
const auto kOpBob = *OperatorId::parse("op-bob");

// ---- InMemoryRateLimiter ----

TEST(InMemoryRateLimiterTest, AllowsRequestsWithinLimit) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 3;
  InMemoryRateLimiter limiter{clock, config};

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value())
        << "request " << i << " should succeed";
  }
}

TEST(InMemoryRateLimiterTest, BlocksRequestsExceedingLimit) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 2;
  InMemoryRateLimiter limiter{clock, config};

  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());

  const auto rejected = limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped);
  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(error_code(rejected.error()), "validation.rate_limit_exceeded");
}

TEST(InMemoryRateLimiterTest, ResetsCounterAfterWindowExpiry) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 1;
  config.window_duration = std::chrono::seconds{60};
  InMemoryRateLimiter limiter{clock, config};

  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  ASSERT_FALSE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());

  // Advance past window boundary
  clock.advance(std::chrono::seconds{61});

  // Counter should have reset
  EXPECT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
}

TEST(InMemoryRateLimiterTest, ManualResetClearsCounter) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.bounded_per_minute = 1;
  InMemoryRateLimiter limiter{clock, config};

  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::bounded).has_value());
  ASSERT_FALSE(
      limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::bounded).has_value());

  limiter.reset(kOpAlice);

  EXPECT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::bounded).has_value());
}

TEST(InMemoryRateLimiterTest, LimitsAreIndependentPerBlastClass) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 100;
  config.bounded_per_minute = 1;
  InMemoryRateLimiter limiter{clock, config};

  // scoped still works after bounded is exhausted
  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::bounded).has_value());
  ASSERT_FALSE(
      limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::bounded).has_value());
  EXPECT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
}

TEST(InMemoryRateLimiterTest, CountersAreIndependentPerOperator) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 1;
  InMemoryRateLimiter limiter{clock, config};

  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  ASSERT_FALSE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());

  // Bob has his own counter
  EXPECT_TRUE(limiter.check_and_record(kOpBob, kActionId, BlastRadiusClass::scoped).has_value());
}

TEST(InMemoryRateLimiterTest, ResetDoesNotAffectOtherOperators) {
  FakeClock clock{kEpoch};
  RateLimitConfig config;
  config.scoped_per_minute = 1;
  InMemoryRateLimiter limiter{clock, config};

  ASSERT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  ASSERT_TRUE(limiter.check_and_record(kOpBob, kActionId, BlastRadiusClass::scoped).has_value());

  // Exhaust Alice's slot
  ASSERT_FALSE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  // Bob already used his slot too
  ASSERT_FALSE(limiter.check_and_record(kOpBob, kActionId, BlastRadiusClass::scoped).has_value());

  // Reset only Alice
  limiter.reset(kOpAlice);

  EXPECT_TRUE(limiter.check_and_record(kOpAlice, kActionId, BlastRadiusClass::scoped).has_value());
  // Bob is still blocked
  EXPECT_FALSE(limiter.check_and_record(kOpBob, kActionId, BlastRadiusClass::scoped).has_value());
}

// ---- InMemoryBudgetController ----

const auto kSession1 = *SessionId::parse("session-1");
const auto kSession2 = *SessionId::parse("session-2");

TEST(InMemoryBudgetControllerTest, AllowsConsumptionWithinBudget) {
  BudgetConfig config;
  config.bounded_total = 100;
  InMemoryBudgetController budget{config};

  EXPECT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 50).has_value());
  EXPECT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 50).has_value());
}

TEST(InMemoryBudgetControllerTest, BlocksConsumptionExceedingBudget) {
  BudgetConfig config;
  config.bounded_total = 10;
  InMemoryBudgetController budget{config};

  ASSERT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 10).has_value());

  const auto rejected = budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 1);
  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(error_code(rejected.error()), "validation.budget_exhausted");
}

TEST(InMemoryBudgetControllerTest, RejectsUnitsThatWouldImmediatelyOverflow) {
  BudgetConfig config;
  config.broad_total = 5;
  InMemoryBudgetController budget{config};

  const auto rejected = budget.consume(kOpAlice, kSession1, BlastRadiusClass::broad, 100);
  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(error_code(rejected.error()), "validation.budget_exhausted");
}

TEST(InMemoryBudgetControllerTest, ResetOperatorClearsAllSessions) {
  BudgetConfig config;
  config.bounded_total = 5;
  InMemoryBudgetController budget{config};

  ASSERT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 5).has_value());
  ASSERT_FALSE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 1).has_value());

  budget.reset_operator(kOpAlice);

  EXPECT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 5).has_value());
}

TEST(InMemoryBudgetControllerTest, BudgetsAreIndependentPerSession) {
  BudgetConfig config;
  config.bounded_total = 10;
  InMemoryBudgetController budget{config};

  ASSERT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 10).has_value());
  // session-2 has its own budget
  EXPECT_TRUE(budget.consume(kOpAlice, kSession2, BlastRadiusClass::bounded, 10).has_value());
}

TEST(InMemoryBudgetControllerTest, ResetDoesNotAffectOtherOperators) {
  BudgetConfig config;
  config.bounded_total = 5;
  InMemoryBudgetController budget{config};

  ASSERT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 5).has_value());
  ASSERT_TRUE(budget.consume(kOpBob, kSession1, BlastRadiusClass::bounded, 5).has_value());

  budget.reset_operator(kOpAlice);

  // Alice reset, Bob still exhausted
  EXPECT_TRUE(budget.consume(kOpAlice, kSession1, BlastRadiusClass::bounded, 5).has_value());
  EXPECT_FALSE(budget.consume(kOpBob, kSession1, BlastRadiusClass::bounded, 1).has_value());
}

} // namespace
