#include "aetheris/infrastructure/audit_chain.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/sha256.hpp"

namespace aetheris::infrastructure {

namespace {

[[nodiscard]] std::string_view outcome_string(domain::OutcomeKind kind) noexcept {
  switch (kind) {
  case domain::OutcomeKind::approved:
    return "approved";
  case domain::OutcomeKind::rejected:
    return "rejected";
  case domain::OutcomeKind::timed_out:
    return "timed_out";
  case domain::OutcomeKind::cancelled:
    return "cancelled";
  }
  return "unknown";
}

[[nodiscard]] std::string format_timestamp(std::chrono::system_clock::time_point tp) noexcept {
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
  const auto us =
      std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() %
      1'000'000;
  return std::format("{}.{:06d}Z", secs, us);
}

} // namespace

std::string canonical_record_bytes(const domain::DecisionRecord& record) noexcept {
  // Fixed-order newline-delimited fields. No field may contain a newline -
  // identifiers and JSON payloads satisfy this because they use escaped newlines.
  return record.id.value() + '\n' + record.intent_id.value() + '\n' + record.action_id.value() +
         '\n' + record.operator_id.value() + '\n' + record.tenant_id.value() + '\n' +
         format_timestamp(record.timestamp) + '\n' + std::string{outcome_string(record.outcome)} +
         '\n' + record.parameters_json + '\n' + record.result_json + '\n' + record.redacted_fields;
}

domain::RecordHash compute_record_hash(const domain::DecisionRecord& record) noexcept {
  return sha256(canonical_record_bytes(record));
}

domain::ChainHash compute_chain_hash(const domain::RecordHash& record_hash,
                                     const domain::ChainHash& prev_chain_hash) noexcept {
  std::array<std::uint8_t, 64> combined{};
  static_assert(sizeof(combined) == record_hash.size() + prev_chain_hash.size());
  std::memcpy(combined.data(), record_hash.data(), record_hash.size());
  std::memcpy(combined.data() + record_hash.size(), prev_chain_hash.data(), prev_chain_hash.size());
  return sha256(std::span<const std::uint8_t>{combined.data(), combined.size()});
}

domain::Result<void> verify_audit_chain(std::span<const domain::AuditChainNode> nodes) noexcept {
  domain::ChainHash expected_prev = domain::chain_hash_genesis();

  for (const auto& node : nodes) {
    const auto expected_seq = static_cast<std::uint64_t>(&node - nodes.data());
    if (node.sequence_number != expected_seq) {
      return domain::fail(domain::make_internal_error(
          "audit.chain.sequence_mismatch",
          std::format("Expected sequence {} but found {}.", expected_seq, node.sequence_number)));
    }

    if (node.prev_chain_hash != expected_prev) {
      return domain::fail(domain::make_internal_error(
          "audit.chain.prev_hash_mismatch",
          std::format("Prev hash mismatch at sequence {}.", node.sequence_number)));
    }

    const auto computed_record_hash = compute_record_hash(node.record);
    if (node.record_hash != computed_record_hash) {
      return domain::fail(domain::make_internal_error(
          "audit.chain.record_hash_mismatch",
          std::format("Record hash mismatch at sequence {}.", node.sequence_number)));
    }

    const auto computed_chain_hash = compute_chain_hash(node.record_hash, node.prev_chain_hash);
    if (node.chain_hash != computed_chain_hash) {
      return domain::fail(domain::make_internal_error(
          "audit.chain.chain_hash_mismatch",
          std::format("Chain hash mismatch at sequence {}.", node.sequence_number)));
    }

    expected_prev = node.chain_hash;
  }

  return {};
}

domain::AuditChainNode AuditChainBuilder::append(domain::DecisionRecord record) noexcept {
  const auto record_hash = compute_record_hash(record);
  const auto chain_hash = compute_chain_hash(record_hash, prev_chain_hash_);

  domain::AuditChainNode node{
      .sequence_number = next_sequence_,
      .record = std::move(record),
      .record_hash = record_hash,
      .chain_hash = chain_hash,
      .prev_chain_hash = prev_chain_hash_,
  };

  prev_chain_hash_ = chain_hash;
  ++next_sequence_;
  return node;
}

} // namespace aetheris::infrastructure
