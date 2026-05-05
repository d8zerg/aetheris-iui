#pragma once

#include <string_view>

#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Abstract LLM text-generation backend.
 *
 * Implementations:
 *   - StubLlmBackend   (deterministic canned responses; tests and offline demos)
 *   - LlamaCppBackend  (in-process inference via llama.cpp C API)
 *   - OpenAiHttpBackend (OpenAI-compatible REST; Boost.Beast)
 *   - OnnxLlmBackend   (specialised models via ONNX Runtime)
 *
 * The IntentClassifier calls this port twice per user intent:
 *   1. Classification pass: intent text -> action_id + confidence
 *   2. Slot-filling pass: action schema + intent text -> parameter JSON
 *
 * Must not throw.  Returns InferenceError on transient backend failures so
 * the IntentEngine can apply its degradation policy without aborting.
 */
class LlmBackendPort {
 public:
  LlmBackendPort() = default;
  LlmBackendPort(const LlmBackendPort&) = delete;
  LlmBackendPort& operator=(const LlmBackendPort&) = delete;
  LlmBackendPort(LlmBackendPort&&) = delete;
  LlmBackendPort& operator=(LlmBackendPort&&) = delete;
  virtual ~LlmBackendPort() = default;

  /**
   * Generates text for the given request.
   * Returns InferenceError on failure (timeout, OOM, context overflow, etc.).
   * Must not throw.
   */
  [[nodiscard]] virtual Result<LlmResponse> generate(const LlmRequest& request) noexcept = 0;

  /**
   * Stable identifier for this backend; used in telemetry and audit.
   */
  [[nodiscard]] virtual std::string_view backend_name() const noexcept = 0;

  /**
   * Indicates whether the backend is ready to accept requests.
   * Callers may skip the backend and apply degradation when false.
   */
  [[nodiscard]] virtual bool is_available() const noexcept = 0;
};

} // namespace aetheris::domain
