#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "aetheris/domain/ports/embeddings_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Embeddings adapter that always returns a zero vector of fixed dimension.
 *
 * Used when no ML backend is configured. VectorKnowledgeSource over
 * NullEmbeddings degrades to arbitrary ordering (all cosine similarities
 * are equal) rather than failing.
 */
class NullEmbeddings final : public domain::EmbeddingsPort {
 public:
  explicit NullEmbeddings(std::size_t dimension = 4) noexcept : dimension_(dimension) {}

  [[nodiscard]] domain::Result<std::vector<float>>
  embed(std::string_view /*text*/) noexcept override {
    return std::vector<float>(dimension_, 0.0f);
  }

  [[nodiscard]] std::size_t dimension() const noexcept override {
    return dimension_;
  }

 private:
  std::size_t dimension_;
};

} // namespace aetheris::infrastructure
