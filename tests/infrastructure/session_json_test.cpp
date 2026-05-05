#include <chrono>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"
#include "aetheris/infrastructure/session_json.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

const auto kT0 = std::chrono::system_clock::time_point{std::chrono::seconds{2000}};
const auto kT1 = std::chrono::system_clock::time_point{std::chrono::seconds{2001}};
const auto kT2 = std::chrono::system_clock::time_point{std::chrono::seconds{2002}};
const auto kT3 = std::chrono::system_clock::time_point{std::chrono::seconds{2003}};

[[nodiscard]] IntentSession make_fresh_session(std::vector<Slot> slots = {},
                                               std::string session_id = "sess-json") {
  return *IntentSession::create(*SessionId::parse(session_id), *OperatorId::parse("op-alice"),
                                *TenantId::parse("tenant-acme"), *ActionId::parse("camera.disable"),
                                std::move(slots), kT0);
}

TEST(SessionJsonTest, RoundTripFillState) {
  const auto original =
      make_fresh_session({Slot{.name = "cameraId", .required = true, .value_json = std::nullopt}});
  const auto json = serialize_session_json(original);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->id().value(), original.id().value());
  EXPECT_EQ(restored->operator_id().value(), original.operator_id().value());
  EXPECT_EQ(restored->tenant_id().value(), original.tenant_id().value());
  EXPECT_EQ(restored->action_id().value(), original.action_id().value());
  EXPECT_EQ(restored->state(), SessionState::fill);
  ASSERT_EQ(restored->slots().size(), 1U);
  EXPECT_EQ(restored->slots()[0].name, "cameraId");
  EXPECT_FALSE(restored->slots()[0].value_json.has_value());
}

TEST(SessionJsonTest, RoundTripClarificationState) {
  auto s = make_fresh_session();
  ASSERT_TRUE(s.request_clarification("Which camera?", kT1).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::clarification);
  EXPECT_EQ(restored->clarification_question(), "Which camera?");
}

TEST(SessionJsonTest, RoundTripPreviewState) {
  auto s =
      make_fresh_session({Slot{.name = "cameraId", .required = true, .value_json = R"("cam-5")"}});
  ASSERT_TRUE(s.preview(R"({"affected":1})", kT1).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::preview);
  EXPECT_EQ(restored->preview_result_json(), R"({"affected":1})");
  ASSERT_EQ(restored->slots().size(), 1U);
  EXPECT_TRUE(restored->slots()[0].value_json.has_value());
  EXPECT_EQ(*restored->slots()[0].value_json, R"("cam-5")");
}

TEST(SessionJsonTest, RoundTripCommitState) {
  auto s = make_fresh_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.confirm(kT2).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::commit);
}

TEST(SessionJsonTest, RoundTripArchiveCompleted) {
  auto s = make_fresh_session();
  ASSERT_TRUE(s.preview("", kT1).has_value());
  ASSERT_TRUE(s.confirm(kT2).has_value());
  ASSERT_TRUE(s.complete(R"({"status":"ok"})", kT3).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::archive);
  ASSERT_TRUE(restored->archive_reason().has_value());
  EXPECT_EQ(*restored->archive_reason(), ArchiveReason::completed);
  EXPECT_EQ(restored->result_json(), R"({"status":"ok"})");
}

TEST(SessionJsonTest, RoundTripArchiveCancelled) {
  auto s = make_fresh_session();
  ASSERT_TRUE(s.cancel("user bailed", kT1).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::archive);
  EXPECT_EQ(*restored->archive_reason(), ArchiveReason::cancelled);
  EXPECT_EQ(restored->archive_note(), "user bailed");
}

TEST(SessionJsonTest, RoundTripArchiveExpired) {
  auto s = make_fresh_session();
  ASSERT_TRUE(s.expire(kT1).has_value());

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state(), SessionState::archive);
  EXPECT_EQ(*restored->archive_reason(), ArchiveReason::expired);
}

TEST(SessionJsonTest, RoundTripPreservesSlotOptionalField) {
  const auto s = make_fresh_session({
      Slot{.name = "cameraId", .required = true, .value_json = R"("42")"},
      Slot{.name = "note", .required = false, .value_json = std::nullopt},
  });

  const auto json = serialize_session_json(s);
  const auto restored = parse_session_json(json);

  ASSERT_TRUE(restored.has_value());
  ASSERT_EQ(restored->slots().size(), 2U);
  EXPECT_TRUE(restored->slots()[0].required);
  EXPECT_FALSE(restored->slots()[1].required);
  EXPECT_TRUE(restored->slots()[0].value_json.has_value());
  EXPECT_FALSE(restored->slots()[1].value_json.has_value());
}

TEST(SessionJsonTest, RejectsMalformedJson) {
  const auto result = parse_session_json("{not json}");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.json.parse_error");
}

TEST(SessionJsonTest, RejectsMissingRequiredField) {
  const auto result = parse_session_json(R"({"id":"x"})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.json.missing_field");
}

TEST(SessionJsonTest, RejectsUnknownState) {
  const auto result = parse_session_json(R"({
    "id":"s","operator_id":"op","tenant_id":"t","action_id":"a",
    "state":"bogus","created_at_us":0,"updated_at_us":0,"ttl_seconds":3600
  })");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.json.invalid_state");
}

} // namespace
