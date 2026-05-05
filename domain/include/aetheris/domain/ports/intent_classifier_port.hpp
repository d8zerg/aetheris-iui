#pragma once

#include <string>
#include <vector>

#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Abstract intent classifier that maps free-form operator text to a
 * ranked list of action candidates.
 *
 * The LlmIntentClassifier infrastructure adapter implements this port
 * using a two-level generation strategy: a classification pass to identify
 * the action and extract initial slots, followed by an optional slot-filling
 * pass when required parameters are missing.
 *
 * Returns AmbiguityError when the primary candidate's confidence is below
 * the configured threshold, or when the gap between the top two candidates
 * is too small to select a winner reliably.
 *
 * Returns InferenceError on backend failure, enabling degradation in the
 * calling IntentEngine.
 */
class IntentClassifierPort {
 public:
  IntentClassifierPort() = default;
  IntentClassifierPort(const IntentClassifierPort&) = delete;
  IntentClassifierPort& operator=(const IntentClassifierPort&) = delete;
  IntentClassifierPort(IntentClassifierPort&&) = delete;
  IntentClassifierPort& operator=(IntentClassifierPort&&) = delete;
  virtual ~IntentClassifierPort() = default;

  /**
   * Classifies the intent text given the enriched knowledge context.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<ClassifiedIntent>
  classify(const std::string& intent_text, const std::vector<KnowledgeEntry>& context,
           const std::string& locale) noexcept = 0;
};

} // namespace aetheris::domain
