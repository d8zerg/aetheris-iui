#include "aetheris/infrastructure/llm_intent_classifier.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/intent.hpp"
#include "aetheris/infrastructure/prompt_builder.hpp"

namespace aetheris::infrastructure {

using nlohmann::json;
using namespace domain;

namespace {

[[nodiscard]] Result<ClassifiedIntent> parse_classification_response(const std::string& raw) {
  json doc;
  try {
    doc = json::parse(raw);
  } catch (const json::parse_error&) {
    return fail(make_inference_error("classifier.response.parse_error",
                                     "LLM response is not valid JSON.",
                                     {ErrorDetail{"raw", raw.substr(0, 200)}}));
  }

  if (!doc.is_object() || !doc.contains("candidates") || !doc["candidates"].is_array()) {
    return fail(make_inference_error("classifier.response.missing_candidates",
                                     "LLM response does not contain a 'candidates' array.",
                                     {ErrorDetail{"raw", raw.substr(0, 200)}}));
  }

  const auto& cands = doc["candidates"];
  if (cands.empty()) {
    return fail(make_inference_error("classifier.response.empty_candidates",
                                     "LLM response contains an empty candidates list."));
  }

  std::vector<IntentCandidate> candidates;
  candidates.reserve(cands.size());

  for (const auto& cj : cands) {
    if (!cj.is_object() || !cj.contains("action_id") || !cj.contains("confidence")) {
      return fail(make_inference_error(
          "classifier.response.invalid_candidate",
          "Candidate entry is missing required fields (action_id, confidence)."));
    }
    if (!cj["action_id"].is_string() || !cj["confidence"].is_number()) {
      return fail(make_inference_error(
          "classifier.response.invalid_candidate",
          "Candidate action_id must be a string and confidence must be a number."));
    }

    auto action_id = ActionId::parse(cj["action_id"].get<std::string>());
    if (!action_id.has_value()) {
      return fail(make_inference_error("classifier.response.invalid_action_id",
                                       "Candidate action_id is not a valid identifier.",
                                       {ErrorDetail{"value", cj["action_id"].get<std::string>()}}));
    }

    const float confidence = cj["confidence"].get<float>();

    std::string slots_json = "{}";
    if (cj.contains("slots") && cj["slots"].is_object()) {
      slots_json = cj["slots"].dump();
    }

    candidates.push_back(IntentCandidate{
        .action_id = std::move(*action_id),
        .confidence = confidence,
        .slots_json = std::move(slots_json),
    });
  }

  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const IntentCandidate& a, const IntentCandidate& b) noexcept {
                     return a.confidence > b.confidence;
                   });

  IntentCandidate primary = std::move(candidates.front());
  candidates.erase(candidates.begin());

  return ClassifiedIntent{
      .primary = std::move(primary),
      .alternatives = std::move(candidates),
      .raw_llm_response = raw,
  };
}

[[nodiscard]] bool has_missing_required_slots(const ActionSchema& schema,
                                              std::string_view slots_json) {
  json param_schema;
  try {
    param_schema = json::parse(schema.parameters().json_schema());
  } catch (...) {
    return false;
  }

  if (!param_schema.contains("required") || !param_schema["required"].is_array()) {
    return false;
  }

  json slots;
  try {
    slots = json::parse(slots_json);
  } catch (...) {
    slots = json::object();
  }
  if (!slots.is_object()) {
    slots = json::object();
  }

  for (const auto& req : param_schema["required"]) {
    if (!req.is_string()) {
      continue;
    }
    const std::string key = req.get<std::string>();
    if (!slots.contains(key) || slots[key].is_null()) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string merge_slots(std::string_view current_json, const std::string& new_json) {
  json current;
  try {
    current = json::parse(current_json);
  } catch (...) {
    current = json::object();
  }
  if (!current.is_object()) {
    current = json::object();
  }

  json updated;
  try {
    updated = json::parse(new_json);
  } catch (...) {
    return std::string{current_json};
  }
  if (!updated.is_object()) {
    return std::string{current_json};
  }

  for (auto& [key, val] : updated.items()) {
    current[key] = val;
  }
  return current.dump();
}

} // namespace

Result<ClassifiedIntent> LlmIntentClassifier::classify(const std::string& intent_text,
                                                       const std::vector<KnowledgeEntry>& context,
                                                       const std::string& locale) noexcept {
  try {
    const auto& schemas = registry_.all();

    std::string prompt = PromptBuilder::build_classification_prompt(
        std::span<const ActionSchema>{schemas.data(), schemas.size()}, context, intent_text,
        locale);

    auto response = backend_.generate(LlmRequest{
        .prompt = std::move(prompt),
        .max_tokens = config_.max_tokens,
        .temperature = 0.0f,
    });
    if (!response.has_value()) {
      return fail(response.error());
    }

    auto classified = parse_classification_response(response->text);
    if (!classified.has_value()) {
      return fail(classified.error());
    }

    const float top_conf = classified->primary.confidence;
    if (top_conf < config_.min_confidence) {
      return fail(
          make_ambiguity_error("classifier.ambiguity.low_confidence",
                               "Primary candidate confidence is below the minimum threshold.",
                               {ErrorDetail{"confidence", std::to_string(top_conf)},
                                ErrorDetail{"threshold", std::to_string(config_.min_confidence)},
                                ErrorDetail{"action_id", classified->primary.action_id.value()}}));
    }

    if (!classified->alternatives.empty()) {
      const float gap = top_conf - classified->alternatives.front().confidence;
      if (gap < config_.min_gap) {
        return fail(
            make_ambiguity_error("classifier.ambiguity.insufficient_gap",
                                 "Confidence gap between top two candidates is too small.",
                                 {ErrorDetail{"gap", std::to_string(gap)},
                                  ErrorDetail{"min_gap", std::to_string(config_.min_gap)}}));
      }
    }

    // Level 2: slot-filling when required slots are absent
    const ActionSchema* schema = registry_.latest_for(classified->primary.action_id);
    if (schema != nullptr && has_missing_required_slots(*schema, classified->primary.slots_json)) {
      std::string fill_prompt = PromptBuilder::build_slot_fill_prompt(
          *schema, intent_text, classified->primary.slots_json);

      auto fill_response = backend_.generate(LlmRequest{
          .prompt = std::move(fill_prompt),
          .max_tokens = config_.max_tokens,
          .temperature = 0.0f,
      });
      if (fill_response.has_value()) {
        classified->primary.slots_json =
            merge_slots(classified->primary.slots_json, fill_response->text);
      }
      // Level 2 failure is non-fatal: proceed with Level 1 slots
    }

    return classified;

  } catch (const std::exception& ex) {
    return fail(make_inference_error(
        "classifier.unexpected_error",
        std::string{"Unexpected error during intent classification: "} + ex.what()));
  } catch (...) {
    return fail(make_inference_error("classifier.unexpected_error",
                                     "Unknown error during intent classification."));
  }
}

} // namespace aetheris::infrastructure
