#include "aetheris/infrastructure/json_lines_exporter.hpp"

#include <array>
#include <ostream>
#include <string>

#include <nlohmann/json.hpp>

#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/sha256.hpp"

namespace aetheris::infrastructure {

using nlohmann::json;
using namespace domain;

namespace {

[[nodiscard]] std::string hash_to_hex(std::span<const std::uint8_t, 32> h) noexcept {
  return hex_encode(h);
}

[[nodiscard]] std::string outcome_to_string(OutcomeKind kind) noexcept {
  switch (kind) {
  case OutcomeKind::approved:
    return "approved";
  case OutcomeKind::rejected:
    return "rejected";
  case OutcomeKind::timed_out:
    return "timed_out";
  case OutcomeKind::cancelled:
    return "cancelled";
  }
  return "approved";
}

[[nodiscard]] Result<OutcomeKind> parse_outcome(const std::string& s) {
  if (s == "approved")
    return OutcomeKind::approved;
  if (s == "rejected")
    return OutcomeKind::rejected;
  if (s == "timed_out")
    return OutcomeKind::timed_out;
  if (s == "cancelled")
    return OutcomeKind::cancelled;
  return fail(make_input_error("audit.json.outcome.invalid", "Unknown outcome kind.",
                               {ErrorDetail{"value", s}}));
}

[[nodiscard]] Result<std::array<std::uint8_t, 32>> parse_hex_hash(const std::string& hex) {
  if (hex.size() != 64) {
    return fail(make_input_error("audit.json.hash.invalid_length",
                                 "Hash hex string must be 64 characters."));
  }
  std::array<std::uint8_t, 32> result{};
  for (std::size_t i = 0; i < 32; ++i) {
    const auto hi = hex[i * 2];
    const auto lo = hex[i * 2 + 1];
    auto from_hex = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      return -1;
    };
    const int hi_val = from_hex(hi);
    const int lo_val = from_hex(lo);
    if (hi_val < 0 || lo_val < 0) {
      return fail(make_input_error("audit.json.hash.invalid_char",
                                   "Hash hex string contains non-hex character."));
    }
    result[i] = static_cast<std::uint8_t>((hi_val << 4) | lo_val);
  }
  return result;
}

[[nodiscard]] std::int64_t timestamp_to_unix_us(std::chrono::system_clock::time_point tp) noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] std::chrono::system_clock::time_point unix_us_to_timestamp(std::int64_t us) noexcept {
  return std::chrono::system_clock::time_point{std::chrono::microseconds{us}};
}

} // namespace

std::string serialize_audit_node_json(const AuditChainNode& node) {
  json doc;
  doc["seq"] = node.sequence_number;
  doc["record"]["id"] = node.record.id.value();
  doc["record"]["intentId"] = node.record.intent_id.value();
  doc["record"]["actionId"] = node.record.action_id.value();
  doc["record"]["operatorId"] = node.record.operator_id.value();
  doc["record"]["tenantId"] = node.record.tenant_id.value();
  doc["record"]["timestampUs"] = timestamp_to_unix_us(node.record.timestamp);
  doc["record"]["outcome"] = outcome_to_string(node.record.outcome);
  doc["record"]["parametersJson"] = node.record.parameters_json;
  doc["record"]["resultJson"] = node.record.result_json;
  doc["record"]["redactedFields"] = node.record.redacted_fields;
  doc["recordHash"] = hash_to_hex(node.record_hash);
  doc["chainHash"] = hash_to_hex(node.chain_hash);
  doc["prevChainHash"] = hash_to_hex(node.prev_chain_hash);
  return doc.dump();
}

Result<AuditChainNode> parse_audit_node_json(std::string_view json_text) {
  json doc;
  try {
    doc = json::parse(json_text);
  } catch (const json::parse_error& err) {
    return fail(
        make_input_error("audit.json.parse_error", std::string{"JSON parse error: "} + err.what()));
  }

  auto require_str = [&](const json& obj, const char* key) -> Result<std::string> {
    if (!obj.contains(key) || !obj[key].is_string()) {
      return fail(make_input_error("audit.json.missing_field",
                                   std::string{"Missing string field: "} + key));
    }
    return obj[key].get<std::string>();
  };

  if (!doc.contains("record") || !doc["record"].is_object()) {
    return fail(make_input_error("audit.json.missing_field", "Missing 'record' object."));
  }
  const auto& rec = doc["record"];

  auto id_str = require_str(rec, "id");
  if (!id_str.has_value())
    return fail(id_str.error());
  auto intent_str = require_str(rec, "intentId");
  if (!intent_str.has_value())
    return fail(intent_str.error());
  auto action_str = require_str(rec, "actionId");
  if (!action_str.has_value())
    return fail(action_str.error());
  auto operator_str = require_str(rec, "operatorId");
  if (!operator_str.has_value())
    return fail(operator_str.error());
  auto tenant_str = require_str(rec, "tenantId");
  if (!tenant_str.has_value())
    return fail(tenant_str.error());
  auto outcome_str = require_str(rec, "outcome");
  if (!outcome_str.has_value())
    return fail(outcome_str.error());
  auto params_str = require_str(rec, "parametersJson");
  if (!params_str.has_value())
    return fail(params_str.error());
  auto result_str = require_str(rec, "resultJson");
  if (!result_str.has_value())
    return fail(result_str.error());
  auto redacted_str = require_str(rec, "redactedFields");
  if (!redacted_str.has_value())
    return fail(redacted_str.error());

  auto id = DecisionId::parse(*id_str);
  if (!id.has_value())
    return fail(id.error());
  auto intent_id = IntentId::parse(*intent_str);
  if (!intent_id.has_value())
    return fail(intent_id.error());
  auto action_id = ActionId::parse(*action_str);
  if (!action_id.has_value())
    return fail(action_id.error());
  auto operator_id = OperatorId::parse(*operator_str);
  if (!operator_id.has_value())
    return fail(operator_id.error());
  auto tenant_id = TenantId::parse(*tenant_str);
  if (!tenant_id.has_value())
    return fail(tenant_id.error());

  auto outcome = parse_outcome(*outcome_str);
  if (!outcome.has_value())
    return fail(outcome.error());

  if (!rec.contains("timestampUs") || !rec["timestampUs"].is_number_integer()) {
    return fail(make_input_error("audit.json.missing_field", "Missing 'timestampUs'."));
  }
  const auto timestamp = unix_us_to_timestamp(rec["timestampUs"].get<std::int64_t>());

  if (!doc.contains("seq") || !doc["seq"].is_number_unsigned()) {
    return fail(make_input_error("audit.json.missing_field", "Missing 'seq'."));
  }
  const auto seq = doc["seq"].get<std::uint64_t>();

  auto record_hash_str = require_str(doc, "recordHash");
  if (!record_hash_str.has_value())
    return fail(record_hash_str.error());
  auto record_hash = parse_hex_hash(*record_hash_str);
  if (!record_hash.has_value())
    return fail(record_hash.error());

  auto chain_hash_str = require_str(doc, "chainHash");
  if (!chain_hash_str.has_value())
    return fail(chain_hash_str.error());
  auto chain_hash = parse_hex_hash(*chain_hash_str);
  if (!chain_hash.has_value())
    return fail(chain_hash.error());

  auto prev_hash_str = require_str(doc, "prevChainHash");
  if (!prev_hash_str.has_value())
    return fail(prev_hash_str.error());
  auto prev_hash = parse_hex_hash(*prev_hash_str);
  if (!prev_hash.has_value())
    return fail(prev_hash.error());

  return AuditChainNode{
      .sequence_number = seq,
      .record =
          DecisionRecord{
              .id = std::move(*id),
              .intent_id = std::move(*intent_id),
              .action_id = std::move(*action_id),
              .operator_id = std::move(*operator_id),
              .tenant_id = std::move(*tenant_id),
              .timestamp = timestamp,
              .outcome = *outcome,
              .parameters_json = std::move(*params_str),
              .result_json = std::move(*result_str),
              .redacted_fields = std::move(*redacted_str),
          },
      .record_hash = *record_hash,
      .chain_hash = *chain_hash,
      .prev_chain_hash = *prev_hash,
  };
}

JsonLinesExporter::JsonLinesExporter(std::ostream& out) noexcept : out_{out} {}

Result<void> JsonLinesExporter::export_record(const AuditChainNode& node) {
  out_ << serialize_audit_node_json(node) << '\n';
  if (!out_.good()) {
    return fail(
        make_internal_error("audit.exporter.write_failed", "Stream write failed during export."));
  }
  return {};
}

Result<void> JsonLinesExporter::flush() {
  out_.flush();
  if (!out_.good()) {
    return fail(
        make_internal_error("audit.exporter.flush_failed", "Stream flush failed during export."));
  }
  return {};
}

} // namespace aetheris::infrastructure
