#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"

namespace {

using namespace aetheris::domain;
using tp = std::chrono::system_clock::time_point;

const auto kT0 = tp{std::chrono::seconds{1000}};
const auto kT1 = tp{std::chrono::seconds{1001}};
const auto kT2 = tp{std::chrono::seconds{1002}};
const auto kT3 = tp{std::chrono::seconds{1003}};

[[nodiscard]] IntentSession make_session(std::vector<Slot> slots = {},
                                         std::chrono::seconds ttl = std::chrono::seconds{3600}) {
  return *IntentSession::create(*SessionId::parse("sess-1"), *OperatorId::parse("op-alice"),
                                *TenantId::parse("tenant-acme"), *ActionId::parse("camera.disable"),
                                std::move(slots), kT0, ttl);
}

[[nodiscard]] Slot required_slot(std::string name,
                                 std::optional<std::string> value = std::nullopt) {
  return Slot{.name = std::move(name), .required = true, .value_json = std::move(value)};
}

[[nodiscard]] Slot optional_slot(std::string name) {
  return Slot{.name = std::move(name), .required = false, .value_json = std::nullopt};
}

// ---- create ----

TEST(IntentSessionTest, CreateStartsInFillState) {
  const auto s = make_session();
  EXPECT_EQ(s.state(), SessionState::fill);
}

TEST(IntentSessionTest, CreateSetsCreatedAndUpdatedAt) {
  const auto s = make_session();
  EXPECT_EQ(s.created_at(), kT0);
  EXPECT_EQ(s.updated_at(), kT0);
}

TEST(IntentSessionTest, CreatePreservesImmutableFields) {
  const auto s = make_session();
  EXPECT_EQ(s.operator_id().value(), "op-alice");
  EXPECT_EQ(s.tenant_id().value(), "tenant-acme");
  EXPECT_EQ(s.action_id().value(), "camera.disable");
}

TEST(IntentSessionTest, CreateRejectsSlotWithEmptyName) {
  std::vector<Slot> slots{{.name = "", .required = true, .value_json = std::nullopt}};
  const auto result = IntentSession::create(*SessionId::parse("s-1"), *OperatorId::parse("op-x"),
                                            *TenantId::parse("t-x"), *ActionId::parse("a.x"),
                                            std::move(slots), kT0);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.slot.empty_name");
}

// ---- all_required_slots_filled ----

TEST(IntentSessionTest, AllRequiredSlotsFilled_TrueWhenNoSlots) {
  const auto s = make_session({});
  EXPECT_TRUE(s.all_required_slots_filled());
}

TEST(IntentSessionTest, AllRequiredSlotsFilled_FalseWhenRequiredSlotEmpty) {
  const auto s = make_session({required_slot("cameraId")});
  EXPECT_FALSE(s.all_required_slots_filled());
}

TEST(IntentSessionTest, AllRequiredSlotsFilled_TrueWhenRequiredSlotFilled) {
  const auto s = make_session({required_slot("cameraId", R"("42")")});
  EXPECT_TRUE(s.all_required_slots_filled());
}

TEST(IntentSessionTest, AllRequiredSlotsFilled_TrueWhenOptionalSlotEmpty) {
  const auto s = make_session({required_slot("cameraId", R"("42")"), optional_slot("note")});
  EXPECT_TRUE(s.all_required_slots_filled());
}

// ---- fill_slot ----

TEST(IntentSessionTest, FillSlotUpdatesValue) {
  auto s = make_session({required_slot("cameraId")});
  ASSERT_TRUE(s.fill_slot("cameraId", R"("42")", kT1).has_value());
  EXPECT_EQ(*s.slots()[0].value_json, R"("42")");
  EXPECT_EQ(s.updated_at(), kT1);
}

TEST(IntentSessionTest, FillSlotRejectsUnknownSlot) {
  auto s = make_session({required_slot("cameraId")});
  const auto r = s.fill_slot("unknown", R"("x")", kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.slot.unknown");
}

TEST(IntentSessionTest, FillSlotRejectsEmptyValue) {
  auto s = make_session({required_slot("cameraId")});
  const auto r = s.fill_slot("cameraId", "", kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.slot.empty_value");
}

TEST(IntentSessionTest, FillSlotRejectsWhenNotInFillState) {
  auto s = make_session();
  ASSERT_TRUE(s.request_clarification("Which camera?", kT1).has_value());
  const auto r = s.fill_slot("x", "1", kT2);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.invalid");
}

// ---- fill -> clarification ----

TEST(IntentSessionTest, RequestClarificationTransitionsState) {
  auto s = make_session();
  ASSERT_TRUE(s.request_clarification("Which camera?", kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::clarification);
  EXPECT_EQ(s.clarification_question(), "Which camera?");
  EXPECT_EQ(s.updated_at(), kT1);
}

TEST(IntentSessionTest, RequestClarificationRejectsEmptyQuestion) {
  auto s = make_session();
  const auto r = s.request_clarification("", kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.clarification.empty_question");
}

TEST(IntentSessionTest, RequestClarificationRejectsFromNonFillState) {
  auto s = make_session();
  ASSERT_TRUE(s.request_clarification("Q?", kT1).has_value());
  // Already in clarification, cannot request again
  const auto r = s.request_clarification("Another Q?", kT2);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.invalid");
}

// ---- clarification -> fill ----

TEST(IntentSessionTest, AcceptClarificationReturnsToFill) {
  auto s = make_session();
  ASSERT_TRUE(s.request_clarification("Which camera?", kT1).has_value());
  ASSERT_TRUE(s.accept_clarification("Camera 5", kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::fill);
  EXPECT_EQ(s.clarification_answer(), "Camera 5");
  EXPECT_TRUE(s.clarification_question().empty());
  EXPECT_EQ(s.updated_at(), kT2);
}

TEST(IntentSessionTest, AcceptClarificationRejectsFromNonClarificationState) {
  auto s = make_session();
  const auto r = s.accept_clarification("answer", kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.invalid");
}

// ---- fill -> preview ----

TEST(IntentSessionTest, PreviewRequiresAllRequiredSlotsFilled) {
  auto s = make_session({required_slot("cameraId")});
  const auto r = s.preview(R"({"ok":true})", kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.preview.slots_incomplete");
}

TEST(IntentSessionTest, PreviewSucceedsWhenAllRequiredSlotsFilled) {
  auto s = make_session({required_slot("cameraId", R"("42")")});
  ASSERT_TRUE(s.preview(R"({"ok":true})", kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::preview);
  EXPECT_EQ(s.preview_result_json(), R"({"ok":true})");
}

TEST(IntentSessionTest, PreviewSucceedsWithNoSlots) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::preview);
}

// ---- preview -> fill ----

TEST(IntentSessionTest, RejectPreviewReturnsToFill) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.reject_preview(kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::fill);
  EXPECT_TRUE(s.preview_result_json().empty());
}

// ---- preview -> commit ----

TEST(IntentSessionTest, ConfirmTransitionsToCommit) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.confirm(kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::commit);
}

TEST(IntentSessionTest, ConfirmRejectsFromFillState) {
  auto s = make_session();
  const auto r = s.confirm(kT1);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.invalid");
}

// ---- commit -> archive ----

TEST(IntentSessionTest, CompleteTransitionsToArchive) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.confirm(kT2).has_value());
  ASSERT_TRUE(s.complete(R"({"status":"ok"})", kT3).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
  EXPECT_EQ(s.result_json(), R"({"status":"ok"})");
  ASSERT_TRUE(s.archive_reason().has_value());
  EXPECT_EQ(*s.archive_reason(), ArchiveReason::completed);
}

TEST(IntentSessionTest, CompleteRejectsFromPreviewState) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  const auto r = s.complete("result", kT2);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.invalid");
}

// ---- cancel from any state ----

TEST(IntentSessionTest, CancelFromFillState) {
  auto s = make_session();
  ASSERT_TRUE(s.cancel("operator abandoned", kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
  EXPECT_EQ(*s.archive_reason(), ArchiveReason::cancelled);
  EXPECT_EQ(s.archive_note(), "operator abandoned");
}

TEST(IntentSessionTest, CancelFromClarificationState) {
  auto s = make_session();
  ASSERT_TRUE(s.request_clarification("Q?", kT1).has_value());
  ASSERT_TRUE(s.cancel("timed out", kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
  EXPECT_EQ(*s.archive_reason(), ArchiveReason::cancelled);
}

TEST(IntentSessionTest, CancelFromPreviewState) {
  auto s = make_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.cancel("rejected by operator", kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
}

TEST(IntentSessionTest, CancelRejectsWhenAlreadyArchived) {
  auto s = make_session();
  ASSERT_TRUE(s.cancel("first", kT1).has_value());
  const auto r = s.cancel("second", kT2);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.already_archived");
}

// ---- expire ----

TEST(IntentSessionTest, ExpireTransitionsToArchive) {
  auto s = make_session();
  ASSERT_TRUE(s.expire(kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
  EXPECT_EQ(*s.archive_reason(), ArchiveReason::expired);
}

TEST(IntentSessionTest, ExpireRejectsWhenAlreadyArchived) {
  auto s = make_session();
  ASSERT_TRUE(s.expire(kT1).has_value());
  const auto r = s.expire(kT2);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_code(r.error()), "session.transition.already_archived");
}

// ---- is_expired / TTL ----

TEST(IntentSessionTest, IsExpiredReturnsFalseBeforeTtl) {
  auto s = make_session({}, std::chrono::seconds{60});
  EXPECT_FALSE(s.is_expired(kT0 + std::chrono::seconds{59}));
}

TEST(IntentSessionTest, IsExpiredReturnsTrueAtTtlBoundary) {
  auto s = make_session({}, std::chrono::seconds{60});
  EXPECT_TRUE(s.is_expired(kT0 + std::chrono::seconds{60}));
}

TEST(IntentSessionTest, IsExpiredReturnsTrueAfterTtl) {
  auto s = make_session({}, std::chrono::seconds{60});
  EXPECT_TRUE(s.is_expired(kT0 + std::chrono::seconds{120}));
}

// ---- is_terminal ----

TEST(IntentSessionTest, IsTerminalOnlyForArchiveState) {
  EXPECT_FALSE(is_terminal(SessionState::fill));
  EXPECT_FALSE(is_terminal(SessionState::clarification));
  EXPECT_FALSE(is_terminal(SessionState::preview));
  EXPECT_FALSE(is_terminal(SessionState::commit));
  EXPECT_TRUE(is_terminal(SessionState::archive));
}

// ---- Full happy path ----

TEST(IntentSessionTest, FullLifecycleWithClarificationAndPreview) {
  auto s = make_session({required_slot("cameraId")});

  ASSERT_TRUE(s.request_clarification("Which camera?", kT0).has_value());
  EXPECT_EQ(s.state(), SessionState::clarification);

  ASSERT_TRUE(s.accept_clarification("Camera 5", kT1).has_value());
  EXPECT_EQ(s.state(), SessionState::fill);

  ASSERT_TRUE(s.fill_slot("cameraId", R"("cam-5")", kT1).has_value());
  EXPECT_TRUE(s.all_required_slots_filled());

  ASSERT_TRUE(s.preview(R"({"affected":1})", kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::preview);

  ASSERT_TRUE(s.confirm(kT2).has_value());
  EXPECT_EQ(s.state(), SessionState::commit);

  ASSERT_TRUE(s.complete(R"({"status":"ok"})", kT3).has_value());
  EXPECT_EQ(s.state(), SessionState::archive);
  EXPECT_EQ(*s.archive_reason(), ArchiveReason::completed);
}

} // namespace
