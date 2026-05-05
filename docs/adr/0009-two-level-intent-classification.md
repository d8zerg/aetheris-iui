# ADR 0009 - Two-Level Intent Classification

Status: accepted

## Context
Aetheris-IUI must map free-form operator text to a strongly-typed `IntentSession`
with all required action parameters filled.  A single LLM call cannot reliably
solve both problems simultaneously: classification benefits from seeing many action
candidates at once, while slot-filling benefits from focused instructions for one
specific action with its JSON Schema.

## Decision
Implement a two-level generation strategy inside `LlmIntentClassifier`:

| Level | Prompt focus | Expected response |
|-------|-------------|-------------------|
| 1     | All registered action schemas + knowledge context -> action ranking | `{"candidates":[{"action_id":"…","confidence":0-1,"slots":{…}}]}` |
| 2     | Single resolved action schema + original intent -> slot values only | `{"param":"value",…}` |

Level 2 runs **only** when the Level 1 response is missing one or more entries
from the resolved schema's `required` list.  If Level 2 fails (network error,
parse error, OOM), the pipeline continues with the Level 1 slots; the
`IntentSession` is created in the `fill` state and the operator is prompted for
the missing values.

### Ambiguity thresholds (configurable per deployment)
- `min_confidence` (default 0.7) - primary candidate's confidence must meet this.
- `min_gap` (default 0.2) - confidence gap between the top two candidates.

Violating either threshold returns `AmbiguityError`, causing the
`IntentEngine` to propagate the error to the interface layer for clarification.

## Architecture
```
IntentEngine (application)
  │  calls
  ▼
IntentClassifierPort (domain port)
  │  implemented by
  ▼
LlmIntentClassifier (infrastructure)
  ├─ LlmBackendPort  ← Level 1 + Level 2 generate() calls
  ├─ ActionSchemaRegistry ← prompt construction + required-slot check
  └─ PromptBuilder (infrastructure helper, pure static)
```

`SchemaSlotExtractor` (infrastructure) implements `SlotExtractorPort` and
translates the final `slots_json` string into a typed `vector<Slot>` aligned
with the action's JSON Schema `properties` and `required` arrays.

## Consequences
- Each intent text triggers 1 or 2 LLM calls.  2-call latency is bounded because
  Level 2 is skipped when Level 1 already provides all required slots.
- Classification accuracy and slot-filling accuracy can be tuned independently
  via the two prompt templates in `PromptBuilder`.
- The `IntentEngine` and the `IntentClassifierPort` abstraction allow the two-level
  strategy to be replaced (e.g., with a structured-output single-call approach)
  without changing any application or domain code.
