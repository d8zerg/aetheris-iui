#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * A single result from an approximate nearest-neighbour search.
 */
struct VectorSearchResult final {
  std::string id;
  float score{0.0f};        // similarity score: higher = better
  std::string payload_json; // arbitrary payload stored alongside the vector
};

/**
 * Abstract vector index for semantic similarity search.
 *
 * Implementations:
 *   - InMemoryVectorIndex (cosine similarity; tests and small corpora)
 *   - Qdrant / Milvus / pgvector adapters (future native client wrappers)
 *
 * The VectorKnowledgeSource uses this port together with EmbeddingsPort to
 * provide semantic retrieval to the KnowledgeOrchestrator.
 */
class VectorIndexPort {
 public:
  VectorIndexPort() = default;
  VectorIndexPort(const VectorIndexPort&) = delete;
  VectorIndexPort& operator=(const VectorIndexPort&) = delete;
  VectorIndexPort(VectorIndexPort&&) = delete;
  VectorIndexPort& operator=(VectorIndexPort&&) = delete;
  virtual ~VectorIndexPort() = default;

  /**
   * Inserts or replaces the entry identified by id.
   * embedding.size() must equal the index dimension.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<void> upsert(std::string id, std::vector<float> embedding,
                                            std::string payload_json) noexcept = 0;

  /**
   * Returns up to top_k results ordered by descending similarity score.
   * query_vector.size() must equal the index dimension.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<std::vector<VectorSearchResult>>
  search(std::span<const float> query_vector, std::size_t top_k) noexcept = 0;

  /**
   * Removes the entry with the given id. No-op if not present.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<void> remove(std::string_view id) noexcept = 0;
};

} // namespace aetheris::domain
