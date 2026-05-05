#include <gtest/gtest.h>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"
#include "aetheris/infrastructure/in_memory_session_repository.hpp"
#include "support/session_repository_contracts.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

// ---- Contract suite ----

TEST(InMemorySessionRepositoryTest, PassesAllContractChecks) {
  InMemorySessionRepository repo;
  aetheris::tests::contracts::run_all_session_repository_contracts(repo);
}

// ---- Additional in-memory specific tests ----

TEST(InMemorySessionRepositoryTest, SizeReflectsSavedSessions) {
  InMemorySessionRepository repo;
  EXPECT_EQ(repo.size(), 0U);

  const auto s1 =
      *IntentSession::create(*SessionId::parse("s1"), *OperatorId::parse("op-a"),
                             *TenantId::parse("t-a"), *ActionId::parse("act.x"), {},
                             std::chrono::system_clock::time_point{std::chrono::seconds{0}});
  ASSERT_TRUE(repo.save(s1).has_value());
  EXPECT_EQ(repo.size(), 1U);

  ASSERT_TRUE(repo.remove(*SessionId::parse("s1")).has_value());
  EXPECT_EQ(repo.size(), 0U);
}

TEST(InMemorySessionRepositoryTest, RejectsCrossOwnerIdCollision) {
  InMemorySessionRepository repo;
  const auto kT = std::chrono::system_clock::time_point{std::chrono::seconds{0}};

  const auto s_alice =
      *IntentSession::create(*SessionId::parse("shared-id"), *OperatorId::parse("op-alice"),
                             *TenantId::parse("tenant-a"), *ActionId::parse("act.x"), {}, kT);
  ASSERT_TRUE(repo.save(s_alice).has_value());

  // Bob tries to save a session with the same ID
  const auto s_bob =
      *IntentSession::create(*SessionId::parse("shared-id"), *OperatorId::parse("op-bob"),
                             *TenantId::parse("tenant-b"), *ActionId::parse("act.y"), {}, kT);
  const auto result = repo.save(s_bob);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "session.repository.id_conflict");
}

// Tenant isolation: one tenant cannot see another's sessions via list_active_for
TEST(InMemorySessionRepositoryTest, TenantIsolationInListActiveFor) {
  InMemorySessionRepository repo;
  const auto kT = std::chrono::system_clock::time_point{std::chrono::seconds{0}};

  const auto s_a =
      *IntentSession::create(*SessionId::parse("iso-a"), *OperatorId::parse("op-x"),
                             *TenantId::parse("tenant-a"), *ActionId::parse("act.x"), {}, kT);
  const auto s_b =
      *IntentSession::create(*SessionId::parse("iso-b"), *OperatorId::parse("op-x"),
                             *TenantId::parse("tenant-b"), *ActionId::parse("act.x"), {}, kT);
  ASSERT_TRUE(repo.save(s_a).has_value());
  ASSERT_TRUE(repo.save(s_b).has_value());

  const auto result_a =
      repo.list_active_for(*OperatorId::parse("op-x"), *TenantId::parse("tenant-a"));
  ASSERT_TRUE(result_a.has_value());
  ASSERT_EQ(result_a->size(), 1U);
  EXPECT_EQ((*result_a)[0].value(), "iso-a");

  const auto result_b =
      repo.list_active_for(*OperatorId::parse("op-x"), *TenantId::parse("tenant-b"));
  ASSERT_TRUE(result_b.has_value());
  ASSERT_EQ(result_b->size(), 1U);
  EXPECT_EQ((*result_b)[0].value(), "iso-b");
}

} // namespace
