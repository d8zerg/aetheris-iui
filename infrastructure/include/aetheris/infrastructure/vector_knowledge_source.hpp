#pragma once

#include <span>
#include <string>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/embeddings_port.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"
#include "aetheris/domain/ports/vector_index_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * KnowledgeSource backed by EmbeddingsPort + VectorIndexPort.
 *
 * Embeds the query text and performs an ANN search, returning the top
 * results as KnowledgeEntries whose content is the stored payload_json.
 *
 * This source is optional - the KnowledgeOrchestrator degrades gracefully
 * if this source fails (e.g. embedding backend unavailable). The baseline
 * configuration runs without it; see ADR-0008.
 */
class VectorKnowledgeSource final : public domain::KnowledgeSourcePort {
 public:
  VectorKnowledgeSource(domain::EmbeddingsPort& embeddings, domain::VectorIndexPort& index) noexcept
      : embeddings_(embeddings), index_(index) {}

  [[nodiscard]] std::string name() const noexcept override {
    return "vector";
  }
  [[nodiscard]] int priority() const noexcept override {
    return 2;
  }

  [[nodiscard]] domain::Result<std::vector<domain::KnowledgeEntry>>
  retrieve(const domain::KnowledgeQuery& query) noexcept override {
    auto embed_result = embeddings_.embed(query.text);
    if (!embed_result.has_value()) {
      return domain::fail(std::move(embed_result.error()));
    }

    const auto& vec = *embed_result;
    auto search_result =
        index_.search(std::span<const float>{vec.data(), vec.size()}, query.max_results);
    if (!search_result.has_value()) {
      return domain::fail(std::move(search_result.error()));
    }

    std::vector<domain::KnowledgeEntry> entries;
    entries.reserve(search_result->size());
    for (auto& hit : *search_result) {
      entries.push_back(domain::KnowledgeEntry{
          .id = hit.id,
          .source_name = "vector",
          .content = std::move(hit.payload_json),
          .relevance_score = hit.score,
          .priority = 2,
          .tags = {},
      });
    }
    return entries;
  }

 private:
  domain::EmbeddingsPort& embeddings_;
  domain::VectorIndexPort& index_;
};

} // namespace aetheris::infrastructure
