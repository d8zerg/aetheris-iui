#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"
#include "aetheris/infrastructure/in_memory_session_repository.hpp"
#include "aetheris/infrastructure/session_json.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

// Simulated system clock
class FakeDialogueClock {
 public:
  std::chrono::system_clock::time_point now() const noexcept {
    return t_;
  }
  void advance(std::chrono::seconds delta) noexcept {
    t_ += delta;
  }

 private:
  std::chrono::system_clock::time_point t_{std::chrono::seconds{10000}};
};

/**
 * Integration test: full multi-turn dialogue without LLM.
 *
 * Simulates the sequence:
 *   1. Operator initiates "disable camera" intent
 *   2. System asks for clarification (ambiguous camera)
 *   3. Operator clarifies -> slot filled
 *   4. System shows dry-run preview
 *   5. Operator rejects preview, changes parameter
 *   6. System shows updated preview
 *   7. Operator confirms
 *   8. Action completes -> session archived
 *
 * Repository persistence is exercised at each step (save -> reload).
 */
TEST(SessionLifecycleTest, MultiTurnDialogueWithClarificationAndPreviewRejection) {
  InMemorySessionRepository repo;
  FakeDialogueClock clock;

  // Step 1: create session with an unfilled required slot
  const auto session_id = *SessionId::parse("integration-sess-1");
  {
    auto session = *IntentSession::create(
        session_id, *OperatorId::parse("op-alice"), *TenantId::parse("tenant-acme"),
        *ActionId::parse("camera.disable"),
        {Slot{.name = "cameraId", .required = true, .value_json = std::nullopt}}, clock.now(),
        std::chrono::seconds{300});

    ASSERT_EQ(session.state(), SessionState::fill);
    ASSERT_FALSE(session.all_required_slots_filled());
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{1});

  // Step 2: request clarification (camera is ambiguous)
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(session.request_clarification("Multiple cameras found - which one?", clock.now())
                    .has_value());
    ASSERT_EQ(session.state(), SessionState::clarification);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{5});

  // Step 3: operator provides clarification, slot filled
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(
        session.accept_clarification("Camera in lobby, ID cam-007", clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::fill);
    ASSERT_TRUE(session.fill_slot("cameraId", R"("cam-007")", clock.now()).has_value());
    ASSERT_TRUE(session.all_required_slots_filled());
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{1});

  // Step 4: system runs dry-run and presents preview
  {
    auto session = *repo.load(session_id);
    const std::string dry_run_result = R"({"affected_cameras":1,"camera_id":"cam-007"})";
    ASSERT_TRUE(session.preview(dry_run_result, clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::preview);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{3});

  // Step 5: operator rejects preview ("wrong camera, use cam-008")
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(session.reject_preview(clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::fill);
    ASSERT_TRUE(session.fill_slot("cameraId", R"("cam-008")", clock.now()).has_value());
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{1});

  // Step 6: re-run dry-run with updated parameter
  {
    auto session = *repo.load(session_id);
    const std::string updated_dry_run = R"({"affected_cameras":1,"camera_id":"cam-008"})";
    ASSERT_TRUE(session.preview(updated_dry_run, clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::preview);
    EXPECT_EQ(session.preview_result_json(), updated_dry_run);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{2});

  // Step 7: operator confirms
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(session.confirm(clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::commit);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  clock.advance(std::chrono::seconds{1});

  // Step 8: action completes
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(
        session.complete(R"({"status":"ok","camera_id":"cam-008"})", clock.now()).has_value());
    ASSERT_EQ(session.state(), SessionState::archive);
    ASSERT_EQ(*session.archive_reason(), ArchiveReason::completed);

    // Verify final slot state is preserved
    ASSERT_EQ(session.slots().size(), 1U);
    ASSERT_TRUE(session.slots()[0].value_json.has_value());
    EXPECT_EQ(*session.slots()[0].value_json, R"("cam-008")");

    ASSERT_TRUE(repo.save(session).has_value());
  }

  // Final verification: session is archived and not in active list
  const auto active =
      repo.list_active_for(*OperatorId::parse("op-alice"), *TenantId::parse("tenant-acme"));
  ASSERT_TRUE(active.has_value());
  EXPECT_TRUE(active->empty());
}

/**
 * Integration test: session interrupted mid-dialogue and cancelled.
 */
TEST(SessionLifecycleTest, SessionInterruptedByCancellation) {
  InMemorySessionRepository repo;
  const auto kT = std::chrono::system_clock::time_point{std::chrono::seconds{20000}};

  const auto session_id = *SessionId::parse("interrupted-sess");
  {
    auto session = *IntentSession::create(
        session_id, *OperatorId::parse("op-bob"), *TenantId::parse("tenant-x"),
        *ActionId::parse("facility.lockdown"),
        {Slot{.name = "facilityId", .required = true, .value_json = std::nullopt}}, kT);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  // Operator starts filling but then cancels
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(session.fill_slot("facilityId", R"("building-a")", kT + std::chrono::seconds{5})
                    .has_value());
    ASSERT_TRUE(
        session.cancel("operator changed intent", kT + std::chrono::seconds{10}).has_value());
    ASSERT_EQ(session.state(), SessionState::archive);
    ASSERT_EQ(*session.archive_reason(), ArchiveReason::cancelled);
    ASSERT_TRUE(repo.save(session).has_value());
  }

  // No active sessions remain
  const auto active =
      repo.list_active_for(*OperatorId::parse("op-bob"), *TenantId::parse("tenant-x"));
  ASSERT_TRUE(active.has_value());
  EXPECT_TRUE(active->empty());
}

/**
 * Integration test: TTL expiry collector.
 */
TEST(SessionLifecycleTest, ExpiredSessionsCollectedByGarbageCollector) {
  InMemorySessionRepository repo;
  const auto kT = std::chrono::system_clock::time_point{std::chrono::seconds{30000}};

  // Two sessions with short TTL, one with long TTL
  for (const auto* id : {"exp-1", "exp-2"}) {
    auto session = *IntentSession::create(*SessionId::parse(id), *OperatorId::parse("op-gc"),
                                          *TenantId::parse("tenant-gc"), *ActionId::parse("act.x"),
                                          {}, kT, std::chrono::seconds{60});
    ASSERT_TRUE(repo.save(session).has_value());
  }
  {
    auto session = *IntentSession::create(
        *SessionId::parse("long-lived"), *OperatorId::parse("op-gc"), *TenantId::parse("tenant-gc"),
        *ActionId::parse("act.x"), {}, kT, std::chrono::seconds{3600});
    ASSERT_TRUE(repo.save(session).has_value());
  }

  // Simulate GC pass after 90 seconds
  const auto gc_now = kT + std::chrono::seconds{90};
  const auto expired_ids = repo.list_expired(gc_now);
  ASSERT_TRUE(expired_ids.has_value());
  ASSERT_EQ(expired_ids->size(), 2U);

  for (const auto& id : *expired_ids) {
    auto session = *repo.load(id);
    ASSERT_TRUE(session.expire(gc_now).has_value());
    ASSERT_TRUE(repo.save(session).has_value());
  }

  // After GC, no sessions should be reported as expired again
  const auto after_gc = repo.list_expired(gc_now + std::chrono::seconds{1});
  ASSERT_TRUE(after_gc.has_value());
  EXPECT_TRUE(after_gc->empty());

  // Long-lived session still active
  const auto active =
      repo.list_active_for(*OperatorId::parse("op-gc"), *TenantId::parse("tenant-gc"));
  ASSERT_TRUE(active.has_value());
  ASSERT_EQ(active->size(), 1U);
  EXPECT_EQ((*active)[0].value(), "long-lived");
}

/**
 * Integration test: JSON persistence round-trip across repository save/load.
 */
TEST(SessionLifecycleTest, SessionSurvivesJsonRoundTripPersistence) {
  InMemorySessionRepository repo;
  const auto kT = std::chrono::system_clock::time_point{std::chrono::seconds{40000}};

  const auto session_id = *SessionId::parse("json-persist-sess");
  {
    auto session =
        *IntentSession::create(session_id, *OperatorId::parse("op-x"), *TenantId::parse("t-x"),
                               *ActionId::parse("cam.disable"),
                               {Slot{.name = "id", .required = true, .value_json = R"("42")"}}, kT);
    ASSERT_TRUE(session.preview(R"({"ok":true})", kT + std::chrono::seconds{1}).has_value());
    ASSERT_TRUE(repo.save(session).has_value());
  }

  // Simulate restart by serializing then parsing then re-saving
  {
    auto loaded = *repo.load(session_id);
    const std::string json = serialize_session_json(loaded);
    auto restored = parse_session_json(json);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->state(), SessionState::preview);
    EXPECT_EQ(restored->id().value(), session_id.value());
    ASSERT_TRUE(repo.save(*restored).has_value());
  }

  // Session is still usable after restoration
  {
    auto session = *repo.load(session_id);
    ASSERT_TRUE(session.confirm(kT + std::chrono::seconds{5}).has_value());
    ASSERT_EQ(session.state(), SessionState::commit);
  }
}

} // namespace
