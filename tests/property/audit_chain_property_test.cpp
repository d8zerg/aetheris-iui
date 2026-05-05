#include <chrono>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "aetheris/domain/audit.hpp"
#include "aetheris/infrastructure/audit_chain.hpp"
#include "aetheris/infrastructure/json_lines_exporter.hpp"
#include "aetheris/infrastructure/memory_audit_sink.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

[[nodiscard]] DecisionRecord make_record(int index, OutcomeKind outcome) {
  const auto id_str = "d-" + std::to_string(index);
  return DecisionRecord{
      .id = *DecisionId::parse(id_str),
      .intent_id = *IntentId::parse("intent-x"),
      .action_id = *ActionId::parse("camera.disable"),
      .operator_id = *OperatorId::parse("op-alice"),
      .tenant_id = *TenantId::parse("tenant-acme"),
      .timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{1000 + index}},
      .outcome = outcome,
      .parameters_json = R"({"cameraId":")" + std::to_string(index) + "\"}",
      .result_json = outcome == OutcomeKind::approved ? R"({"status":"ok"})" : "",
      .redacted_fields = "",
  };
}

// ---- Properties ----

[[nodiscard]] bool any_valid_sequence_forms_valid_chain() {
  return rc::check("any sequence of records forms a valid audit chain", [] {
    const auto count = *rc::gen::inRange(1, 20);
    const std::array<OutcomeKind, 4> outcomes{OutcomeKind::approved, OutcomeKind::rejected,
                                              OutcomeKind::timed_out, OutcomeKind::cancelled};

    AuditChainBuilder builder;
    MemoryAuditSink sink;

    for (int i = 0; i < count; ++i) {
      const auto idx = *rc::gen::inRange<int>(0, static_cast<int>(outcomes.size()));
      const auto node = builder.append(make_record(i, outcomes[static_cast<std::size_t>(idx)]));
      const auto ok = sink.append(node);
      RC_ASSERT(ok.has_value());
    }

    const auto verification = verify_audit_chain(sink.nodes());
    RC_ASSERT(verification.has_value());
  });
}

[[nodiscard]] bool any_tamper_is_detected() {
  return rc::check("tampering with any record is detected by the verifier", [] {
    const auto count = *rc::gen::inRange(1, 10);

    AuditChainBuilder builder;
    std::vector<AuditChainNode> chain;
    for (int i = 0; i < count; ++i) {
      chain.push_back(builder.append(make_record(i, OutcomeKind::approved)));
    }

    // Tamper one node's parameters_json
    const auto tamper_idx = static_cast<std::size_t>(*rc::gen::inRange(0, count));
    chain[tamper_idx].record.parameters_json = R"({"cameraId":"tampered"})";

    const auto verification = verify_audit_chain(chain);
    RC_ASSERT(!verification.has_value());
  });
}

[[nodiscard]] bool json_round_trip_preserves_chain_validity() {
  return rc::check("JSON round-trip of a chain preserves verifiability", [] {
    const auto count = *rc::gen::inRange(1, 8);

    AuditChainBuilder builder;
    std::vector<AuditChainNode> chain;
    for (int i = 0; i < count; ++i) {
      chain.push_back(builder.append(make_record(i, OutcomeKind::approved)));
    }

    // Serialize each node to JSON and parse it back
    std::vector<AuditChainNode> reparsed;
    reparsed.reserve(chain.size());
    for (const auto& node : chain) {
      auto parsed = parse_audit_node_json(serialize_audit_node_json(node));
      RC_ASSERT(parsed.has_value());
      reparsed.push_back(std::move(*parsed));
    }

    const auto verification = verify_audit_chain(reparsed);
    RC_ASSERT(verification.has_value());
  });
}

} // namespace

int main() {
  const bool chain_valid = any_valid_sequence_forms_valid_chain();
  const bool tamper_detected = any_tamper_is_detected();
  const bool round_trip = json_round_trip_preserves_chain_validity();
  return (chain_valid && tamper_detected && round_trip) ? 0 : 1;
}
