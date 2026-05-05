#pragma once

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * KnowledgeSource that serves multilingual glossary definitions.
 *
 * Terms are matched case-insensitively as substrings of the query text.
 * Locale selection per term:
 *   - Exact match (entry.locale == query.locale): score 1.0
 *   - "en" fallback when no exact locale entry exists: score 0.8
 *
 * Register entries with add_entry() before the source is queried.
 */
class GlossaryKnowledgeSource final : public domain::KnowledgeSourcePort {
 public:
  GlossaryKnowledgeSource() = default;

  void add_entry(std::string term, std::string locale, std::string definition) {
    entries_.push_back(GlossaryRecord{std::move(term), std::move(locale), std::move(definition)});
  }

  [[nodiscard]] std::string name() const noexcept override {
    return "glossary";
  }
  [[nodiscard]] int priority() const noexcept override {
    return 1;
  }

  [[nodiscard]] domain::Result<std::vector<domain::KnowledgeEntry>>
  retrieve(const domain::KnowledgeQuery& query) noexcept override {
    const std::string query_lower = to_lower(query.text);
    std::vector<domain::KnowledgeEntry> results;
    std::set<std::string> terms_matched; // lowercase terms with an exact locale result

    // First pass: exact locale match
    for (const auto& entry : entries_) {
      if (entry.locale != query.locale) {
        continue;
      }
      if (!term_in_query(query_lower, entry.term)) {
        continue;
      }
      results.push_back(make_entry(entry, 1.0f));
      terms_matched.insert(to_lower(entry.term));
    }

    // Second pass: "en" fallback for terms without an exact locale result
    if (query.locale != "en") {
      for (const auto& entry : entries_) {
        if (entry.locale != "en") {
          continue;
        }
        if (!term_in_query(query_lower, entry.term)) {
          continue;
        }
        if (terms_matched.count(to_lower(entry.term)) > 0) {
          continue;
        }
        results.push_back(make_entry(entry, 0.8f));
      }
    }

    if (results.size() > query.max_results) {
      results.resize(query.max_results);
    }
    return results;
  }

 private:
  struct GlossaryRecord {
    std::string term;
    std::string locale;
    std::string definition;
  };

  [[nodiscard]] static domain::KnowledgeEntry make_entry(const GlossaryRecord& record,
                                                         float score) noexcept {
    return domain::KnowledgeEntry{
        .id = record.term + "@" + record.locale,
        .source_name = "glossary",
        .content = record.term + ": " + record.definition,
        .relevance_score = score,
        .priority = 1,
        .tags = {record.term},
    };
  }

  [[nodiscard]] static bool term_in_query(const std::string& query_lower, const std::string& term) {
    return query_lower.find(to_lower(term)) != std::string::npos;
  }

  [[nodiscard]] static std::string to_lower(std::string_view text) {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
  }

  std::vector<GlossaryRecord> entries_;
};

} // namespace aetheris::infrastructure
