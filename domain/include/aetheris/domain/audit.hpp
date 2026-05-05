#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

// ---- Hashing primitives --------------------------------------------------------

/** 32-byte cryptographic digest of a single record's canonical representation. */
using RecordHash = std::array<std::uint8_t, 32>;

/** 32-byte running chain digest: SHA-256(record_hash || prev_chain_hash). */
using ChainHash = std::array<std::uint8_t, 32>;

/** Returns the zero-value used as the sentinel prev_chain_hash for the first node. */
[[nodiscard]] constexpr ChainHash chain_hash_genesis() noexcept {
  ChainHash h{};
  return h;
}

// ---- Outcome -------------------------------------------------------------------

enum class OutcomeKind {
  approved,  ///< operator confirmed; action was executed
  rejected,  ///< operator explicitly declined
  timed_out, ///< confirmation window expired
  cancelled, ///< operator cancelled before the confirmation step
};

// ---- Redaction -----------------------------------------------------------------

enum class RedactionMode {
  mask,       ///< replace field value with "***"
  hash_value, ///< replace field value with its hex SHA-256
  remove,     ///< remove the field entirely from the JSON payload
};

struct RedactionRule final {
  std::string field_path; ///< JSON Pointer (RFC 6901), e.g. "/parameters/pin"
  RedactionMode mode;
};

/**
 * Describes which fields are redacted from a DecisionRecord payload and how.
 *
 * Redaction is applied before the record is serialized for hashing, so the
 * hash covers the already-redacted content. This means redacted records are
 * still tamper-evident but do not expose sensitive values.
 */
class RedactionPolicy final {
 public:
  [[nodiscard]] static Result<RedactionPolicy> create(std::vector<RedactionRule> rules) {
    if (std::any_of(rules.begin(), rules.end(),
                    [](const auto& rule) { return rule.field_path.empty(); })) {
      return fail(make_input_error("audit.redaction.empty_field_path",
                                   "Redaction rule field path must not be empty."));
    }
    return RedactionPolicy{std::move(rules)};
  }

  [[nodiscard]] static RedactionPolicy none() noexcept {
    return RedactionPolicy{{}};
  }

  [[nodiscard]] const std::vector<RedactionRule>& rules() const noexcept {
    return rules_;
  }
  [[nodiscard]] bool empty() const noexcept {
    return rules_.empty();
  }

 private:
  explicit RedactionPolicy(std::vector<RedactionRule> rules) : rules_{std::move(rules)} {}
  std::vector<RedactionRule> rules_;
};

// ---- DecisionRecord ------------------------------------------------------------

/**
 * The immutable record of one finalized operator decision.
 *
 * Invariant: created only once per intent; immutable after construction.
 * The record content is the input to the hash chain - any mutation would
 * invalidate the chain.
 */
struct DecisionRecord final {
  DecisionId id;
  IntentId intent_id;
  ActionId action_id;
  OperatorId operator_id;
  TenantId tenant_id;
  std::chrono::system_clock::time_point timestamp;
  OutcomeKind outcome;
  std::string parameters_json; ///< JSON string of submitted parameters (post-redaction)
  std::string result_json;     ///< JSON string of execution result (empty if not approved)
  std::string redacted_fields; ///< comma-separated field paths that were redacted, or empty
};

// ---- AuditChainNode ------------------------------------------------------------

/**
 * A node in the append-only tamper-evident audit chain.
 *
 * Invariants:
 *   record_hash  = SHA-256(canonical_bytes(record))
 *   chain_hash   = SHA-256(record_hash || prev_chain_hash)
 *   The first node has prev_chain_hash = chain_hash_genesis().
 *   Sequence numbers are consecutive starting from 0.
 */
struct AuditChainNode final {
  std::uint64_t sequence_number;
  DecisionRecord record;
  RecordHash record_hash;
  ChainHash chain_hash;
  ChainHash prev_chain_hash;
};

} // namespace aetheris::domain
