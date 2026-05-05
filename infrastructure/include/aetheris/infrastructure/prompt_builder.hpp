#pragma once

#include <span>
#include <string>
#include <vector>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/knowledge.hpp"

namespace aetheris::infrastructure {

/**
 * Builds deterministic LLM prompts for the two-level intent classification pipeline.
 *
 * Level 1: classification - maps free-form user text to action + initial slot values.
 * Level 2: slot-filling - resolves missing required parameters for the resolved action.
 *
 * All methods are pure static and produce no side effects.
 */
class PromptBuilder {
 public:
  /**
   * Builds the Level 1 classification prompt.
   *
   * Instructs the LLM to return exactly:
   *   {"candidates":[{"action_id":"<id>","confidence":<0.0-1.0>,"slots":{...}},...]}
   */
  [[nodiscard]] static std::string
  build_classification_prompt(std::span<const domain::ActionSchema> schemas,
                              const std::vector<domain::KnowledgeEntry>& context,
                              const std::string& intent_text, const std::string& locale) {
    std::string p;
    p += "You are an intent classifier for an operator control interface.\n";
    p += "Classify the user intent into one of the available actions.\n";
    p += "Respond with a JSON object in exactly this format and nothing else:\n";
    p +=
        R"({"candidates":[{"action_id":"<id>","confidence":<0.0-1.0>,"slots":{"<param>":"<value>"}}]})";
    p += "\nList up to 3 candidates sorted by confidence descending.\n";
    p += "Include only parameter slots that can be inferred from the user's text.\n\n";
    p += "Locale: ";
    p += locale;
    p += "\n\n";

    if (!schemas.empty()) {
      p += "Available actions:\n";
      for (const auto& schema : schemas) {
        p += "- action_id: ";
        p += schema.action_id().value();
        p += "\n  parameters: ";
        p += schema.parameters().json_schema();
        p += "\n  examples:\n";
        for (const auto& ex : schema.examples().values()) {
          p += "    \"";
          p += ex.intent;
          p += "\"\n";
        }
      }
      p += "\n";
    }

    if (!context.empty()) {
      p += "Context:\n";
      for (const auto& entry : context) {
        p += "- ";
        p += entry.content;
        p += "\n";
      }
      p += "\n";
    }

    p += "User intent: \"";
    p += intent_text;
    p += "\"\n";
    return p;
  }

  /**
   * Builds the Level 2 slot-filling prompt.
   *
   * Instructs the LLM to return a JSON object containing ALL extracted slot
   * values (previously extracted plus newly inferred), keyed by parameter name.
   */
  [[nodiscard]] static std::string build_slot_fill_prompt(const domain::ActionSchema& schema,
                                                          const std::string& intent_text,
                                                          std::string_view current_slots_json) {
    std::string p;
    p += "You are a parameter extractor for an operator control interface.\n";
    p += "Extract the missing required parameters from the user's intent.\n";
    p += "Respond with a JSON object mapping parameter names to their values.\n";
    p += "Include ALL previously extracted slots plus any newly inferred ones.\n\n";
    p += "Action: ";
    p += schema.action_id().value();
    p += "\nParameter schema:\n";
    p += schema.parameters().json_schema();
    p += "\n\nUser intent: \"";
    p += intent_text;
    p += "\"\nCurrent slots: ";
    p += current_slots_json;
    p += "\n";
    return p;
  }
};

} // namespace aetheris::infrastructure
