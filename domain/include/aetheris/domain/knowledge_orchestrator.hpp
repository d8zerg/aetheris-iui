#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Aggregates all registered KnowledgeSourcePort implementations.
 *
 * On each retrieve() call:
 *   1. Queries every registered source in priority order (ascending).
 *   2. Continues past per-source failures - graceful degradation.
 *   3. Stable-sorts merged results by (entry.priority ASC, relevance_score DESC).
 *   4. Deduplicates by (source_name, id), keeping the highest-ranked copy.
 *   5. Truncates to query.max_results.
 *
 * Returns an empty vector (not an error) when all sources fail - callers
 * receive enriched results when available and proceed without enrichment
 * when unavailable.
 */
class KnowledgeOrchestrator final {
 public:
  /**
   * Sources are not owned; callers must ensure they outlive this object.
   * The list is sorted by priority() on construction.
   */
  explicit KnowledgeOrchestrator(std::vector<KnowledgeSourcePort*> sources) noexcept
      : sources_(std::move(sources)) {
    std::stable_sort(sources_.begin(), sources_.end(),
                     [](const KnowledgeSourcePort* a, const KnowledgeSourcePort* b) noexcept {
                       return a->priority() < b->priority();
                     });
  }

  [[nodiscard]] Result<std::vector<KnowledgeEntry>> retrieve(const KnowledgeQuery& query) noexcept {
    std::vector<KnowledgeEntry> merged;

    for (KnowledgeSourcePort* source : sources_) {
      auto result = source->retrieve(query);
      if (!result.has_value()) {
        continue; // degrade: skip failing source
      }
      for (auto& entry : *result) {
        merged.push_back(std::move(entry));
      }
    }

    std::stable_sort(merged.begin(), merged.end(),
                     [](const KnowledgeEntry& a, const KnowledgeEntry& b) noexcept {
                       if (a.priority != b.priority) {
                         return a.priority < b.priority;
                       }
                       return a.relevance_score > b.relevance_score;
                     });

    std::unordered_set<std::string> seen;
    seen.reserve(merged.size());
    const auto erase_from =
        std::remove_if(merged.begin(), merged.end(), [&seen](const KnowledgeEntry& e) {
          return !seen.insert(e.source_name + '\0' + e.id).second;
        });
    merged.erase(erase_from, merged.end());

    if (merged.size() > query.max_results) {
      merged.resize(query.max_results);
    }

    return merged;
  }

 private:
  std::vector<KnowledgeSourcePort*> sources_;
};

} // namespace aetheris::domain
