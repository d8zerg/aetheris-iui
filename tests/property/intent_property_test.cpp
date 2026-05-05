#include <string>
#include <vector>

#include <rapidcheck.h>

#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/quantity.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "aetheris/infrastructure/llm_intent_classifier.hpp"
#include "aetheris/infrastructure/stub_llm_backend.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

[[nodiscard]] ActionSchemaRegistry make_registry() {
  ActionSchemaRegistry registry;
  (void)registry.register_schema(*ActionSchema::create(ActionSchemaDraft{
      .action_id = *ActionId::parse("camera.disable"),
      .version = *SchemaVersion::parse("1.0.0"),
      .parameters = *ParameterSignature::create(
          R"({"type":"object","properties":{"cameraId":{"type":"string"}},"required":["cameraId"]})"),
      .reversibility = ReversibilityClass::reversible,
      .blast_radius = BlastRadius{.classification = BlastRadiusClass::bounded,
                                  .limit = BlastRadiusLimit::from(10)},
      .idempotency_key = *IdempotencyKey::create("action_id + cameraId"),
      .dry_run = DryRunRequirement::optional,
      .side_effect = SideEffectClass::writes_system,
      .required_scopes = *NonEmptyVector<std::string>::create({"camera.write"}),
      .confirmation = ConfirmationMode::single,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create(
          {ActionExample{.intent = "Disable camera 5", .parameters_json = R"({"cameraId":"5"})"}}),
      .validation_rules = {},
  }));
  return registry;
}

/**
 * Property: classify() never crashes or throws for any arbitrary LLM response.
 * It must return either a ClassifiedIntent or a typed PlatformError.
 */
[[nodiscard]] bool classify_never_crashes_on_arbitrary_response() {
  return rc::check("LlmIntentClassifier::classify() does not crash on arbitrary LLM output", [] {
    const auto raw = *rc::gen::string<std::string>();

    auto registry = make_registry();
    StubLlmBackend backend;
    backend.push_response(raw);
    // Provide a second response for potential Level 2 call
    backend.push_response(raw);

    LlmIntentClassifier classifier{
        backend, registry, {.min_confidence = 0.0f, .min_gap = 0.0f, .max_tokens = 64}};

    const auto result = classifier.classify("test intent", {}, "en");
    // Result must be a valid Result<ClassifiedIntent> - either value or error
    if (result.has_value()) {
      // Primary action_id must not be empty if we got a value
      RC_ASSERT(!result->primary.action_id.value().empty());
    } else {
      // Error must have a non-empty code
      RC_ASSERT(!error_code(result.error()).empty());
    }
  });
}

/**
 * Property: classify() with a well-formed single-candidate response above
 * threshold always produces a ClassifiedIntent (no false negatives).
 */
[[nodiscard]] bool valid_high_confidence_response_always_succeeds() {
  return rc::check("valid high-confidence JSON response always returns ClassifiedIntent", [] {
    // Generate a valid action_id (alphanumeric + common chars)
    const std::string action_id = "camera.disable";
    const float confidence = 0.9f + 0.09f * (static_cast<float>(*rc::gen::inRange(0, 10)) / 10.0f);

    const std::string slots_json =
        R"({"cameraId":"cam-)" + std::to_string(*rc::gen::inRange(0, 100)) + "\"}";

    const std::string response = R"({"candidates":[{"action_id":")" + action_id +
                                 R"(","confidence":)" + std::to_string(confidence) +
                                 R"(,"slots":)" + slots_json + "}]}";

    auto registry = make_registry();
    StubLlmBackend backend;
    backend.push_response(response);

    LlmIntentClassifier classifier{
        backend, registry, {.min_confidence = 0.7f, .min_gap = 0.0f, .max_tokens = 64}};

    const auto result = classifier.classify("disable camera", {}, "en");
    RC_ASSERT(result.has_value());
    RC_ASSERT(result->primary.action_id.value() == action_id);
  });
}

} // namespace

int main() {
  const bool p1 = classify_never_crashes_on_arbitrary_response();
  const bool p2 = valid_high_confidence_response_always_succeeds();
  return (p1 && p2) ? 0 : 1;
}
