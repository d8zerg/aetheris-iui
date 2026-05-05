#pragma once

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/session_repository_port.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::tests::contracts {

using namespace domain;

const auto kRepoT0 = std::chrono::system_clock::time_point{std::chrono::seconds{5000}};
const auto kRepoT1 = std::chrono::system_clock::time_point{std::chrono::seconds{5001}};

[[nodiscard]] inline IntentSession
make_repo_session(std::string session_id, std::string operator_id = "op-alice",
                  std::string tenant_id = "tenant-acme",
                  std::chrono::seconds ttl = std::chrono::seconds{3600}) {
  return *IntentSession::create(*SessionId::parse(session_id), *OperatorId::parse(operator_id),
                                *TenantId::parse(tenant_id), *ActionId::parse("camera.disable"), {},
                                kRepoT0, ttl);
}

// Contract: save and load round-trips the session
inline void expect_save_and_load(SessionRepositoryPort& repo) {
  const auto session = make_repo_session("sess-rt");
  ASSERT_TRUE(repo.save(session).has_value());
  const auto loaded = repo.load(*SessionId::parse("sess-rt"));
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->id().value(), "sess-rt");
  EXPECT_EQ(loaded->operator_id().value(), "op-alice");
  EXPECT_EQ(loaded->state(), SessionState::fill);
}

// Contract: load returns not_found for unknown ID
inline void expect_load_not_found(SessionRepositoryPort& repo) {
  const auto result = repo.load(*SessionId::parse("nonexistent"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.repository.not_found");
}

// Contract: remove silently succeeds even if session doesn't exist
inline void expect_remove_idempotent(SessionRepositoryPort& repo) {
  EXPECT_TRUE(repo.remove(*SessionId::parse("ghost-sess")).has_value());
}

// Contract: remove makes the session unreachable
inline void expect_remove_makes_session_unreachable(SessionRepositoryPort& repo) {
  const auto session = make_repo_session("sess-rm");
  ASSERT_TRUE(repo.save(session).has_value());
  ASSERT_TRUE(repo.remove(*SessionId::parse("sess-rm")).has_value());
  const auto loaded = repo.load(*SessionId::parse("sess-rm"));
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(error_code(loaded.error()), "session.repository.not_found");
}

// Contract: list_expired returns sessions past TTL, not yet archived
inline void expect_list_expired(SessionRepositoryPort& repo) {
  auto short_ttl =
      make_repo_session("sess-exp", "op-alice", "tenant-acme", std::chrono::seconds{10});
  auto long_ttl =
      make_repo_session("sess-ok", "op-alice", "tenant-acme", std::chrono::seconds{3600});
  ASSERT_TRUE(repo.save(short_ttl).has_value());
  ASSERT_TRUE(repo.save(long_ttl).has_value());

  const auto expired_at = kRepoT0 + std::chrono::seconds{11};
  const auto expired = repo.list_expired(expired_at);
  ASSERT_TRUE(expired.has_value());
  ASSERT_EQ(expired->size(), 1U);
  EXPECT_EQ((*expired)[0].value(), "sess-exp");
}

// Contract: list_expired excludes already-archived sessions
inline void expect_list_expired_excludes_archived(SessionRepositoryPort& repo) {
  auto session = make_repo_session("sess-arch", "op-alice", "tenant-acme", std::chrono::seconds{1});
  ASSERT_TRUE(session.expire(kRepoT0).has_value());
  ASSERT_TRUE(repo.save(session).has_value());

  const auto expired_at = kRepoT0 + std::chrono::seconds{100};
  const auto expired = repo.list_expired(expired_at);
  ASSERT_TRUE(expired.has_value());
  for (const auto& id : *expired) {
    EXPECT_NE(id.value(), "sess-arch");
  }
}

// Contract: list_active_for returns sessions for the given operator+tenant
// Uses unique op/tenant IDs to avoid interference from other contract checks.
inline void expect_list_active_for_isolation(SessionRepositoryPort& repo) {
  const auto alice_session = make_repo_session("iso-sess-alice", "op-iso-alice", "tenant-iso");
  const auto bob_session = make_repo_session("iso-sess-bob", "op-iso-bob", "tenant-iso");
  const auto other_tenant = make_repo_session("iso-sess-other", "op-iso-alice", "tenant-other");
  ASSERT_TRUE(repo.save(alice_session).has_value());
  ASSERT_TRUE(repo.save(bob_session).has_value());
  ASSERT_TRUE(repo.save(other_tenant).has_value());

  const auto result =
      repo.list_active_for(*OperatorId::parse("op-iso-alice"), *TenantId::parse("tenant-iso"));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].value(), "iso-sess-alice");
}

// Contract: save updates an existing session
inline void expect_save_updates_existing(SessionRepositoryPort& repo) {
  auto session = make_repo_session("sess-upd");
  ASSERT_TRUE(repo.save(session).has_value());

  ASSERT_TRUE(session.cancel("changed mind", kRepoT1).has_value());
  ASSERT_TRUE(repo.save(session).has_value());

  const auto loaded = repo.load(*SessionId::parse("sess-upd"));
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->state(), SessionState::archive);
}

// Runs all contract tests against the given repository instance.
// Each check uses a unique session ID prefix to avoid collisions.
inline void run_all_session_repository_contracts(SessionRepositoryPort& repo) {
  expect_save_and_load(repo);
  expect_load_not_found(repo);
  expect_remove_idempotent(repo);
  expect_remove_makes_session_unreachable(repo);
  expect_list_expired(repo);
  expect_list_expired_excludes_archived(repo);
  expect_list_active_for_isolation(repo);
  expect_save_updates_existing(repo);
}

} // namespace aetheris::tests::contracts
