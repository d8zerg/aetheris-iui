# ADR-0006: Capability Binding as Primary LLM Isolation Mechanism

Status: accepted

## Context

An LLM-powered intent engine must never execute an action that the current operator
is not permitted to perform, regardless of what the LLM infers from the intent.  This is
the fundamental safety invariant: **the LLM resolves intent to candidate actions, but it
does not grant permissions.**

Two broad strategies exist for enforcing this invariant:

1. **Post-selection filtering** - let the LLM freely rank all registered schemas; filter
   out unpermitted candidates after selection.  The LLM sees schemas it cannot use.

2. **Pre-selection capability binding** - materialize the operator's permission set into a
   narrow, operator-specific schema list before the LLM sees any candidates.  The LLM
   only reasons over schemas the operator is allowed to invoke.

## Decision

We use **pre-selection capability binding** as the primary isolation mechanism.

`OperatorCapabilitySet` encapsulates three independent authorization dimensions:

- **Explicit action allow-list** (`permitted_action_ids`) - schemas not in the list are
  invisible to the LLM for this operator.
- **OAuth-style scopes** (`granted_scopes`) - every scope declared in the schema's
  `required_scopes` must be present in the operator's grant.
- **Blast radius ceiling** (`max_blast_radius`) - schemas whose blast radius class exceeds
  the operator's ceiling are excluded regardless of explicit allow-list membership.

`filter_permitted_schemas` computes the intersection of these three dimensions against the
`ActionSchemaRegistry` and returns only the schemas that satisfy all three.  The LLM
receives this reduced list as its candidate space.

A second enforcement layer - `ValidationPipeline` - re-runs the same permission check
plus blast radius enforcement on the resolved schema after LLM selection.  This
defense-in-depth catches any bypass attempt (e.g., direct API invocation bypassing the
LLM, a rogue adapter that presents a different schema list).

## Consequences

**Good:**
- The attack surface for capability escalation is minimized by design.  Even a fully
  compromised LLM cannot invoke an action that is absent from the filtered list, because
  that action identifier would fail the `ValidationPipeline` permission check before
  any infrastructure port is called.
- Prompt injection attacks that attempt to hijack the LLM's output are bounded: the
  injected text can at most cause the LLM to select a different action from the
  *already-permitted* candidate set.  It cannot expand the set.
- The capability model is stateless and pure: `filter_permitted_schemas` and
  `check_permission` are side-effect-free functions; they can be called at any point in
  the request lifecycle without consistency concerns.
- Operator capability sets are immutable after construction.  No downstream component
  can escalate its own privileges by mutating the set.

**Trade-offs:**
- The filtered candidate list may be smaller than the full registry, which can reduce
  LLM resolution accuracy when many similar schemas exist.  Mitigation: operator
  permissions should be scoped to real job roles, not artificially narrowed for
  performance.
- Capability sets must be materialized from an external authorization source (IAM,
  RBAC, ABAC) before each intent resolution.  The adapter that performs this
  materialization is outside the scope of this ADR and is deferred to a later step.

## Related

- **Untrusted content tagging** (`Untrusted<T>`) - domain type that forces explicit
  acknowledgment at every site where untrusted external data (domain-system fields,
  LLM output) crosses a trust boundary.  Complements capability binding by making
  injection risks visible at the type level.
- **ValidationPipeline** - Chain of Responsibility that re-enforces capability binding
  after LLM selection, providing defense-in-depth.
- **ADR-0005** - hash chain audit log that records every pipeline decision for
  post-hoc compliance verification.
