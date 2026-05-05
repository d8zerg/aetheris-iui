#include <chrono>
#include <sstream>

#include <gtest/gtest.h>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/audit_chain.hpp"
#include "aetheris/infrastructure/json_lines_exporter.hpp"
#include "aetheris/infrastructure/memory_audit_sink.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

[[nodiscard]] DecisionRecord make_record(std::string id_val) {
  return DecisionRecord{
      .id = *DecisionId::parse(id_val),
      .intent_id = *IntentId::parse("intent-x"),
      .action_id = *ActionId::parse("camera.disable"),
      .operator_id = *OperatorId::parse("op-alice"),
      .tenant_id = *TenantId::parse("tenant-acme"),
      .timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{42}},
      .outcome = OutcomeKind::approved,
      .parameters_json = R"({"cameraId":"1"}))",
      .result_json = R"({"status":"ok"})",
      .redacted_fields = "",
  };
}

TEST(JsonLinesExporterTest, SerializesNodeToValidJson) {
  AuditChainBuilder builder;
  const auto node = builder.append(make_record("decision-001"));

  std::ostringstream out;
  JsonLinesExporter exporter{out};
  ASSERT_TRUE(exporter.export_record(node).has_value());
  ASSERT_TRUE(exporter.flush().has_value());

  const auto line = out.str();
  EXPECT_FALSE(line.empty());
  EXPECT_EQ(line.back(), '\n');
}

TEST(JsonLinesExporterTest, RoundTripPreservesAllFields) {
  AuditChainBuilder builder;
  const auto original = builder.append(make_record("decision-42"));

  const auto json_str = serialize_audit_node_json(original);
  const auto parsed = parse_audit_node_json(json_str);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->sequence_number, original.sequence_number);
  EXPECT_EQ(parsed->record.id.value(), original.record.id.value());
  EXPECT_EQ(parsed->record.action_id.value(), original.record.action_id.value());
  EXPECT_EQ(parsed->record.outcome, original.record.outcome);
  EXPECT_EQ(parsed->record.parameters_json, original.record.parameters_json);
  EXPECT_EQ(parsed->record.result_json, original.record.result_json);
  EXPECT_EQ(parsed->record_hash, original.record_hash);
  EXPECT_EQ(parsed->chain_hash, original.chain_hash);
  EXPECT_EQ(parsed->prev_chain_hash, original.prev_chain_hash);
}

TEST(JsonLinesExporterTest, ParsedChainVerifiesCorrectly) {
  AuditChainBuilder builder;
  std::vector<AuditChainNode> parsed_nodes;

  for (int i = 0; i < 3; ++i) {
    const auto node = builder.append(make_record("d-" + std::to_string(i)));
    const auto json_str = serialize_audit_node_json(node);
    auto reparsed = parse_audit_node_json(json_str);
    ASSERT_TRUE(reparsed.has_value());
    parsed_nodes.push_back(std::move(*reparsed));
  }

  const auto result = verify_audit_chain(parsed_nodes);
  EXPECT_TRUE(result.has_value());
}

TEST(JsonLinesExporterTest, RejectsMalformedJson) {
  const auto result = parse_audit_node_json("{not valid");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "audit.json.parse_error");
}

TEST(JsonLinesExporterTest, RejectsMissingSeqField) {
  const auto result = parse_audit_node_json(R"({"record":{}})");
  ASSERT_FALSE(result.has_value());
}

} // namespace
