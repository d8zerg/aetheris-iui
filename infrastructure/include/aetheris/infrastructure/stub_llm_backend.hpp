#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/ports/llm_backend_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Deterministic LLM backend for tests and offline scenarios.
 *
 * Callers register a sequence of responses via push_response(). Each
 * generate() call returns the next response in the queue. When the queue
 * is exhausted, generate() returns an InferenceError unless a fallback
 * handler is set.
 *
 * This lets tests script exact LLM behaviour without any ML runtime.
 */
class StubLlmBackend final : public domain::LlmBackendPort {
 public:
  using Handler = std::function<domain::Result<domain::LlmResponse>(const domain::LlmRequest&)>;

  explicit StubLlmBackend(bool available = true) noexcept : available_(available) {}

  /**
   * Enqueues a canned text response.
   */
  void push_response(std::string text) {
    responses_.push_back(domain::LlmResponse{.text = std::move(text)});
  }

  /**
   * Enqueues a canned error response.
   */
  void push_error(std::string code, std::string message) {
    errors_.push_back({std::move(code), std::move(message), next_is_error_.size()});
    next_is_error_.push_back(true);
  }

  /**
   * Sets a fallback handler invoked when the response queue is exhausted.
   */
  void set_fallback(Handler handler) {
    fallback_ = std::move(handler);
  }

  [[nodiscard]] domain::Result<domain::LlmResponse>
  generate(const domain::LlmRequest& request) noexcept override {
    const std::size_t idx = call_count_++;
    if (idx < responses_.size()) {
      return responses_[idx];
    }
    const std::size_t err_idx = idx - responses_.size();
    if (err_idx < errors_.size()) {
      const auto& e = errors_[err_idx];
      return domain::fail(domain::make_inference_error(e.code, e.message));
    }
    if (fallback_) {
      return fallback_(request);
    }
    return domain::fail(domain::make_inference_error("stub.queue_exhausted",
                                                     "StubLlmBackend response queue exhausted."));
  }

  [[nodiscard]] std::string_view backend_name() const noexcept override {
    return "stub";
  }

  [[nodiscard]] bool is_available() const noexcept override {
    return available_;
  }

  [[nodiscard]] std::size_t call_count() const noexcept {
    return call_count_;
  }

 private:
  struct ErrorRecord {
    std::string code;
    std::string message;
    std::size_t at_index;
  };

  bool available_;
  std::size_t call_count_{0};
  std::vector<domain::LlmResponse> responses_;
  std::vector<ErrorRecord> errors_;
  std::vector<bool> next_is_error_;
  Handler fallback_;
};

} // namespace aetheris::infrastructure
