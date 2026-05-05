#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "aetheris/domain/identifier.hpp"

namespace aetheris::domain {

/**
 * Request sent to an LLM backend.
 */
struct LlmRequest final {
  std::string prompt;
  std::size_t max_tokens{512};
  float temperature{0.0f}; // 0.0 for deterministic classification
  std::string stop_sequence{};
};

/**
 * Raw response from an LLM backend.
 */
struct LlmResponse final {
  std::string text;                     // generated text
  std::optional<float> logprob_score{}; // token log-probability if available
  std::size_t tokens_generated{0};
};

/**
 * One candidate action produced by intent classification.
 */
struct IntentCandidate final {
  ActionId action_id;
  float confidence{0.0f};   // 0.0 (no confidence) .. 1.0 (certain)
  std::string slots_json{}; // JSON object of extracted parameter values
};

/**
 * Result of a successful intent classification pass.
 *
 * primary holds the highest-confidence candidate.
 * alternatives holds any additional candidates in descending confidence order.
 */
struct ClassifiedIntent final {
  IntentCandidate primary;
  std::vector<IntentCandidate> alternatives{};
  std::string raw_llm_response{}; // preserved for audit and debugging
};

} // namespace aetheris::domain
