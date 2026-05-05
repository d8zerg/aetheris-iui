#include "aetheris/infrastructure/memory_audit_sink.hpp"

#include "aetheris/domain/error.hpp"

namespace aetheris::infrastructure {

domain::Result<void> MemoryAuditSink::append(const domain::AuditChainNode& node) {
  if (!nodes_.empty() && node.sequence_number <= nodes_.back().sequence_number) {
    return domain::fail(
        domain::make_input_error("audit.sink.duplicate_sequence",
                                 "AuditChainNode sequence number is not strictly increasing."));
  }
  nodes_.push_back(node);
  return {};
}

std::uint64_t MemoryAuditSink::size() const noexcept {
  return static_cast<std::uint64_t>(nodes_.size());
}

} // namespace aetheris::infrastructure
