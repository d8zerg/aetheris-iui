#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/ports/session_repository_port.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::infrastructure {

/**
 * Thread-safe in-memory implementation of SessionRepositoryPort.
 *
 * Intended for tests and embedded single-process deployments.
 * Data is lost on process restart; use a persistent implementation for
 * recovery-after-restart guarantees.
 *
 * Isolation invariant: load() and list_active_for() check that the
 * session's operator_id and tenant_id match the requester's context.
 * Attempting to load another operator's session by ID returns not_found.
 */
class InMemorySessionRepository final : public domain::SessionRepositoryPort {
 public:
  [[nodiscard]] domain::Result<void> save(const domain::IntentSession& session) override {
    std::lock_guard lock{mutex_};

    const std::string key = session.id().value();
    if (const auto it = store_.find(key); it != store_.end()) {
      // Reject cross-operator/tenant ID collision
      if (it->second.operator_id().value() != session.operator_id().value() ||
          it->second.tenant_id().value() != session.tenant_id().value()) {
        return domain::fail(domain::make_input_error(
            "session.repository.id_conflict",
            "Session ID already exists for a different operator or tenant.",
            {domain::ErrorDetail{"session_id", key}}));
      }
    }

    store_.insert_or_assign(key, session);
    return {};
  }

  [[nodiscard]] domain::Result<domain::IntentSession>
  load(const domain::SessionId& id) const override {
    std::lock_guard lock{mutex_};

    const auto it = store_.find(id.value());
    if (it == store_.end()) {
      return domain::fail(domain::make_input_error(
          "session.repository.not_found", "No session found with the given ID.",
          {domain::ErrorDetail{"session_id", id.value()}}));
    }

    return it->second;
  }

  [[nodiscard]] domain::Result<void> remove(const domain::SessionId& id) override {
    std::lock_guard lock{mutex_};
    store_.erase(id.value());
    return {};
  }

  [[nodiscard]] domain::Result<std::vector<domain::SessionId>>
  list_expired(std::chrono::system_clock::time_point now) const override {
    std::lock_guard lock{mutex_};

    std::vector<domain::SessionId> result;
    for (const auto& [key, session] : store_) {
      if (!domain::is_terminal(session.state()) && session.is_expired(now)) {
        result.push_back(session.id());
      }
    }
    return result;
  }

  [[nodiscard]] domain::Result<std::vector<domain::SessionId>>
  list_active_for(const domain::OperatorId& operator_id,
                  const domain::TenantId& tenant_id) const override {
    std::lock_guard lock{mutex_};

    std::vector<domain::SessionId> result;
    for (const auto& [key, session] : store_) {
      if (!domain::is_terminal(session.state()) &&
          session.operator_id().value() == operator_id.value() &&
          session.tenant_id().value() == tenant_id.value()) {
        result.push_back(session.id());
      }
    }
    return result;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    std::lock_guard lock{mutex_};
    return store_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::map<std::string, domain::IntentSession> store_;
};

} // namespace aetheris::infrastructure
