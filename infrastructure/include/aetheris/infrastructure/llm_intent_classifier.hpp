#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/intent_classifier_port.hpp"
#include "aetheris/domain/ports/llm_backend_port.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/schema_registry.hpp"

namespace aetheris::infrastructure {

struct LlmIntentClassifierConfig {
  float min_confidence{0.7f};
  float min_gap{0.2f};
  std::size_t max_tokens{256};
};

/**
 * IntentClassifierPort implementation using a two-level LLM generation strategy.
 *
 * Level 1: single generate() call produces action_id + confidence + initial slots
 *   as a JSON object: {"candidates":[{"action_id":"...","confidence":0.9,"slots":{...}}]}
 *
 * Level 2: second generate() call runs only when required slots are absent from
 *   the Level 1 response.  Even if Level 2 fails the call still succeeds with
 *   whatever slots were extracted at Level 1.
 *
 * Returns AmbiguityError when the primary candidate's confidence falls below
 * min_confidence, or when the gap to the second candidate is below min_gap.
 */
class LlmIntentClassifier final : public domain::IntentClassifierPort {
 public:
  using Config = LlmIntentClassifierConfig;

  LlmIntentClassifier(domain::LlmBackendPort& backend, const domain::ActionSchemaRegistry& registry,
                      Config config = Config{}) noexcept
      : backend_(backend), registry_(registry), config_(config) {}

  [[nodiscard]] domain::Result<domain::ClassifiedIntent>
  classify(const std::string& intent_text, const std::vector<domain::KnowledgeEntry>& context,
           const std::string& locale) noexcept override;

 private:
  domain::LlmBackendPort& backend_;
  const domain::ActionSchemaRegistry& registry_;
  Config config_;
};

} // namespace aetheris::infrastructure
