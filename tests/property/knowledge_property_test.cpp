#include <set>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/knowledge_orchestrator.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"

namespace {

using namespace aetheris::domain;

class StaticKnowledgeSource final : public KnowledgeSourcePort {
 public:
  StaticKnowledgeSource(std::string name, int priority, std::vector<KnowledgeEntry> entries)
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

/**
 * Property: result size never exceeds max_results regardless of source entry count.
 */
[[nodiscard]] bool result_length_never_exceeds_max_results() {
  return rc::check("result size never exceeds max_results", [] {
    const auto count = *rc::gen::inRange(0, 30);
    const auto max_res = *rc::gen::inRange(1, 20);

    std::vector<KnowledgeEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      const float score = static_cast<float>(*rc::gen::inRange(0, 100)) / 100.0f;
      const int prio = *rc::gen::inRange(0, 5);
      entries.push_back(KnowledgeEntry{
          .id = "e" + std::to_string(i),
          .source_name = "src",
          .content = "c",
          .relevance_score = score,
          .priority = prio,
          .tags = {},
      });
    }

    StaticKnowledgeSource source{"src", 0, entries};
    KnowledgeOrchestrator orch({&source});
    const auto result = orch.retrieve(
        KnowledgeQuery{.text = "q", .max_results = static_cast<std::size_t>(max_res)});
    RC_ASSERT(result.has_value());
    RC_ASSERT(result->size() <= static_cast<std::size_t>(max_res));
  });
}

/**
 * Property: no two entries in the result share the same (source_name, id) pair.
 */
[[nodiscard]] bool no_duplicate_source_and_id_pairs() {
  return rc::check("no duplicate (source_name, id) pairs in result", [] {
    const auto count = *rc::gen::inRange(0, 30);

    std::vector<KnowledgeEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      const int id_idx = *rc::gen::inRange(0, 8); // intentional id collisions
      entries.push_back(KnowledgeEntry{
          .id = "e" + std::to_string(id_idx),
          .source_name = "prop-src",
          .content = "c",
          .relevance_score = static_cast<float>(*rc::gen::inRange(0, 100)) / 100.0f,
          .priority = *rc::gen::inRange(0, 3),
          .tags = {},
      });
    }

    StaticKnowledgeSource source{"src", 0, entries};
    KnowledgeOrchestrator orch({&source});
    const auto result = orch.retrieve(KnowledgeQuery{.text = "q", .max_results = 100});
    RC_ASSERT(result.has_value());

    std::set<std::string> seen;
    for (const auto& e : *result) {
      const std::string key = e.source_name + '\0' + e.id;
      RC_ASSERT(seen.insert(key).second);
    }
  });
}

/**
 * Property: result entries are sorted by priority ASC then relevance_score DESC.
 */
[[nodiscard]] bool priority_ordering_maintained() {
  return rc::check("entries are sorted by priority ASC, then score DESC", [] {
    const auto n0 = *rc::gen::inRange(0, 10);
    const auto n5 = *rc::gen::inRange(0, 10);

    std::vector<KnowledgeEntry> entries;
    for (int i = 0; i < n0; ++i) {
      entries.push_back(KnowledgeEntry{
          .id = "prio0-" + std::to_string(i),
          .source_name = "src",
          .content = "c",
          .relevance_score = static_cast<float>(*rc::gen::inRange(0, 100)) / 100.0f,
          .priority = 0,
          .tags = {},
      });
    }
    for (int i = 0; i < n5; ++i) {
      entries.push_back(KnowledgeEntry{
          .id = "prio5-" + std::to_string(i),
          .source_name = "src",
          .content = "c",
          .relevance_score = static_cast<float>(*rc::gen::inRange(0, 100)) / 100.0f,
          .priority = 5,
          .tags = {},
      });
    }

    StaticKnowledgeSource source{"src", 0, entries};
    KnowledgeOrchestrator orch({&source});
    const auto result = orch.retrieve(KnowledgeQuery{.text = "q", .max_results = 100});
    RC_ASSERT(result.has_value());

    for (std::size_t i = 1; i < result->size(); ++i) {
      const auto& prev = (*result)[i - 1];
      const auto& curr = (*result)[i];
      RC_ASSERT(prev.priority <= curr.priority);
      if (prev.priority == curr.priority) {
        RC_ASSERT(prev.relevance_score >= curr.relevance_score);
      }
    }
  });
}

} // namespace

int main() {
  const bool p1 = result_length_never_exceeds_max_results();
  const bool p2 = no_duplicate_source_and_id_pairs();
  const bool p3 = priority_ordering_maintained();
  return (p1 && p2 && p3) ? 0 : 1;
}
