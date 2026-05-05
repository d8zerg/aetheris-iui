# ADR-0007: State Machine as Explicit Domain Entity

Status: accepted

## Context

A multi-turn intent resolution dialogue has inherent sequential structure: the system asks
clarifying questions, the operator fills in parameter slots, the action is previewed, the
operator confirms, and the action executes.  Two approaches exist for modeling this:

1. **Implicit state** - a flat record of flags and nullable fields.  The caller checks
   which fields are populated to infer what stage the session is in.  Transitions are
   unconstrained; any field may be updated at any time.

2. **Explicit state machine** - a closed set of named states with a defined transition
   graph.  Every mutation goes through a transition method that validates the precondition
   and updates the state atomically.

## Decision

We model `IntentSession` as an **explicit state machine** aggregate with five states:

| State           | Meaning                                      |
|-----------------|----------------------------------------------|
| `fill`          | Filling required parameter slots             |
| `clarification` | Awaiting operator disambiguation of intent   |
| `preview`       | Dry-run complete, awaiting operator confirm  |
| `commit`        | Operator confirmed, execution in progress    |
| `archive`       | Terminal: completed, cancelled, or expired   |

Transitions are exposed as named methods (`request_clarification`, `accept_clarification`,
`fill_slot`, `preview`, `reject_preview`, `confirm`, `complete`, `cancel`, `expire`).
Each method returns `Result<void>` and fails with `InputError("session.transition.invalid")`
if called in the wrong state.

All mutating methods accept an explicit `now: time_point` parameter rather than calling
a `ClockPort`, enabling fully deterministic testing without mocking infrastructure.

## Consequences

**Good:**
- **Invalid state combinations are unrepresentable.** A session cannot simultaneously be in
  `clarification` and `commit` - the type system plus the transition guard prevent it.  With
  implicit state, this invariant would require runtime assertions scattered across callers.
- **Transitions are auditable.** Each named method is a first-class event with a clear
  semantic.  When the session is serialized and replayed (restart recovery), the replay
  is a sequence of the same named transitions rather than a raw field overwrite, making
  the recovery path testable with the same assertions.
- **Property-based testing is natural.** The state graph has a finite, enumerable structure;
  properties like "archive is reachable from any non-terminal state" and "no transition
  succeeds after archive" are directly expressible and automatically verified by rapidcheck.
- **Isolation by operator and tenant is enforced at aggregate creation.** The `operator_id`
  and `tenant_id` fields are immutable after `IntentSession::create()`, preventing privilege
  escalation within a session.

**Trade-offs:**
- Adding a new state (e.g., `waiting_for_approval`) requires updating the state enum,
  the transition graph, and the serialization/deserialization round-trip.  This is intentional:
  new states should be explicit design decisions, not ad-hoc field additions.
- State machine replay during deserialization requires driving through the transition sequence
  rather than directly populating fields.  This is a small cost for a significant gain in
  testability and invariant coverage.

## Alternatives Considered

**Implicit state via nullable fields:** Simpler initial implementation.  Rejected because
experience shows that nullable-field state inevitably accumulates inconsistent combinations
(e.g., a `preview_result_json` populated while `confirmation_mode` is `automatic`) that
require defensive runtime checks throughout the codebase.

**Event sourcing:** Storing a sequence of events (domain events) and deriving state.
Considered for the audit trail synergy with ADR-0005.  Deferred: the session lifecycle is
short-lived (TTL-bounded) and the audit log already covers decision events.  Full event
sourcing would add significant complexity without a clear operational benefit at this stage.
