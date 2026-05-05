#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aetheris/application/intent_engine.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/knowledge_orchestrator.hpp"
#include "aetheris/domain/non_empty_vector.hpp"
#include "aetheris/domain/ports/intent_classifier_port.hpp"
#include "aetheris/domain/ports/slot_extractor_port.hpp"
#include "aetheris/domain/quantity.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "aetheris/infrastructure/in_memory_session_repository.hpp"
#include "aetheris/infrastructure/schema_slot_extractor.hpp"
#include "aetheris/infrastructure/uuid_generator.hpp"
#include "support/schema_test_helpers.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::application;
using namespace aetheris::infrastructure;
using namespace aetheris::tests;

// --- Test doubles ---

class StubClassifier final : public IntentClassifierPort {
 public:
  explicit StubClassifier(Result<ClassifiedIntent> result) : result_(std::move(result)) {}

  [[nodiscard]] Result<ClassifiedIntent> classify(const std::string&,
                                                  const std::vector<KnowledgeEntry>&,
                                                  const std::string&) noexcept override {
    return result_;
  }

 private:
  Result<ClassifiedIntent> result_;
};

class StubSlotExtractor final : public SlotExtractorPort {
 public:
  explicit StubSlotExtractor(Result<std::vector<Slot>> result) : result_(std::move(result)) {}

  [[nodiscard]] Result<std::vector<Slot>> extract(const ActionSchema&,
                                                  std::string_view) noexcept override {
    return result_;
  }

 private:
  Result<std::vector<Slot>> result_;
};

// --- Helpers ---

[[nodiscard]] inline ActionSchemaDraft
make_camera_draft(const std::string& action_id = "camera.disable",
                  const std::string& scope = "camera.write") {
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
      .required_scopes = *NonEmptyVector<std::string>::create({scope}),
      .confirmation = ConfirmationMode::single,
      .rollback = RollbackStrategy::rollback_api,
      .examples = *NonEmptyVector<ActionExample>::create(
          {ActionExample{.intent = "Disable camera 5", .parameters_json = R"({"cameraId":"5"})"}}),
      .validation_rules = {},
  };
}

[[nodiscard]] ClassifiedIntent make_classified(const std::string& action_id,
                                               float confidence = 0.95f) {
  return ClassifiedIntent{
      .primary =
          IntentCandidate{
              .action_id = *ActionId::parse(action_id),
              .confidence = confidence,
              .slots_json = R"({"cameraId":"cam-1"})",
          },
      .alternatives = {},
      .raw_llm_response = "",
  };
}

const auto kNow = std::chrono::system_clock::now();

// --- Tests ---

TEST(IntentEngineTest, Process_ValidIntent_ReturnsSessionId) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubClassifier classifier{make_classified("camera.disable")};
  StubSlotExtractor extractor{
      std::vector<Slot>{Slot{.name = "cameraId", .required = true, .value_json = "\"cam-1\""}}};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "disable camera 1", "en", {"camera.write"}, kNow);

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->value().empty());

  // Verify session was persisted
  const auto loaded = sessions.load(*result);
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->action_id().value(), "camera.disable");
}

TEST(IntentEngineTest, Process_InsufficientScopes_ReturnsPolicyError) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubClassifier classifier{make_classified("camera.disable")};
  StubSlotExtractor extractor{std::vector<Slot>{}};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "disable camera 1", "en", {} /* no scopes */, kNow);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "policy");
  EXPECT_EQ(error_code(result.error()), "intent_engine.scopes.insufficient");
}

TEST(IntentEngineTest, Process_ClassifierFails_PropagatesError) {
  ActionSchemaRegistry registry;

  StubClassifier classifier{fail(make_inference_error("backend.down", "LLM unavailable."))};
  StubSlotExtractor extractor{std::vector<Slot>{}};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "test", "en", {}, kNow);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "backend.down");
}

TEST(IntentEngineTest, Process_UnknownAction_ReturnsInputError) {
  ActionSchemaRegistry registry;
  // "camera.disable" is NOT registered

  StubClassifier classifier{make_classified("camera.disable")};
  StubSlotExtractor extractor{std::vector<Slot>{}};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "disable camera 1", "en", {"camera.write"}, kNow);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "input");
  EXPECT_EQ(error_code(result.error()), "intent_engine.action.not_found");
}

TEST(IntentEngineTest, Process_SlotExtractionFails_PropagatesError) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  StubClassifier classifier{make_classified("camera.disable")};
  StubSlotExtractor extractor{
      fail(make_inference_error("slot_extractor.slots.parse_error", "Malformed slots."))};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "disable camera 1", "en", {"camera.write"}, kNow);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
}

TEST(IntentEngineTest, Process_EmptyGrantedScopes_AllScopesRequired) {
  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry
                  .register_schema(
                      *ActionSchema::create(make_camera_draft("camera.disable", "camera.write")))
                  .has_value());

  StubClassifier classifier{make_classified("camera.disable")};
  StubSlotExtractor extractor{std::vector<Slot>{}};
  KnowledgeOrchestrator knowledge{{}};
  InMemorySessionRepository sessions;
  UuidGenerator ids{42};

  IntentEngine engine{knowledge, classifier, extractor, registry, sessions, ids};

  const auto result = engine.process(*OperatorId::parse("op-1"), *TenantId::parse("tenant-1"),
                                     "test", "en", {} /* empty */, kNow);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "policy");
}

// ---- SchemaSlotExtractor integration ----

TEST(SchemaSlotExtractorTest, Extract_FullSchema_BuildsSlotList) {
  using namespace aetheris::infrastructure;

  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  SchemaSlotExtractor extractor;
  const auto* schema = registry.latest_for(*ActionId::parse("camera.disable"));
  ASSERT_NE(schema, nullptr);

  const auto result = extractor.extract(*schema, R"({"cameraId":"cam-5"})");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].name, "cameraId");
  EXPECT_TRUE((*result)[0].required);
  ASSERT_TRUE((*result)[0].value_json.has_value());
  EXPECT_EQ(*(*result)[0].value_json, "\"cam-5\"");
}

TEST(SchemaSlotExtractorTest, Extract_MissingSlot_ValueJsonIsNullopt) {
  using namespace aetheris::infrastructure;

  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  SchemaSlotExtractor extractor;
  const auto* schema = registry.latest_for(*ActionId::parse("camera.disable"));
  ASSERT_NE(schema, nullptr);

  const auto result = extractor.extract(*schema, "{}");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].name, "cameraId");
  EXPECT_TRUE((*result)[0].required);
  EXPECT_FALSE((*result)[0].value_json.has_value());
}

TEST(SchemaSlotExtractorTest, Extract_MalformedSlotsJson_ReturnsInferenceError) {
  using namespace aetheris::infrastructure;

  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  SchemaSlotExtractor extractor;
  const auto* schema = registry.latest_for(*ActionId::parse("camera.disable"));
  ASSERT_NE(schema, nullptr);

  const auto result = extractor.extract(*schema, "not-json");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(error_kind(result.error()), "inference");
  EXPECT_EQ(error_code(result.error()), "slot_extractor.slots.parse_error");
}

TEST(SchemaSlotExtractorTest, Extract_EmptySlotsJson_TreatedAsEmptyObject) {
  using namespace aetheris::infrastructure;

  ActionSchemaRegistry registry;
  ASSERT_TRUE(registry.register_schema(*ActionSchema::create(make_camera_draft())).has_value());

  SchemaSlotExtractor extractor;
  const auto* schema = registry.latest_for(*ActionId::parse("camera.disable"));
  ASSERT_NE(schema, nullptr);

  const auto result = extractor.extract(*schema, "");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_FALSE((*result)[0].value_json.has_value());
}

} // namespace
