#pragma once

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Port: durable, append-only storage for AuditChainNodes.
 *
 * Contract:
 *   - Writes are durable: once append() returns Ok, the node survives a crash.
 *   - Nodes are ordered by sequence_number; implementations must not reorder.
 *   - Duplicate sequence_numbers must be rejected with an InputError.
 */
class AuditSinkPort {
 public:
  AuditSinkPort() = default;
  AuditSinkPort(const AuditSinkPort&) = delete;
  AuditSinkPort& operator=(const AuditSinkPort&) = delete;
  AuditSinkPort(AuditSinkPort&&) = default;
  AuditSinkPort& operator=(AuditSinkPort&&) = default;
  virtual ~AuditSinkPort() = default;

  [[nodiscard]] virtual Result<void> append(const AuditChainNode& node) = 0;
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
};

} // namespace aetheris::domain
