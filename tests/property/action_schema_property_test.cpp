#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <rapidcheck.h>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/infrastructure/action_schema_json.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

// ---- Generators ----

// The interdependencies between fields restrict the generator to valid combinations.
// Strategy: pick a valid combination tuple, then randomize the free-form fields.

struct RevRollback {
  ReversibilityClass reversibility;
  RollbackStrategy rollback;
};

struct BlastConfDryRun {
  BlastRadiusClass blast_class;
  std::uint64_t max_entities;
  SideEffectClass side_effect;
  ConfirmationMode confirmation;
  DryRunRequirement dry_run;
};

// All valid (reversibility, rollback) combinations
constexpr std::array<RevRollback, 4> kRevCombos{{
    {ReversibilityClass::reversible, RollbackStrategy::rollback_api},
    {ReversibilityClass::compensable, RollbackStrategy::compensating_action},
    {ReversibilityClass::irreversible, RollbackStrategy::none},
    {ReversibilityClass::irreversible, RollbackStrategy::manual},
}};

// All valid (blast_class, max_entities, side_effect, confirmation, dry_run) combinations.
// dry_run for irreversible writes is handled separately; these are all-valid combinations.
constexpr std::array<BlastConfDryRun, 6> kBlastCombos{{
    {BlastRadiusClass::scoped, 0, SideEffectClass::read_only, ConfirmationMode::automatic,
     DryRunRequirement::not_applicable},
    {BlastRadiusClass::scoped, 1, SideEffectClass::writes_system, ConfirmationMode::single,
     DryRunRequirement::optional},
    {BlastRadiusClass::bounded, 5, SideEffectClass::writes_system, ConfirmationMode::single,
     DryRunRequirement::optional},
    {BlastRadiusClass::bounded, 10, SideEffectClass::external_calls, ConfirmationMode::typed,
     DryRunRequirement::mandatory},
    {BlastRadiusClass::broad, 1000, SideEffectClass::notifications, ConfirmationMode::multi_party,
     DryRunRequirement::mandatory},
    {BlastRadiusClass::broad, 500, SideEffectClass::external_calls, ConfirmationMode::cooling_off,
     DryRunRequirement::mandatory},
}};

constexpr std::string_view kIdentifierChars = "abcdefghijklmnopqrstuvwxyz0123456789_.-:";

[[nodiscard]] std::string gen_identifier(int min_len = 1, int max_len = 32) {
  const auto len = *rc::gen::inRange(min_len, max_len);
  std::string result;
  result.reserve(static_cast<std::size_t>(len));
  for (int i = 0; i < len; ++i) {
    const auto idx = *rc::gen::inRange<int>(0, static_cast<int>(kIdentifierChars.size()));
    result.push_back(kIdentifierChars[static_cast<std::size_t>(idx)]);
  }
  return result;
}

[[nodiscard]] std::string gen_nonempty_string(int max_len = 64) {
  const auto len = *rc::gen::inRange(1, max_len);
  return *rc::gen::container<std::string>(static_cast<std::size_t>(len),
                                          rc::gen::inRange(' ', '~'));
}

[[nodiscard]] ActionSchemaDraft generate_valid_draft() {
  const auto rev_idx = *rc::gen::inRange<int>(0, static_cast<int>(kRevCombos.size()));
  const auto& rev = kRevCombos[static_cast<std::size_t>(rev_idx)];

  // If irreversible, we need writes + mandatory dry-run for the dry-run invariant.
  // Combinations 0 (read_only + automatic) can't go with irreversible writes.
  // Combinations 2,3,4,5 have side_effect != read_only -> dry_run must be mandatory.
  // kBlastCombos[0] = read_only+not_applicable, kBlastCombos[1..5] are write variants.
  int blast_start = 0;
  if (rev.reversibility == ReversibilityClass::irreversible) {
    // irreversible + read_only is allowed (no mandatory dry_run for read_only)
    // but mandatory dry_run is required for irreversible write actions.
    // Pick combos that are either read_only OR have mandatory dry_run.
    // kBlastCombos[0]: read_only, not_applicable -> ok with irreversible (no writes)
    // kBlastCombos[1]: writes_system, optional -> NOT ok (irreversible requires mandatory dry_run)
    // kBlastCombos[2]: writes_system, optional -> NOT ok
    // kBlastCombos[3]: external_calls, mandatory -> ok
    // kBlastCombos[4]: notifications, mandatory -> ok
    // kBlastCombos[5]: external_calls, mandatory -> ok
    // So valid indices for irreversible: 0, 3, 4, 5
    constexpr std::array<int, 4> irreversible_blast_indices{0, 3, 4, 5};
    const auto pick =
        *rc::gen::inRange<int>(0, static_cast<int>(irreversible_blast_indices.size()));
    const auto blast_idx =
        static_cast<std::size_t>(irreversible_blast_indices[static_cast<std::size_t>(pick)]);
    const auto& bc = kBlastCombos[blast_idx];
    (void)blast_start;

    const auto action_id_str = gen_identifier(3, 20) + "." + gen_identifier(3, 16);
    const auto version_str = std::to_string(*rc::gen::inRange(1, 9)) + ".0.0";
    const auto idempotency_str = "id+" + gen_identifier(3, 12);
    const auto scope = gen_identifier(3, 16) + ".write";
    const auto example_intent = gen_nonempty_string(48);
    const auto example_param_key = gen_identifier(3, 12);

    return ActionSchemaDraft{
        .action_id = *ActionId::parse(action_id_str),
        .version = *SchemaVersion::parse(version_str),
        .parameters = *ParameterSignature::create(R"({"type":"object","required":[")" +
                                                  example_param_key + R"("]})"),
        .reversibility = rev.reversibility,
        .blast_radius = BlastRadius{.classification = bc.blast_class,
                                    .limit = BlastRadiusLimit::from(bc.max_entities)},
        .idempotency_key = *IdempotencyKey::create(idempotency_str),
        .dry_run = bc.dry_run,
        .side_effect = bc.side_effect,
        .required_scopes = *NonEmptyVector<std::string>::create({scope}),
        .confirmation = bc.confirmation,
        .rollback = rev.rollback,
        .examples = *NonEmptyVector<ActionExample>::create(
            {ActionExample{.intent = example_intent,
                           .parameters_json = R"({")" + example_param_key + R"(":"x"})"}}),
        .validation_rules = {},
    };
  }

  const auto blast_idx = *rc::gen::inRange<int>(blast_start, static_cast<int>(kBlastCombos.size()));
  const auto& bc = kBlastCombos[static_cast<std::size_t>(blast_idx)];

  const auto action_id_str = gen_identifier(3, 20) + "." + gen_identifier(3, 16);
  const auto version_str = std::to_string(*rc::gen::inRange(1, 9)) + ".0.0";
  const auto idempotency_str = "id+" + gen_identifier(3, 12);
  const auto scope = gen_identifier(3, 16) + ".read";
  const auto example_intent = gen_nonempty_string(48);
  const auto example_param_key = gen_identifier(3, 12);

  return ActionSchemaDraft{
      .action_id = *ActionId::parse(action_id_str),
      .version = *SchemaVersion::parse(version_str),
      .parameters = *ParameterSignature::create(R"({"type":"object","required":[")" +
                                                example_param_key + R"("]})"),
      .reversibility = rev.reversibility,
      .blast_radius = BlastRadius{.classification = bc.blast_class,
                                  .limit = BlastRadiusLimit::from(bc.max_entities)},
      .idempotency_key = *IdempotencyKey::create(idempotency_str),
      .dry_run = bc.dry_run,
      .side_effect = bc.side_effect,
      .required_scopes = *NonEmptyVector<std::string>::create({scope}),
      .confirmation = bc.confirmation,
      .rollback = rev.rollback,
      .examples = *NonEmptyVector<ActionExample>::create({ActionExample{
          .intent = example_intent, .parameters_json = R"({")" + example_param_key + R"(":"x"})"}}),
      .validation_rules = {},
  };
}

// ---- Properties ----

[[nodiscard]] bool valid_schema_round_trips_through_json() {
  return rc::check("valid ActionSchema round-trips through JSON serialization", [] {
    auto draft = generate_valid_draft();
    auto schema = ActionSchema::create(draft);
    RC_ASSERT(schema.has_value());

    const auto json_text = serialize_action_schema_json(*schema);
    const auto reparsed = parse_action_schema_json(json_text);

    RC_ASSERT(reparsed.has_value());
    RC_ASSERT(reparsed->action_id().value() == schema->action_id().value());
    RC_ASSERT(reparsed->version().value() == schema->version().value());
    RC_ASSERT(reparsed->reversibility() == schema->reversibility());
    RC_ASSERT(reparsed->blast_radius().classification == schema->blast_radius().classification);
    RC_ASSERT(reparsed->blast_radius().limit.value() == schema->blast_radius().limit.value());
    RC_ASSERT(reparsed->confirmation() == schema->confirmation());
    RC_ASSERT(reparsed->rollback() == schema->rollback());
    RC_ASSERT(reparsed->dry_run() == schema->dry_run());
    RC_ASSERT(reparsed->side_effect() == schema->side_effect());
    RC_ASSERT(reparsed->idempotency_key().expression() == schema->idempotency_key().expression());
    RC_ASSERT(reparsed->required_scopes().values().size() ==
              schema->required_scopes().values().size());
    RC_ASSERT(reparsed->examples().values().size() == schema->examples().values().size());
  });
}

[[nodiscard]] bool double_round_trip_is_stable() {
  return rc::check("two JSON round-trips produce identical output", [] {
    auto draft = generate_valid_draft();
    auto schema = ActionSchema::create(draft);
    RC_ASSERT(schema.has_value());

    const auto first = serialize_action_schema_json(*schema);
    const auto reparsed = parse_action_schema_json(first);
    RC_ASSERT(reparsed.has_value());

    const auto second = serialize_action_schema_json(*reparsed);
    RC_ASSERT(first == second);
  });
}

} // namespace

int main() {
  const bool round_trip = valid_schema_round_trips_through_json();
  const bool stable = double_round_trip_is_stable();
  return round_trip && stable ? 0 : 1;
}
