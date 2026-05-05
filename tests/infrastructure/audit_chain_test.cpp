#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/infrastructure/audit_chain.hpp"
#include "aetheris/infrastructure/memory_audit_sink.hpp"
#include "aetheris/infrastructure/sha256.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

[[nodiscard]] DecisionRecord make_record(std::string id_val,
                                         OutcomeKind outcome = OutcomeKind::approved) {
  return DecisionRecord{
      .id = *DecisionId::parse(id_val),
      .intent_id = *IntentId::parse("intent-001"),
      .action_id = *ActionId::parse("camera.disable"),
      .operator_id = *OperatorId::parse("op-alice"),
      .tenant_id = *TenantId::parse("tenant-acme"),
      .timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{1000}},
      .outcome = outcome,
      .parameters_json = R"({"cameraId":"42"})",
      .result_json = outcome == OutcomeKind::approved ? R"({"status":"ok"})" : "",
      .redacted_fields = "",
  };
}

// ---- SHA-256 sanity check ----

TEST(Sha256Test, EmptyStringProducesKnownDigest) {
  const auto digest = sha256(std::string_view{""});

  // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb924...
  EXPECT_EQ(digest[0], 0xe3);
  EXPECT_EQ(digest[1], 0xb0);
  EXPECT_EQ(digest[2], 0xc4);
  EXPECT_EQ(digest[3], 0x42);
}

TEST(Sha256Test, ShortStringProducesKnownDigest) {
  // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469278f16e3b4e29c9e
  const auto digest = sha256(std::string_view{"abc"});
  EXPECT_EQ(digest[0], 0xba);
  EXPECT_EQ(digest[1], 0x78);
  EXPECT_EQ(digest[2], 0x16);
  EXPECT_EQ(digest[3], 0xbf);
}

TEST(Sha256Test, HexEncodeProducesLowerCaseHex) {
  const auto digest = sha256(std::string_view{""});
  const auto hex = hex_encode(digest);
  EXPECT_EQ(hex.size(), 64U);
  EXPECT_EQ(hex.substr(0, 8), "e3b0c442");
}

TEST(Sha256Test, SameInputProducesSameDigest) {
  const auto a = sha256(std::string_view{"hello"});
  const auto b = sha256(std::string_view{"hello"});
  EXPECT_EQ(a, b);
}

TEST(Sha256Test, DifferentInputsProduceDifferentDigests) {
  const auto a = sha256(std::string_view{"hello"});
  const auto b = sha256(std::string_view{"world"});
  EXPECT_NE(a, b);
}

// ---- AuditChainBuilder ----

TEST(AuditChainBuilderTest, FirstNodeHasGenesisAsPrevHash) {
  AuditChainBuilder builder;
  const auto node = builder.append(make_record("decision-001"));

  EXPECT_EQ(node.sequence_number, 0U);
  EXPECT_EQ(node.prev_chain_hash, chain_hash_genesis());
}

TEST(AuditChainBuilderTest, SecondNodePrevHashMatchesFirstChainHash) {
  AuditChainBuilder builder;
  const auto first = builder.append(make_record("decision-001"));
  const auto second = builder.append(make_record("decision-002"));

  EXPECT_EQ(second.sequence_number, 1U);
  EXPECT_EQ(second.prev_chain_hash, first.chain_hash);
}

TEST(AuditChainBuilderTest, ChainHashDependsOnRecord) {
  AuditChainBuilder builder1;
  AuditChainBuilder builder2;

  const auto node1 = builder1.append(make_record("decision-001"));
  const auto node2 = builder2.append(make_record("decision-002"));

  EXPECT_NE(node1.chain_hash, node2.chain_hash);
}

TEST(AuditChainBuilderTest, RecordHashMatchesCanonicalBytes) {
  AuditChainBuilder builder;
  const auto record = make_record("decision-001");
  const auto node = builder.append(record);

  const auto expected_hash = compute_record_hash(node.record);
  EXPECT_EQ(node.record_hash, expected_hash);
}

// ---- verify_audit_chain ----

TEST(VerifyAuditChainTest, EmptyChainIsValid) {
  const auto result = verify_audit_chain({});
  EXPECT_TRUE(result.has_value());
}

TEST(VerifyAuditChainTest, SingleNodeChainIsValid) {
  AuditChainBuilder builder;
  const auto node = builder.append(make_record("decision-001"));
  const std::vector<AuditChainNode> chain{node};

  const auto result = verify_audit_chain(chain);
  EXPECT_TRUE(result.has_value());
}

TEST(VerifyAuditChainTest, MultiNodeChainIsValid) {
  AuditChainBuilder builder;
  std::vector<AuditChainNode> chain;
  chain.push_back(builder.append(make_record("d-001")));
  chain.push_back(builder.append(make_record("d-002")));
  chain.push_back(builder.append(make_record("d-003")));

  const auto result = verify_audit_chain(chain);
  EXPECT_TRUE(result.has_value());
}

TEST(VerifyAuditChainTest, TamperedRecordHashDetected) {
  AuditChainBuilder builder;
  std::vector<AuditChainNode> chain;
  chain.push_back(builder.append(make_record("d-001")));

  // Tamper with the record content after building
  chain[0].record.parameters_json = R"({"cameraId":"99"})";

  const auto result = verify_audit_chain(chain);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "audit.chain.record_hash_mismatch");
}

TEST(VerifyAuditChainTest, TamperedChainHashDetected) {
  AuditChainBuilder builder;
  std::vector<AuditChainNode> chain;
  chain.push_back(builder.append(make_record("d-001")));
  chain.push_back(builder.append(make_record("d-002")));

  // Flip one byte in first node's chain_hash
  chain[0].chain_hash[0] ^= 0xFF;

  const auto result = verify_audit_chain(chain);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "audit.chain.chain_hash_mismatch");
}

TEST(VerifyAuditChainTest, OutOfOrderSequenceDetected) {
  AuditChainBuilder builder;
  std::vector<AuditChainNode> chain;
  chain.push_back(builder.append(make_record("d-001")));
  chain.push_back(builder.append(make_record("d-002")));

  // Swap sequence numbers to simulate out-of-order insertion
  std::swap(chain[0].sequence_number, chain[1].sequence_number);

  const auto result = verify_audit_chain(chain);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "audit.chain.sequence_mismatch");
}

// ---- MemoryAuditSink ----

TEST(MemoryAuditSinkTest, AcceptsSequentialNodes) {
  AuditChainBuilder builder;
  MemoryAuditSink sink;

  const auto node1 = builder.append(make_record("d-001"));
  const auto node2 = builder.append(make_record("d-002"));

  ASSERT_TRUE(sink.append(node1).has_value());
  ASSERT_TRUE(sink.append(node2).has_value());
  EXPECT_EQ(sink.size(), 2U);
}

TEST(MemoryAuditSinkTest, RejectsDuplicateSequenceNumber) {
  AuditChainBuilder builder;
  MemoryAuditSink sink;

  const auto node = builder.append(make_record("d-001"));
  ASSERT_TRUE(sink.append(node).has_value());

  const auto result = sink.append(node);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "audit.sink.duplicate_sequence");
}

TEST(MemoryAuditSinkTest, ChainVerifiesAfterSink) {
  AuditChainBuilder builder;
  MemoryAuditSink sink;

  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(sink.append(builder.append(make_record("d-" + std::to_string(i)))).has_value());
  }

  const auto result = verify_audit_chain(sink.nodes());
  EXPECT_TRUE(result.has_value());
}

} // namespace
