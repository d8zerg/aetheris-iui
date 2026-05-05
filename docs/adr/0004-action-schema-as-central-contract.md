# ADR-0004: Action Schema as the Central Platform Contract

Status: accepted

## Context

Aetheris-IUI translates natural-language operator intent into safe, auditable domain actions.
The platform needs a stable contract between three parties:

1. **Domain integrators** - the teams that expose actions from a target system
2. **The intent engine** - which maps user utterances to candidate actions
3. **The safety layer** - which enforces confirmation, blast radius, and rollback policy

Without a canonical contract these three concerns diverge. Integrators ad-hoc their own
metadata; the intent engine has no consistent schema to reason over; safety rules drift.

## Decision

We introduce **Action Schema** as a first-class versioned contract. It is the unit of
integration and the single source of truth for everything the platform needs to know about
an action.

Action Schema captures:

- **Identity**: stable `id` and semantic `version`
- **Parameter signature**: JSON Schema 2020-12 for the action's accepted inputs
- **Safety metadata**: reversibility class, blast radius class and upper bound, dry-run
  requirement, side effect class
- **Confirmation policy**: one of five modes (`automatic`, `single`, `typed`, `multi_party`,
  `cooling_off`) derived from the combination of blast radius and side effects
- **Rollback strategy**: how to undo or compensate the action
- **Examples**: non-empty list of (intent, parameters) pairs used by the intent engine for
  retrieval and fine-tuning
- **Validation rules**: declarative business invariants that the safety pipeline checks
  before execution

The schema is authored in JSON (wire format) and maps 1:1 to the `ActionSchema` C++ domain
aggregate, which enforces all invariants at construction time.

## Consequences

**Good:**
- Integrators have a single artefact to author and version; everything else is derived.
- The intent engine, safety layer, and audit system all consume the same object - no
  translation between subsystem-specific formats.
- Invariants are enforced at the domain level, not scattered across callers:
  - Reversible actions require `rollback_api`.
  - Compensable actions require `compensating_action`.
  - Irreversible actions cannot have automated rollback.
  - Broad blast radius requires `multi_party` or `cooling_off` confirmation.
  - Irreversible write actions require mandatory dry-run.
- JSON representation is fully round-trippable and validated by property-based tests.
- `aetheris-lint` validates schema files before they enter the registry.

**Trade-offs:**
- Integrators must declare the full schema upfront; there is no partial or dynamic schema.
  This is intentional: ambiguity in the contract is a safety risk, not a convenience to
  preserve.
- The confirmation mode is constrained by blast radius and side effects; integrators cannot
  freely choose it. This reduces flexibility but prevents under-confirmation of dangerous
  actions.
- Schema versioning is monotonic and identified by the `version` field. Compatibility across
  versions is the integrator's responsibility; the registry detects duplicate (id, version)
  pairs and rejects them.

## Alternatives Considered

**Dynamic capability discovery (introspection only):** The platform could discover actions
by querying the target system at runtime instead of requiring a schema file. Rejected: live
introspection provides type signatures but not safety metadata, confirmation policy, or
examples - all of which are required for the intent pipeline.

**Separate metadata files per concern:** Safety metadata in one file, confirmation policy in
another. Rejected: splits what must be consistent into multiple files, making invariant
violations possible and harder to detect.

**Protocol Buffers / Thrift as primary wire format:** Provides strong typing on the wire but
requires schema compilation and tooling. Rejected at this stage: JSON Schema 2020-12 is more
accessible to domain integrators and sufficient for v0. A compiled wire format can be added
in a later step without breaking the domain model.
