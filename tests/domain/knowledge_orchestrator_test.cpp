#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/knowledge_orchestrator.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"

namespace {

using namespace aetheris::domain;

// Test double: returns a fixed list of entries for any query.
class StaticSource final : public KnowledgeSourcePort {
 public:
  StaticSource(std::string name, int priority, std::vector<KnowledgeEntry> entries)
      : name_(std::move(name)), priority_(priority), entries_(std::move(entries)) {}

  [[nodiscard]] std::string name() const noexcept override {
    return name_;
  }
  [[nodiscard]] int priority() const noexcept override {
    return priority_;
  }
  [[nodiscard]] Result<std::vector<KnowledgeEntry>>
  retrieve(const KnowledgeQuery& /*query*/) noexcept override {
    return entries_;
  }

 private:
  std::string name_;
  int priority_;
  std::vector<KnowledgeEntry> entries_;
};

// Test double: always returns a domain error.
class FailingSource final : public KnowledgeSourcePort {
 public:
  explicit FailingSource(std::string name = "failing", int priority = 0) noexcept
      : name_(std::move(name)), priority_(priority) {}
  [[nodiscard]] std::string name() const noexcept override {
    return name_;
  }
  [[nodiscard]] int priority() const noexcept override {
    return priority_;
  }
  [[nodiscard]] Result<std::vector<KnowledgeEntry>>
  retrieve(const KnowledgeQuery& /*query*/) noexcept override {
    return fail(make_internal_error("test.source_failed", "Simulated failure"));
  }

 private:
  std::string name_;
  int priority_;
};

KnowledgeEntry make_entry(std::string id, std::string source_name, float score, int priority = 0) {
  return KnowledgeEntry{
      .id = std::move(id),
      .source_name = std::move(source_name),
      .content = "content",
      .relevance_score = score,
      .priority = priority,
      .tags = {},
  };
}

TEST(KnowledgeOrchestratorTest, EmptySourceList_ReturnsEmptyVector) {
  KnowledgeOrchestrator orch({});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "query"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(KnowledgeOrchestratorTest, SingleSource_ReturnsItsResults) {
  StaticSource source{"src", 0, {make_entry("e1", "src", 1.0f)}};
  KnowledgeOrchestrator orch({&source});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "query"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].id, "e1");
}

TEST(KnowledgeOrchestratorTest, TwoSources_HigherPriorityFirst) {
  // Sources passed in wrong order - orchestrator must sort by priority().
  StaticSource low_src{"low", 10, {make_entry("low-e", "low", 1.0f, 10)}};
  StaticSource high_src{"high", 0, {make_entry("high-e", "high", 1.0f, 0)}};
  KnowledgeOrchestrator orch({&low_src, &high_src});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2U);
  EXPECT_EQ((*result)[0].source_name, "high");
  EXPECT_EQ((*result)[1].source_name, "low");
}

TEST(KnowledgeOrchestratorTest, FailingSource_OtherResultsStillReturned) {
  StaticSource good{"good", 0, {make_entry("g1", "good", 0.9f, 0)}};
  FailingSource bad{"bad", 1};
  KnowledgeOrchestrator orch({&good, &bad});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].id, "g1");
}

TEST(KnowledgeOrchestratorTest, AllSourcesFail_ReturnsEmptyNotError) {
  FailingSource a{"a", 0};
  FailingSource b{"b", 1};
  KnowledgeOrchestrator orch({&a, &b});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(KnowledgeOrchestratorTest, DuplicatesBySourceAndId_HigherRankedKept) {
  // Two sources return entries with same (source_name, id).
  KnowledgeEntry e1{
      .id = "dup", .source_name = "common", .content = "c", .relevance_score = 0.9f, .priority = 0};
  KnowledgeEntry e2{
      .id = "dup", .source_name = "common", .content = "c", .relevance_score = 0.5f, .priority = 1};
  StaticSource s1{"s1", 0, {e1}};
  StaticSource s2{"s2", 1, {e2}};
  KnowledgeOrchestrator orch({&s1, &s2});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 0.9f);
}

TEST(KnowledgeOrchestratorTest, MaxResultsTruncation) {
  std::vector<KnowledgeEntry> entries;
  for (int i = 0; i < 8; ++i) {
    entries.push_back(make_entry("e" + std::to_string(i), "src", 1.0f));
  }
  StaticSource source{"src", 0, entries};
  KnowledgeOrchestrator orch({&source});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q", .max_results = 3});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3U);
}

TEST(KnowledgeOrchestratorTest, SamePriority_SortedByRelevanceDescending) {
  StaticSource source{"src",
                      0,
                      {
                          make_entry("low", "src", 0.3f, 0),
                          make_entry("high", "src", 0.9f, 0),
                          make_entry("mid", "src", 0.6f, 0),
                      }};
  KnowledgeOrchestrator orch({&source});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 3U);
  EXPECT_FLOAT_EQ((*result)[0].relevance_score, 0.9f);
  EXPECT_FLOAT_EQ((*result)[1].relevance_score, 0.6f);
  EXPECT_FLOAT_EQ((*result)[2].relevance_score, 0.3f);
}

TEST(KnowledgeOrchestratorTest, SameIdDifferentSource_BothKept) {
  // (source_name, id) pair is the dedup key - different source_name is not a dup.
  KnowledgeEntry e_a = make_entry("shared-id", "source-a", 0.8f, 0);
  KnowledgeEntry e_b = make_entry("shared-id", "source-b", 0.6f, 1);
  StaticSource sa{"sa", 0, {e_a}};
  StaticSource sb{"sb", 1, {e_b}};
  KnowledgeOrchestrator orch({&sa, &sb});
  const auto result = orch.retrieve(KnowledgeQuery{.text = "q"});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2U);
}

} // namespace
