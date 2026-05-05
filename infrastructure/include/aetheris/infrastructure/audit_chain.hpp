#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Returns the canonical byte representation of a DecisionRecord for hashing.
 *
 * The canonical form is a newline-delimited concatenation of deterministic
 * field values in a fixed order. Stable across platform and compiler versions.
 */
[[nodiscard]] std::string canonical_record_bytes(const domain::DecisionRecord& record) noexcept;

/**
 * Computes the RecordHash for a DecisionRecord.
 *
 * record_hash = SHA-256(canonical_record_bytes(record))
 */
[[nodiscard]] domain::RecordHash compute_record_hash(const domain::DecisionRecord& record) noexcept;

/**
 * Computes the ChainHash given a record hash and the previous chain hash.
 *
 * chain_hash = SHA-256(record_hash || prev_chain_hash)
 */
[[nodiscard]] domain::ChainHash
compute_chain_hash(const domain::RecordHash& record_hash,
                   const domain::ChainHash& prev_chain_hash) noexcept;

/**
 * Verifies that a chain of AuditChainNodes is internally consistent.
 *
 * Checks:
 *   - sequence numbers are consecutive starting from 0
 *   - record_hash of each node matches SHA-256(canonical_record_bytes(record))
 *   - chain_hash of each node matches SHA-256(record_hash || prev_chain_hash)
 *   - prev_chain_hash of node[i] equals chain_hash of node[i-1]
 *   - first node has prev_chain_hash == chain_hash_genesis()
 *
 * Returns Ok on a valid chain, or an InternalError describing the first violation.
 */
[[nodiscard]] domain::Result<void>
verify_audit_chain(std::span<const domain::AuditChainNode> nodes) noexcept;

/**
 * Stateful builder that appends DecisionRecords into a growing AuditChain.
 *
 * Each call to append() produces one AuditChainNode whose chain_hash
 * cryptographically binds it to all previous nodes.
 */
class AuditChainBuilder final {
 public:
  AuditChainBuilder() = default;

  /** Creates the next AuditChainNode from a DecisionRecord. */
  [[nodiscard]] domain::AuditChainNode append(domain::DecisionRecord record) noexcept;

  [[nodiscard]] const domain::ChainHash& current_chain_hash() const noexcept {
    return prev_chain_hash_;
  }

  [[nodiscard]] std::uint64_t next_sequence() const noexcept {
    return next_sequence_;
  }

 private:
  std::uint64_t next_sequence_{0};
  domain::ChainHash prev_chain_hash_{domain::chain_hash_genesis()};
};

} // namespace aetheris::infrastructure
