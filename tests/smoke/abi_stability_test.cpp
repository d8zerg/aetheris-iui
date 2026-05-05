#include <cstring>

#include <gtest/gtest.h>

#include "aetheris/application/runtime_descriptor.hpp"
#include "aetheris/interface/c_api.h"

// ── Stable ABI version ───────────────────────────────────────────────────────

TEST(AbiStabilityTest, VersionStringMatchesCompileTimeConstant) {
  EXPECT_STREQ(aetheris_version(), AETHERIS_IUI_VERSION);
}

TEST(AbiStabilityTest, AbiVersionMatchesRuntimeDescriptor) {
  EXPECT_EQ(aetheris_abi_version(), aetheris::application::kStableAbiVersion);
}

TEST(AbiStabilityTest, AbiVersionIsPositive) {
  EXPECT_GT(aetheris_abi_version(), 0u);
}

// ── Context lifecycle ─────────────────────────────────────────────────────────

TEST(AbiStabilityTest, CreateAndDestroyContext) {
  aetheris_context* ctx = nullptr;
  const auto status = aetheris_create_context(&ctx);
  EXPECT_EQ(status.code, AETHERIS_STATUS_OK);
  EXPECT_NE(ctx, nullptr);
  aetheris_destroy_context(ctx);
}

TEST(AbiStabilityTest, CreateContextRejectsNullOut) {
  const auto status = aetheris_create_context(nullptr);
  EXPECT_EQ(status.code, AETHERIS_STATUS_INVALID_ARGUMENT);
}

TEST(AbiStabilityTest, DestroyNullContextIsNoOp) {
  aetheris_destroy_context(nullptr); // must not crash
}

// ── aetheris_free_string ──────────────────────────────────────────────────────

TEST(AbiStabilityTest, FreeStringHandlesNull) {
  aetheris_free_string(nullptr); // must not crash
}

// ── aetheris_session_snapshot_json ────────────────────────────────────────────

static const char* kValidSession = R"({
  "id": "sess-abi-001",
  "action_id": "sensor.read",
  "operator_id": "op-1",
  "tenant_id": "tenant-1",
  "state": "fill",
  "confirmation_mode": "automatic",
  "slots": [],
  "clarification_question": null,
  "preview_result_json": null,
  "archive_reason": null,
  "created_at_us": 1000000,
  "updated_at_us": 1000000
})";

TEST(AbiStabilityTest, SessionSnapshotJsonRejectsNullContext) {
  char* out = nullptr;
  const auto status = aetheris_session_snapshot_json(nullptr, kValidSession, &out);
  EXPECT_EQ(status.code, AETHERIS_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(out, nullptr);
}

TEST(AbiStabilityTest, SessionSnapshotJsonRejectsNullInput) {
  aetheris_context* ctx = nullptr;
  ASSERT_EQ(aetheris_create_context(&ctx).code, AETHERIS_STATUS_OK);

  char* out = nullptr;
  const auto status = aetheris_session_snapshot_json(ctx, nullptr, &out);
  EXPECT_EQ(status.code, AETHERIS_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(out, nullptr);

  aetheris_destroy_context(ctx);
}

TEST(AbiStabilityTest, SessionSnapshotJsonRoundTrip) {
  aetheris_context* ctx = nullptr;
  ASSERT_EQ(aetheris_create_context(&ctx).code, AETHERIS_STATUS_OK);

  char* out = nullptr;
  const auto status = aetheris_session_snapshot_json(ctx, kValidSession, &out);
  EXPECT_EQ(status.code, AETHERIS_STATUS_OK) << status.message;
  ASSERT_NE(out, nullptr);
  EXPECT_NE(std::strlen(out), 0u);

  // Result must be valid JSON containing the session id
  EXPECT_NE(std::strstr(out, "sess-abi-001"), nullptr)
      << "id not found in snapshot output: " << out;

  aetheris_free_string(out);
  aetheris_destroy_context(ctx);
}

TEST(AbiStabilityTest, SessionSnapshotJsonRejectsMalformedJson) {
  aetheris_context* ctx = nullptr;
  ASSERT_EQ(aetheris_create_context(&ctx).code, AETHERIS_STATUS_OK);

  char* out = nullptr;
  const auto status = aetheris_session_snapshot_json(ctx, "{ not valid json }", &out);
  EXPECT_EQ(status.code, AETHERIS_STATUS_INTERNAL_ERROR);
  EXPECT_EQ(out, nullptr);

  aetheris_destroy_context(ctx);
}
