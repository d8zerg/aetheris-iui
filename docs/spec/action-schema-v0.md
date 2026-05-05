# Action Schema v0

Status: draft

Action Schema is the public contract between Aetheris-IUI and a target domain system. It
describes one executable action, its parameter signature, safety metadata, confirmation
mode, examples, and rollback strategy.

The current C++ domain model implements the core invariants before the JSON parser lands:

- reversible actions require `rollback_api`
- compensable actions require `compensating_action`
- irreversible actions cannot declare automated rollback
- automatic confirmation is limited to scoped read-only actions
- broad blast radius requires `multi_party` or `cooling_off`
- read-only actions mark dry-run as `not_applicable`
- irreversible write actions require mandatory dry-run
- examples must include non-empty intent text and parameter JSON

## Canonical Fields

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Stable action identifier |
| `version` | string | Schema version |
| `parameters` | JSON Schema 2020-12 object | Signature of accepted parameters |
| `reversibility` | enum | `reversible`, `compensable`, `irreversible` |
| `blastRadius.class` | enum | `scoped`, `bounded`, `broad` |
| `blastRadius.maxEntities` | integer | Declared upper bound |
| `idempotencyKey` | string | Expression used to derive an idempotency key |
| `dryRun` | enum | `mandatory`, `optional`, `not_applicable` |
| `sideEffects` | enum | `read_only`, `writes_system`, `external_calls`, `notifications` |
| `requiredScopes` | array | Non-empty permission scope list |
| `confirmation` | enum | `automatic`, `single`, `typed`, `multi_party`, `cooling_off` |
| `rollback` | enum | `none`, `rollback_api`, `compensating_action`, `manual` |
| `examples` | array | Non-empty intent-to-parameters examples |
| `validationRules` | array | Business rules outside JSON Schema |

The machine-readable draft schema is in `docs/spec/action-schema.schema.json`. Reference
examples are in `docs/spec/reference-action-schemas.json`.
