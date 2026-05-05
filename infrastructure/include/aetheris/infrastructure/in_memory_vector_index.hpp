#pragma once

#include <algorithm>
#include <cmath>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/ports/vector_index_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Thread-safe in-memory vector index using exact cosine similarity search.
 *
 * Suitable for tests and small corpora (< ~10k entries).
 * For production scale, replace with Qdrant, Milvus, or pgvector adapters.
 */
class InMemoryVectorIndex final : public domain::VectorIndexPort {
 public:
  explicit InMemoryVectorIndex(std::size_t dimension) noexcept : dimension_(dimension) {}

  [[nodiscard]] domain::Result<void> upsert(std::string id, std::vector<float> embedding,
                                            std::string payload_json) noexcept override {
    if (embedding.size() != dimension_) {
      return domain::fail(
          domain::make_input_error("vector_index.dimension_mismatch",
                                   "Embedding dimension does not match index dimension."));
    }
    std::lock_guard lock(mutex_);
    entries_[id] = Entry{std::move(embedding), std::move(payload_json)};
    return {};
  }

  [[nodiscard]] domain::Result<std::vector<domain::VectorSearchResult>>
  search(std::span<const float> query_vector, std::size_t top_k) noexcept override {
    if (query_vector.size() != dimension_) {
      return domain::fail(
          domain::make_input_error("vector_index.dimension_mismatch",
                                   "Query vector dimension does not match index dimension."));
    }
    std::lock_guard lock(mutex_);

    std::vector<domain::VectorSearchResult> results;
    results.reserve(entries_.size());
    for (const auto& [id, entry] : entries_) {
      results.push_back({.id = id,
                         .score = cosine_similarity(query_vector, entry.embedding),
                         .payload_json = entry.payload_json});
    }

    const std::size_t out_size = std::min(top_k, results.size());
    std::partial_sort(
        results.begin(), results.begin() + static_cast<std::ptrdiff_t>(out_size), results.end(),
        [](const domain::VectorSearchResult& a, const domain::VectorSearchResult& b) noexcept {
          return a.score > b.score;
        });
    results.resize(out_size);
    return results;
  }

  [[nodiscard]] domain::Result<void> remove(std::string_view id) noexcept override {
    std::lock_guard lock(mutex_);
    entries_.erase(std::string{id});
    return {};
  }

  [[nodiscard]] std::size_t size() const noexcept {
    std::lock_guard lock(mutex_);
    return entries_.size();
  }

 private:
  struct Entry {
    std::vector<float> embedding;
    std::string payload_json;
  };

  [[nodiscard]] static float cosine_similarity(std::span<const float> a,
                                               const std::vector<float>& b) noexcept {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
      dot += a[i] * b[i];
      norm_a += a[i] * a[i];
      norm_b += b[i] * b[i];
    }
    const float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    return denom > 0.0f ? dot / denom : 0.0f;
  }

  std::size_t dimension_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Entry> entries_;
};

} // namespace aetheris::infrastructure
