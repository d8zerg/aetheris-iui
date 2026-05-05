#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Converts raw text to a dense floating-point embedding vector.
 *
 * Implementations may wrap llama.cpp, ONNX Runtime, or a remote embedding
 * service. NullEmbeddings is provided for baseline and offline use.
 */
class EmbeddingsPort {
 public:
  EmbeddingsPort() = default;
  EmbeddingsPort(const EmbeddingsPort&) = delete;
  EmbeddingsPort& operator=(const EmbeddingsPort&) = delete;
  EmbeddingsPort(EmbeddingsPort&&) = delete;
  EmbeddingsPort& operator=(EmbeddingsPort&&) = delete;
  virtual ~EmbeddingsPort() = default;

  /**
   * Produces a dense vector for the given text.
   * The returned vector always has dimension() elements on success.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<std::vector<float>> embed(std::string_view text) noexcept = 0;

  /**
   * Fixed output dimension of this embedding model.
   */
  [[nodiscard]] virtual std::size_t dimension() const noexcept = 0;
};

} // namespace aetheris::domain
