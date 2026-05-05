#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/capability.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/schema_registry.hpp"

namespace {

using namespace aetheris::domain;

// ---- Generators ----

[[nodiscard]] ActionSchema make_test_schema(int index, BlastRadiusClass blast_class,
                                            const std::string& scope) {
  const std::string action = "action." + std::to_string(index);
  // broad blast class requires multi_party or cooling_off confirmation
  const auto confirmation = (blast_class == BlastRadiusClass::broad) ? ConfirmationMode::multi_party
                                                                     : ConfirmationMode::single;
  const auto draft =
      ActionSchemaDraft{.action_id = *ActionId::parse(action),
                        .version = *SchemaVersion::parse("1.0.0"),
                        .parameters = *ParameterSignature::create(R"({"type":"object"})"),
                        .reversibility = ReversibilityClass::reversible,
                        .blast_radius = BlastRadius{.classification = blast_class,
                                                    .limit = BlastRadiusLimit::from(100)},
                        .idempotency_key = *IdempotencyKey::create("action_id"),
                        .dry_run = DryRunRequirement::optional,
                        .side_effect = SideEffectClass::writes_system,
                        .required_scopes = *NonEmptyVector<std::string>::create({scope}),
                        .confirmation = confirmation,
                        .rollback = RollbackStrategy::rollback_api,
                        .examples = *NonEmptyVector<ActionExample>::create(
                            {ActionExample{.intent = "test", .parameters_json = R"({"x":"1"})"}}),
                        .validation_rules = {}};
  return *ActionSchema::create(draft);
}

// ---- Properties ----

[[nodiscard]] bool filter_never_returns_unpermitted_action_ids() {
  return rc::check("filter_permitted_schemas never returns actions not in permitted_action_ids",
                   [] {
                     const auto n = *rc::gen::inRange(1, 10);
                     const auto permit_count = *rc::gen::inRange(0, n + 1);

                     // Build registry with n schemas
                     ActionSchemaRegistry registry;
                     for (int i = 0; i < n; ++i) {
                       (void)registry.register_schema(
                           make_test_schema(i, BlastRadiusClass::scoped, "scope.all"));
                     }

                     // Operator permits a random subset of [0..n)
                     std::set<std::string> permitted;
                     for (int i = 0; i < permit_count && i < n; ++i) {
                       permitted.insert("action." + std::to_string(i));
                     }

                     const OperatorCapabilitySet op{
                         .operator_id = *OperatorId::parse("op-prop"),
                         .permitted_action_ids = permitted,
                         .granted_scopes = {"scope.all"},
                         .max_blast_radius = BlastRadiusClass::broad,
                     };

                     const auto result = filter_permitted_schemas(op, registry);

                     for (const auto* schema : result) {
                       RC_ASSERT(permitted.contains(schema->action_id().value()));
                     }
                   });
}

[[nodiscard]] bool filter_never_returns_actions_with_missing_scopes() {
  return rc::check("filter_permitted_schemas never returns schemas requiring missing scopes", [] {
    const auto n = *rc::gen::inRange(1, 8);

    ActionSchemaRegistry registry;
    for (int i = 0; i < n; ++i) {
      const std::string scope = "scope." + std::to_string(i % 3); // scopes 0, 1, 2
      (void)registry.register_schema(make_test_schema(i, BlastRadiusClass::scoped, scope));
    }

    // Operator holds only scope.0 and scope.1
    const std::set<std::string> granted{"scope.0", "scope.1"};
    std::set<std::string> all_action_ids;
    for (int i = 0; i < n; ++i) {
      all_action_ids.insert("action." + std::to_string(i));
    }

    const OperatorCapabilitySet op{
        .operator_id = *OperatorId::parse("op-scopeprop"),
        .permitted_action_ids = all_action_ids,
        .granted_scopes = granted,
        .max_blast_radius = BlastRadiusClass::broad,
    };

    const auto result = filter_permitted_schemas(op, registry);

    for (const auto* schema : result) {
      for (const auto& required : schema->required_scopes().values()) {
        RC_ASSERT(granted.contains(required));
      }
    }
  });
}

[[nodiscard]] bool filter_never_returns_actions_exceeding_blast_ceiling() {
  return rc::check(
      "filter_permitted_schemas never returns schemas exceeding operator blast ceiling", [] {
        const auto n = *rc::gen::inRange(1, 8);

        // Half schemas are scoped, half are broad
        ActionSchemaRegistry registry;
        for (int i = 0; i < n; ++i) {
          const auto blast = (i % 2 == 0) ? BlastRadiusClass::scoped : BlastRadiusClass::broad;
          (void)registry.register_schema(make_test_schema(i, blast, "scope.all"));
        }

        std::set<std::string> all_ids;
        for (int i = 0; i < n; ++i)
          all_ids.insert("action." + std::to_string(i));

        const OperatorCapabilitySet op{
            .operator_id = *OperatorId::parse("op-blastprop"),
            .permitted_action_ids = all_ids,
            .granted_scopes = {"scope.all"},
            .max_blast_radius = BlastRadiusClass::bounded, // bounded < broad -> broad blocked
        };

        const auto result = filter_permitted_schemas(op, registry);

        for (const auto* schema : result) {
          const auto class_int = static_cast<int>(schema->blast_radius().classification);
          const auto ceiling_int = static_cast<int>(op.max_blast_radius);
          RC_ASSERT(class_int <= ceiling_int);
        }
      });
}

} // namespace

int main() {
  const bool prop1 = filter_never_returns_unpermitted_action_ids();
  const bool prop2 = filter_never_returns_actions_with_missing_scopes();
  const bool prop3 = filter_never_returns_actions_exceeding_blast_ceiling();
  return (prop1 && prop2 && prop3) ? 0 : 1;
}
