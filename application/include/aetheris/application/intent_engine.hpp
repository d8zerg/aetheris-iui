#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/knowledge_orchestrator.hpp"
#include "aetheris/domain/ports/id_generator_port.hpp"
#include "aetheris/domain/ports/intent_classifier_port.hpp"
#include "aetheris/domain/ports/session_repository_port.hpp"
#include "aetheris/domain/ports/slot_extractor_port.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/schema_registry.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::application {

struct IntentEngineConfig {
  std::size_t max_knowledge_entries{10};
  std::chrono::seconds session_ttl{3600};
};

/**
 * Application-layer orchestrator for the intent resolution pipeline.
 *
 * Pipeline (per process() call):
 *   1. Retrieve knowledge context - graceful degradation if sources fail.
 *   2. Classify intent text -> ranked list of action candidates.
 *   3. Look up the winning action schema in the registry.
 *   4. Verify the operator's granted scopes cover all required_scopes.
 *   5. Extract typed slots from the classifier's slots_json.
 *   6. Create and persist an IntentSession.
 *   7. Return the new SessionId.
 */
class IntentEngine {
 public:
  using Config = IntentEngineConfig;

  IntentEngine(domain::KnowledgeOrchestrator& knowledge, domain::IntentClassifierPort& classifier,
               domain::SlotExtractorPort& slot_extractor, domain::ActionSchemaRegistry& registry,
               domain::SessionRepositoryPort& sessions, domain::IdGeneratorPort& ids,
               Config config = Config{}) noexcept
      : knowledge_(knowledge), classifier_(classifier), slot_extractor_(slot_extractor),
        registry_(registry), sessions_(sessions), ids_(ids), config_(config) {}

  [[nodiscard]] domain::Result<domain::SessionId>
  process(domain::OperatorId operator_id, domain::TenantId tenant_id,
          const std::string& intent_text, const std::string& locale,
          const std::vector<std::string>& granted_scopes,
          std::chrono::system_clock::time_point now) noexcept {
    try {
      const domain::KnowledgeQuery query{
          .text = intent_text,
          .action_id = {},
          .locale = locale,
          .max_results = config_.max_knowledge_entries,
      };

      std::vector<domain::KnowledgeEntry> context;
      if (auto kr = knowledge_.retrieve(query); kr.has_value()) {
        context = std::move(*kr);
      }
      // knowledge failure -> empty context (degradation, not an error)

      auto classified = classifier_.classify(intent_text, context, locale);
      if (!classified.has_value()) {
        return domain::fail(classified.error());
      }

      const domain::ActionId& action_id = classified->primary.action_id;
      const domain::ActionSchema* schema = registry_.latest_for(action_id);
      if (schema == nullptr) {
        return domain::fail(domain::make_input_error(
            "intent_engine.action.not_found",
            "The classified action is not registered in the schema registry.",
            {domain::ErrorDetail{"action_id", action_id.value()}}));
      }

      for (const auto& required : schema->required_scopes().values()) {
        const bool found = std::find(granted_scopes.begin(), granted_scopes.end(), required) !=
                           granted_scopes.end();
        if (!found) {
          return domain::fail(
              domain::make_policy_error("intent_engine.scopes.insufficient",
                                        "Operator lacks a required scope for this action.",
                                        {domain::ErrorDetail{"required_scope", required},
                                         domain::ErrorDetail{"action_id", action_id.value()}}));
        }
      }

      auto slots = slot_extractor_.extract(*schema, classified->primary.slots_json);
      if (!slots.has_value()) {
        return domain::fail(slots.error());
      }

      auto session_id = ids_.next_session_id();
      if (!session_id.has_value()) {
        return domain::fail(session_id.error());
      }

      auto session =
          domain::IntentSession::create(*session_id, std::move(operator_id), std::move(tenant_id),
                                        action_id, std::move(*slots), now, config_.session_ttl);
      if (!session.has_value()) {
        return domain::fail(session.error());
      }

      auto saved = sessions_.save(*session);
      if (!saved.has_value()) {
        return domain::fail(saved.error());
      }

      return std::move(*session_id);

    } catch (const std::exception& ex) {
      return domain::fail(domain::make_internal_error(
          "intent_engine.unexpected_error",
          std::string{"Unexpected error in intent pipeline: "} + ex.what()));
    } catch (...) {
      return domain::fail(domain::make_internal_error("intent_engine.unexpected_error",
                                                      "Unknown error in intent pipeline."));
    }
  }

 private:
  domain::KnowledgeOrchestrator& knowledge_;
  domain::IntentClassifierPort& classifier_;
  domain::SlotExtractorPort& slot_extractor_;
  domain::ActionSchemaRegistry& registry_;
  domain::SessionRepositoryPort& sessions_;
  domain::IdGeneratorPort& ids_;
  Config config_;
};

} // namespace aetheris::application
