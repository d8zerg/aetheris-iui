#pragma once

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Port: streaming export of audit chain nodes to an external system.
 *
 * Contract:
 *   - export_record() is called once per node in sequence-number order.
 *   - flush() must be called after the last export_record() to guarantee
 *     all buffered data is committed to the destination.
 *   - Implementations may buffer internally; flush() drains the buffer.
 */
class AuditExporterPort {
 public:
  AuditExporterPort() = default;
  AuditExporterPort(const AuditExporterPort&) = delete;
  AuditExporterPort& operator=(const AuditExporterPort&) = delete;
  AuditExporterPort(AuditExporterPort&&) = default;
  AuditExporterPort& operator=(AuditExporterPort&&) = default;
  virtual ~AuditExporterPort() = default;

  [[nodiscard]] virtual Result<void> export_record(const AuditChainNode& node) = 0;
  [[nodiscard]] virtual Result<void> flush() = 0;
};

} // namespace aetheris::domain
