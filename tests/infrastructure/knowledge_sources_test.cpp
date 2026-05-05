#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "aetheris/infrastructure/glossary_knowledge_source.hpp"
#include "aetheris/infrastructure/in_memory_vector_index.hpp"
#include "aetheris/infrastructure/null_embeddings.hpp"
#include "aetheris/infrastructure/schema_knowledge_source.hpp"
#include "aetheris/infrastructure/vector_knowledge_source.hpp"
#include "support/knowledge_source_contracts.hpp"
#include "support/schema_test_helpers.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;
using namespace aetheris::tests;

// ---- SchemaKnowledgeSource ----

TEST(SchemaKnowledgeSourceTest, EmptyRegistry_ReturnsEmpty) {
  ActionSchemaRegistry reg;
  SchemaKnowledgeSource source{reg};
  const auto result = source.retrieve(KnowledgeQuery{.text = "anything"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(SchemaKnowledgeSourceTest, ActionIdQuery_ExistingAction_ReturnsEntry) {
  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_bounded_write_draft("camera.disable")))
                  .has_value());
  SchemaKnowledgeSource source{reg};

  const auto result =
      source.retrieve(KnowledgeQuery{.text = "", .action_id = *ActionId::parse("camera.disable")});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].id, "camera.disable@1.0.0");
  EXPECT_EQ((*result)[0].source_name, "schema");
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 1.0f);
  EXPECT_NE((*result)[0].content.find("camera.disable"), std::string::npos);
}

TEST(SchemaKnowledgeSourceTest, ActionIdQuery_UnknownAction_ReturnsEmpty) {
  ActionSchemaRegistry reg;
  SchemaKnowledgeSource source{reg};
  const auto result =
      source.retrieve(KnowledgeQuery{.text = "", .action_id = *ActionId::parse("unknown.action")});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(SchemaKnowledgeSourceTest, TextQuery_MatchesActionId_ReturnsEntry) {
  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_bounded_write_draft("camera.disable")))
                  .has_value());
  SchemaKnowledgeSource source{reg};

  const auto result = source.retrieve(KnowledgeQuery{.text = "camera"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].source_name, "schema");
}

TEST(SchemaKnowledgeSourceTest, TextQuery_MatchesExampleIntent_ReturnsEntry) {
  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_bounded_write_draft("camera.disable")))
                  .has_value());
  SchemaKnowledgeSource source{reg};

  // Example intent is "Disable camera 1"
  const auto result = source.retrieve(KnowledgeQuery{.text = "disable camera 1"});
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->empty());
}

TEST(SchemaKnowledgeSourceTest, TextQuery_NoMatch_ReturnsEmpty) {
  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_bounded_write_draft("camera.disable")))
                  .has_value());
  SchemaKnowledgeSource source{reg};

  const auto result = source.retrieve(KnowledgeQuery{.text = "temperature sensor reading"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(SchemaKnowledgeSourceTest, ContractSuite) {
  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_scoped_read_only_draft("sensor.read")))
                  .has_value());
  SchemaKnowledgeSource source{reg};
  contracts::run_all_knowledge_source_contracts(source);
}

// ---- GlossaryKnowledgeSource ----

TEST(GlossaryKnowledgeSourceTest, EmptyGlossary_ReturnsEmpty) {
  GlossaryKnowledgeSource source;
  const auto result = source.retrieve(KnowledgeQuery{.text = "anything"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(GlossaryKnowledgeSourceTest, TermInQuery_ExactLocale_ReturnsDefinition) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom camera control mechanism");
  const auto result =
      source.retrieve(KnowledgeQuery{.text = "configure PTZ settings", .locale = "en"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].source_name, "glossary");
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 1.0f);
  EXPECT_NE((*result)[0].content.find("Pan-tilt-zoom"), std::string::npos);
}

TEST(GlossaryKnowledgeSourceTest, TermNotInQuery_ReturnsEmpty) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom");
  const auto result = source.retrieve(KnowledgeQuery{.text = "list all cameras", .locale = "en"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(GlossaryKnowledgeSourceTest, CaseInsensitiveTermMatch) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom");
  const auto result =
      source.retrieve(KnowledgeQuery{.text = "configure ptz settings", .locale = "en"});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1U);
}

TEST(GlossaryKnowledgeSourceTest, FallbackToEn_WhenNoExactLocale) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom");
  // No "fr" entry exists for PTZ
  const auto result = source.retrieve(KnowledgeQuery{.text = "configurer PTZ", .locale = "fr"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 0.8f);
}

TEST(GlossaryKnowledgeSourceTest, ExactLocale_PrefersOverEnFallback) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom");
  source.add_entry("PTZ", "fr", "Panoramique-Inclinaison-Zoom");
  const auto result = source.retrieve(KnowledgeQuery{.text = "configurer PTZ", .locale = "fr"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_NE((*result)[0].content.find("Panoramique"), std::string::npos);
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 1.0f);
}

TEST(GlossaryKnowledgeSourceTest, MultipleTermsInQuery_AllMatched) {
  GlossaryKnowledgeSource source;
  source.add_entry("PTZ", "en", "Pan-tilt-zoom");
  source.add_entry("IR", "en", "Infrared");
  const auto result =
      source.retrieve(KnowledgeQuery{.text = "enable PTZ and IR mode", .locale = "en"});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2U);
}

TEST(GlossaryKnowledgeSourceTest, ContractSuite) {
  GlossaryKnowledgeSource source;
  source.add_entry("test", "en", "A test definition for contract checks");
  contracts::run_all_knowledge_source_contracts(source);
}

// ---- InMemoryVectorIndex ----

TEST(InMemoryVectorIndexTest, EmptyIndex_SearchReturnsEmpty) {
  InMemoryVectorIndex index{4};
  const std::vector<float> q{0.1f, 0.2f, 0.3f, 0.4f};
  const auto result = index.search(q, 5);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(InMemoryVectorIndexTest, UpsertThenSearch_ReturnsEntry) {
  InMemoryVectorIndex index{4};
  const std::vector<float> vec{1.0f, 0.0f, 0.0f, 0.0f};
  ASSERT_TRUE(index.upsert("id-1", vec, R"({"content":"hello"})").has_value());
  EXPECT_EQ(index.size(), 1U);
  const auto result = index.search(std::span<const float>{vec}, 10);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].id, "id-1");
  EXPECT_FLOAT_EQ((*result)[0].score, 1.0f);
}

TEST(InMemoryVectorIndexTest, Remove_EntryGone) {
  InMemoryVectorIndex index{2};
  ASSERT_TRUE(index.upsert("r-1", {1.0f, 0.0f}, "p").has_value());
  ASSERT_TRUE(index.remove("r-1").has_value());
  EXPECT_EQ(index.size(), 0U);
  const std::vector<float> q{1.0f, 0.0f};
  const auto result = index.search(q, 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(InMemoryVectorIndexTest, DimensionMismatch_UpsertReturnsError) {
  InMemoryVectorIndex index{4};
  const auto result = index.upsert("bad", {1.0f, 2.0f}, "p");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "vector_index.dimension_mismatch");
}

TEST(InMemoryVectorIndexTest, DimensionMismatch_SearchReturnsError) {
  InMemoryVectorIndex index{4};
  const std::vector<float> bad{1.0f, 2.0f};
  const auto result = index.search(bad, 5);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_code(result.error()), "vector_index.dimension_mismatch");
}

TEST(InMemoryVectorIndexTest, TopKRespected) {
  InMemoryVectorIndex index{1};
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(
        index.upsert("e" + std::to_string(i), {static_cast<float>(i + 1)}, "p").has_value());
  }
  const std::vector<float> q5{5.0f};
  const auto result = index.search(q5, 3);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3U);
}

TEST(InMemoryVectorIndexTest, SimilarVectors_RankedHigher) {
  InMemoryVectorIndex index{2};
  ASSERT_TRUE(index.upsert("exact", {1.0f, 0.0f}, "p").has_value());
  ASSERT_TRUE(index.upsert("orthogonal", {0.0f, 1.0f}, "p").has_value());
  const std::vector<float> q10{1.0f, 0.0f};
  const auto result = index.search(q10, 2);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2U);
  EXPECT_EQ((*result)[0].id, "exact");
  EXPECT_FLOAT_EQ((*result)[0].score, 1.0f);
}

// ---- NullEmbeddings ----

TEST(NullEmbeddingsTest, ReturnsZeroVectorOfCorrectDimension) {
  NullEmbeddings emb{8};
  EXPECT_EQ(emb.dimension(), 8U);
  const auto result = emb.embed("anything");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 8U);
  for (float v : *result) {
    EXPECT_FLOAT_EQ(v, 0.0f);
  }
}

TEST(NullEmbeddingsTest, DefaultDimension_FourElements) {
  NullEmbeddings emb;
  EXPECT_EQ(emb.dimension(), 4U);
  const auto r = emb.embed("x");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->size(), 4U);
}

// ---- VectorKnowledgeSource ----

TEST(VectorKnowledgeSourceTest, EmptyIndex_ReturnsEmpty) {
  NullEmbeddings emb{4};
  InMemoryVectorIndex index{4};
  VectorKnowledgeSource source{emb, index};
  const auto result = source.retrieve(KnowledgeQuery{.text = "test"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(VectorKnowledgeSourceTest, UpsertedEntries_ReturnedBySource) {
  NullEmbeddings emb{2};
  InMemoryVectorIndex index{2};
  ASSERT_TRUE(index.upsert("v1", {0.0f, 0.0f}, "payload-1").has_value());
  ASSERT_TRUE(index.upsert("v2", {0.0f, 0.0f}, "payload-2").has_value());
  VectorKnowledgeSource source{emb, index};
  const auto result = source.retrieve(KnowledgeQuery{.text = "test", .max_results = 5});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2U);
  EXPECT_EQ((*result)[0].source_name, "vector");
}

TEST(VectorKnowledgeSourceTest, ContentIsPayload) {
  NullEmbeddings emb{2};
  InMemoryVectorIndex index{2};
  ASSERT_TRUE(index.upsert("doc-1", {0.0f, 0.0f}, R"({"text":"hello world"})").has_value());
  VectorKnowledgeSource source{emb, index};
  const auto result = source.retrieve(KnowledgeQuery{.text = "hello"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].content, R"({"text":"hello world"})");
  EXPECT_EQ((*result)[0].id, "doc-1");
}

TEST(VectorKnowledgeSourceTest, ContractSuite) {
  NullEmbeddings emb{4};
  InMemoryVectorIndex index{4};
  VectorKnowledgeSource source{emb, index};
  contracts::run_all_knowledge_source_contracts(source);
}

// ---- Degradation: vector source fails, schema source still works ----

TEST(KnowledgeDegradationTest, VectorSourceFails_SchemaResultsReturned) {
  // Simulate degradation: schema source is available, vector source has
  // a dimension mismatch that would cause embedding to fail gracefully.
  // We use a VectorIndexPort with wrong dimension to trigger the error
  // inside VectorKnowledgeSource, and verify the orchestrator still
  // returns results from SchemaKnowledgeSource.

  ActionSchemaRegistry reg;
  ASSERT_TRUE(reg.register_schema(
                     *ActionSchema::create(helpers::make_bounded_write_draft("camera.disable")))
                  .has_value());

  // Correct dim for null embeddings is 4, but index has dim=8 -> search error
  NullEmbeddings emb{4};
  InMemoryVectorIndex bad_index{8}; // mismatched dimension
  SchemaKnowledgeSource schema_src{reg};
  VectorKnowledgeSource vector_src{emb, bad_index};

  // Register both; orchestrator must degrade past vector failure
  // Without a full KnowledgeOrchestrator, test directly:
  const auto vec_result = vector_src.retrieve(KnowledgeQuery{.text = "camera"});
  EXPECT_FALSE(vec_result.has_value()); // vector source fails

  const auto schema_result = schema_src.retrieve(KnowledgeQuery{.text = "camera"});
  EXPECT_TRUE(schema_result.has_value()); // schema source still works
  EXPECT_FALSE(schema_result->empty());
}

} // namespace
