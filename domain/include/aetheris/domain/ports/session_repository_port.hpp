#pragma once

#include <chrono>
#include <vector>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::domain {

/**
 * Storage port for IntentSession aggregates.
 *
 * All reads are scoped to (operator_id, tenant_id) - the implementation must
 * guarantee that sessions belonging to one operator/tenant cannot be retrieved
 * by another.  The session ID is globally unique; no two sessions in the same
 * repository may share an ID regardless of operator or tenant.
 */
class SessionRepositoryPort {
 public:
  SessionRepositoryPort() = default;
  SessionRepositoryPort(const SessionRepositoryPort&) = delete;
  SessionRepositoryPort& operator=(const SessionRepositoryPort&) = delete;
  SessionRepositoryPort(SessionRepositoryPort&&) = delete;
  SessionRepositoryPort& operator=(SessionRepositoryPort&&) = delete;
  virtual ~SessionRepositoryPort() = default;

  /**
   * Persists a session (insert or update).
   *
   * Returns InputError("session.repository.id_conflict") if a session with
   * the same ID already exists and belongs to a different operator or tenant.
   */
  [[nodiscard]] virtual Result<void> save(const IntentSession& session) = 0;

  /**
   * Loads a session by ID.
   *
   * Returns InputError("session.repository.not_found") if no session with
   * the given ID exists.
   */
  [[nodiscard]] virtual Result<IntentSession> load(const SessionId& id) const = 0;

  /**
   * Removes a session from the repository.
   *
   * Silently succeeds if the session does not exist.
   */
  [[nodiscard]] virtual Result<void> remove(const SessionId& id) = 0;

  /**
   * Returns IDs of all sessions whose TTL has elapsed relative to `now`.
   *
   * The caller is responsible for calling expire() on the returned sessions
   * and re-saving or removing them.
   */
  [[nodiscard]] virtual Result<std::vector<SessionId>>
  list_expired(std::chrono::system_clock::time_point now) const = 0;

  /**
   * Returns IDs of all non-archived sessions for the given operator and tenant.
   */
  [[nodiscard]] virtual Result<std::vector<SessionId>>
  list_active_for(const OperatorId& operator_id, const TenantId& tenant_id) const = 0;
};

} // namespace aetheris::domain
