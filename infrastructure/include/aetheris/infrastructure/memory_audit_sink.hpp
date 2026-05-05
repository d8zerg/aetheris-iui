#pragma once

#include <cstdint>
#include <vector>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/ports/audit_sink_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * In-memory AuditSinkPort implementation for testing and bootstrap flows.
 *
 * Not durable: nodes are lost on destruction. Rejects duplicate sequence numbers.
 */
class MemoryAuditSink final : public domain::AuditSinkPort {
 public:
  [[nodiscard]] domain::Result<void> append(const domain::AuditChainNode& node) override;
  [[nodiscard]] std::uint64_t size() const noexcept override;

  [[nodiscard]] const std::vector<domain::AuditChainNode>& nodes() const noexcept {
    return nodes_;
  }

 private:
  std::vector<domain::AuditChainNode> nodes_;
};

} // namespace aetheris::infrastructure
