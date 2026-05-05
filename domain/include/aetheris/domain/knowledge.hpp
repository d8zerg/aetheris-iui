#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "aetheris/domain/identifier.hpp"

namespace aetheris::domain {

/**
 * A single piece of contextual knowledge returned by a KnowledgeSource.
 *
 * Entries are ranked by the orchestrator using (priority, relevance_score):
 * lower priority value = higher importance; higher relevance_score = better match.
 */
struct KnowledgeEntry final {
  std::string id;              // stable identifier within the source
  std::string source_name;     // name of the producing KnowledgeSource
  std::string content;         // human-readable or structured knowledge text
  float relevance_score{1.0f}; // 0.0 (irrelevant) .. 1.0 (perfect match)
  int priority{0};             // source-assigned; lower = ranked first
  std::vector<std::string> tags;
};

/**
 * Query issued to one or more KnowledgeSources.
 */
struct KnowledgeQuery final {
  std::string text; // raw intent text or paraphrase to enrich

  // If set, sources may narrow results to this action.
  std::optional<ActionId> action_id{};

  // BCP-47 locale tag; sources use this for language-specific content.
  std::string locale{"en"};

  // Maximum total entries requested from each source.
  std::size_t max_results{10};
};

} // namespace aetheris::domain
