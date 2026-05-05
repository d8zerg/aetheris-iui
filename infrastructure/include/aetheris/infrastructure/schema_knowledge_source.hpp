#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/schema_registry.hpp"

namespace aetheris::infrastructure {

/**
 * KnowledgeSource backed by the ActionSchemaRegistry.
 *
 * If query.action_id is set, returns the latest schema for that action (score 1.0).
 * Otherwise, searches all registered schemas by case-insensitive substring match
 * against the action ID (score 0.9) or example intents (score 0.7).
 *
 * Content format: "action: {id}@{version}\nexamples: {intent1}; {intent2}"
 */
class SchemaKnowledgeSource final : public domain::KnowledgeSourcePort {
 public:
  explicit SchemaKnowledgeSource(const domain::ActionSchemaRegistry& registry) noexcept
      : registry_(registry) {}

  [[nodiscard]] std::string name() const noexcept override {
    return "schema";
  }
  [[nodiscard]] int priority() const noexcept override {
    return 0;
  }

  [[nodiscard]] domain::Result<std::vector<domain::KnowledgeEntry>>
  retrieve(const domain::KnowledgeQuery& query) noexcept override {
    if (query.action_id.has_value()) {
      const auto* schema = registry_.latest_for(*query.action_id);
      if (schema == nullptr) {
        return std::vector<domain::KnowledgeEntry>{};
      }
      return std::vector<domain::KnowledgeEntry>{make_entry(*schema, 1.0f)};
    }

    const std::string query_lower = to_lower(query.text);
    std::vector<domain::KnowledgeEntry> results;

    for (const auto& schema : registry_.all()) {
      const float score = match_score(schema, query_lower);
      if (score > 0.0f) {
        results.push_back(make_entry(schema, score));
        if (results.size() >= query.max_results) {
          break;
        }
      }
    }
    return results;
  }

 private:
  [[nodiscard]] static float match_score(const domain::ActionSchema& schema,
                                         const std::string& query_lower) noexcept {
    const std::string id_lower = to_lower(schema.action_id().value());
    if (id_lower.find(query_lower) != std::string::npos ||
        query_lower.find(id_lower) != std::string::npos) {
      return 0.9f;
    }
    for (const auto& ex : schema.examples().values()) {
      if (to_lower(ex.intent).find(query_lower) != std::string::npos) {
        return 0.7f;
      }
    }
    return 0.0f;
  }

  [[nodiscard]] static domain::KnowledgeEntry make_entry(const domain::ActionSchema& schema,
                                                         float score) noexcept {
    std::string content =
        "action: " + schema.action_id().value() + "@" + schema.version().value() + "\nexamples: ";
    bool first = true;
    for (const auto& ex : schema.examples().values()) {
      if (!first) {
        content += "; ";
      }
      content += ex.intent;
      first = false;
    }
    return domain::KnowledgeEntry{
        .id = schema.action_id().value() + "@" + schema.version().value(),
        .source_name = "schema",
        .content = std::move(content),
        .relevance_score = score,
        .priority = 0,
        .tags = {schema.action_id().value()},
    };
  }

  [[nodiscard]] static std::string to_lower(std::string_view text) {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
  }

  const domain::ActionSchemaRegistry& registry_;
};

} // namespace aetheris::infrastructure
