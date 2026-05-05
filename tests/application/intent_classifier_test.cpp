#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/quantity.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "aetheris/infrastructure/llm_intent_classifier.hpp"
#include "aetheris/infrastructure/stub_llm_backend.hpp"
#include "support/llm_backend_contracts.hpp"
#include "support/schema_test_helpers.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;
using namespace aetheris::tests;

// Schema with full "properties" for slot extraction
[[nodiscard]] inline ActionSchemaDraft
make_camera_draft(const std::string& action_id = "camera.disable") {
  return ActionSchemaDraft{
      .action_id = *ActionId::parse(action_id),
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
  };
}

// ---- StubLlmBackend contract ----

TEST(StubLlmBackendTest, Contracts) {
  StubLlmBackend backend;
  backend.push_response("hello");
  run_all_llm_backend_contracts(backend);
}

TEST(StubLlmBackendTest, AvailableByDefault) {
  StubLlmBackend backend;
  EXPECT_TRUE(backend.is_available());
}

TEST(StubLlmBackendTest, NotAvailableWhenConstructedFalse) {
  StubLlmBackend backend{false};
  EXPECT_FALSE(backend.is_available());
}

TEST(StubLlmBackendTest, ReturnsQueuedResponses) {
  StubLlmBackend backend;
  backend.push_response("first");
  backend.push_response("second");

  const auto r1 = backend.generate(LlmRequest{.prompt = "p"});
  ASSERT_TRUE(r1.has_value());
  EXPECT_EQ(r1->text, "first");

  const auto r2 = backend.generate(LlmRequest{.prompt = "p"});
  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r2->text, "second");
}

TEST(StubLlmBackendTest, ExhaustedQueueReturnsInferenceError) {
  StubLlmBackend backend;
  const auto r = backend.generate(LlmRequest{.prompt = "p"});
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(error_kind(r.error()), "inference");
  EXPECT_EQ(error_code(r.error()), "stub.queue_exhausted");
}

TEST(StubLlmBackendTest, CallCountIncrementsPerCall) {
  StubLlmBackend backend;
  backend.push_response("a");
  backend.push_response("b");
  EXPECT_EQ(backend.call_count(), 0U);
  (void)backend.generate(LlmRequest{.prompt = "p"});
  EXPECT_EQ(backend.call_count(), 1U);
  (void)backend.generate(LlmRequest{.prompt = "p"});
  EXPECT_EQ(backend.call_count(), 2U);
}

// ---- LlmIntentClassifier ----

TEST(LlmIntentClassifierTest, ValidResponse_ReturnsPrimaryCandidate) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubLlmBackend backend;
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.disable","confidence":0.95,"slots":{"cameraId":"cam-1"}}]})");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("disable camera 1", {}, "en");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->primary.action_id.value(), "camera.disable");
  EXPECT_FLOAT_EQ(result->primary.confidence, 0.95f);
  EXPECT_FALSE(result->primary.slots_json.empty());
}

TEST(LlmIntentClassifierTest, MultipleAlternatives_SortedByConfidenceDesc) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft("camera.disable")))
                  .has_value());
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft("camera.reset")))
                  .has_value());

  StubLlmBackend backend;
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.reset","confidence":0.5,"slots":{}},{"action_id":"camera.disable","confidence":0.9,"slots":{"cameraId":"2"}}]})");

  LlmIntentClassifier classifier{backend, registry, {.min_confidence = 0.4f, .min_gap = 0.1f}};
  const auto result = classifier.classify("turn off camera 2", {}, "en");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->primary.action_id.value(), "camera.disable");
  EXPECT_FLOAT_EQ(result->primary.confidence, 0.9f);
  ASSERT_EQ(result->alternatives.size(), 1U);
  EXPECT_EQ(result->alternatives[0].action_id.value(), "camera.reset");
}

TEST(LlmIntentClassifierTest, LowConfidence_ReturnsAmbiguityError) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubLlmBackend backend;
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.disable","confidence":0.5,"slots":{}}]})");

  LlmIntentClassifier classifier{backend, registry, {.min_confidence = 0.7f, .min_gap = 0.2f}};
  const auto result = classifier.classify("maybe disable camera?", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "ambiguity");
  EXPECT_EQ(error_code(result.error()), "classifier.ambiguity.low_confidence");
}

TEST(LlmIntentClassifierTest, SmallGap_ReturnsAmbiguityError) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft("camera.disable")))
                  .has_value());
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft("camera.reset")))
                  .has_value());

  StubLlmBackend backend;
  // Gap = 0.85 - 0.82 = 0.03, below min_gap=0.2
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.disable","confidence":0.85,"slots":{}},{"action_id":"camera.reset","confidence":0.82,"slots":{}}]})");

  LlmIntentClassifier classifier{backend, registry, {.min_confidence = 0.7f, .min_gap = 0.2f}};
  const auto result = classifier.classify("camera action", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "ambiguity");
  EXPECT_EQ(error_code(result.error()), "classifier.ambiguity.insufficient_gap");
}

TEST(LlmIntentClassifierTest, BackendError_PropagatesInferenceError) {
  ActionSchemaRegistry registry;

  StubLlmBackend backend;
  backend.push_error("backend.timeout", "Request timed out.");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("disable camera", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "backend.timeout");
}

TEST(LlmIntentClassifierTest, MalformedJson_ReturnsInferenceError) {
  ActionSchemaRegistry registry;

  StubLlmBackend backend;
  backend.push_response("this is not json at all!");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("test", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "classifier.response.parse_error");
}

TEST(LlmIntentClassifierTest, MissingCandidatesField_ReturnsInferenceError) {
  ActionSchemaRegistry registry;

  StubLlmBackend backend;
  backend.push_response(R"({"intent":"camera.disable","score":0.9})");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("disable camera", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "classifier.response.missing_candidates");
}

TEST(LlmIntentClassifierTest, EmptyCandidates_ReturnsInferenceError) {
  ActionSchemaRegistry registry;

  StubLlmBackend backend;
  backend.push_response(R"({"candidates":[]})");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("test", {}, "en");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "classifier.response.empty_candidates");
}

TEST(LlmIntentClassifierTest, Level2SlotFilling_TriggeredWhenRequired) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubLlmBackend backend;
  // Level 1: missing required "cameraId"
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.disable","confidence":0.95,"slots":{}}]})");
  // Level 2: fills in the missing slot
  backend.push_response(R"({"cameraId":"cam-7"})");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("disable camera 7", {}, "en");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(backend.call_count(), 2U); // both Level 1 and Level 2 were called
  EXPECT_NE(result->primary.slots_json.find("cameraId"), std::string::npos);
}

TEST(LlmIntentClassifierTest, Level2NotTriggered_WhenSlotsPresent) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubLlmBackend backend;
  // Level 1: cameraId already present -> no Level 2
  backend.push_response(
      R"({"candidates":[{"action_id":"camera.disable","confidence":0.95,"slots":{"cameraId":"cam-3"}}]})");

  LlmIntentClassifier classifier{backend, registry};
  const auto result = classifier.classify("disable camera 3", {}, "en");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(backend.call_count(), 1U); // only Level 1 was called
}

} // namespace
