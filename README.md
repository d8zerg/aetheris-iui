# Aetheris-IUI

**Embeddable intent-driven UI infrastructure for enterprise operator workflows.**

Aetheris-IUI translates natural-language operator intent into safe, auditable domain actions. It enforces confirmation policies, blast-radius controls, dry-run requirements, and tamper-evident audit chains - all from a single embeddable C++23 library with stable C ABI and first-party TypeScript, Python, and Node.js bindings.

---

## Features

- **Action Schema** - versioned JSON contract capturing safety metadata (reversibility, blast radius, confirmation mode, rollback strategy, dry-run requirement, side effects)
- **Intent engine** - two-level LLM-backed classifier with schema-guided slot filling and retrieval
- **Session state machine** - explicit `fill -> clarification -> preview -> commit -> archive` lifecycle with TTL and cancellation
- **Confirmation modes** - `automatic`, `single`, `typed`, `multi_party`, `cooling_off` enforced at the domain level
- **Tamper-evident audit chain** - SHA-256 hash chain over decision records with field-level redaction policy
- **Web Components** - `<aui-confirm-button>`, `<aui-slot-form>`, `<aui-diff-view>`, `<aui-session-preview>` with zero runtime dependencies
- **Framework adapters** - React hook, Vue composable, Angular injectable service
- **Stable C ABI** - opaque `aetheris_context*`, versioned via `aetheris_abi_version()`
- **Python & Node.js bindings** - subprocess IPC wrappers; no native compilation required in the integrator's environment
- **Unified CLI** - `aetheris` binary for schema linting, stub generation, eval harness, and audit verification

---

## Repository Layout

```
domain/           Pure domain types, invariants, value objects, ports
application/      Use cases and orchestration over domain ports
infrastructure/   Concrete port implementations (JSON, SHA-256, session store)
interface/        C ABI, stdio JSON-lines server, session snapshot projection
packages/
  core/           @aetheris-iui/core  - TypeScript headless client
  web-components/ @aetheris-iui/web-components - Custom Elements UI layer
  react/          @aetheris-iui/react - React hook + context
  vue/            @aetheris-iui/vue   - Vue composable
  angular/        @aetheris-iui/angular - Angular service + module
bindings/
  python/         aetheris_iui - pure-Python subprocess binding
  node/           @aetheris-iui/node  - ESM Node.js binding
eval/             Golden-dataset evaluation harness (Python)
tools/            Formatting, static analysis, and git hooks
tests/            CTest - unit, property, smoke, ABI stability
bench/            Google Benchmark targets
docs/adr/         Architecture Decision Records (ADR-0001 … ADR-0011)
```

---

## Building

### Prerequisites

| Tool | Minimum version |
|---|---|
| GCC or Clang | C++23 support |
| CMake | 3.25 |
| Ninja | any |
| Conan | 2.x |
| Python | 3.9 (eval harness + Python binding) |
| Node.js | 18 (Node.js binding + web packages) |

### Quick start (bundled dependencies)

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### With Conan-managed dependencies

```bash
conan profile detect --force
conan install . \
  --output-folder=build/conan \
  -s build_type=Debug \
  -s compiler.cppstd=23 \
  --build=missing

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/conan/conan_toolchain.cmake"

cmake --build build
ctest --test-dir build --output-on-failure
```

### Available presets

`debug`, `release`, `relwithdebinfo`, `linux-debug`, `linux-release`, `macos-debug`, `macos-release`, `windows-debug`, `windows-release`

---

## Action Schema

An Action Schema is a JSON file that declares everything the platform needs to know about a domain action. The `aetheris schema lint` command validates it:

```json
{
  "id": "billing.issue_refund",
  "version": "1.0.0",
  "parameters": {
    "type": "object",
    "properties": {
      "order_id": { "type": "string" },
      "amount_cents": { "type": "integer", "minimum": 1 }
    },
    "required": ["order_id", "amount_cents"]
  },
  "reversibility": "compensable",
  "blastRadius": { "class": "bounded", "maxEntities": 1 },
  "idempotencyKey": "{{ order_id }}-refund",
  "dryRun": "optional",
  "sideEffects": "external_calls",
  "requiredScopes": ["billing:write"],
  "confirmation": "typed",
  "rollback": "compensating_action",
  "examples": [
    {
      "intent": "Refund order ORD-9921 for $25",
      "parameters": { "order_id": "ORD-9921", "amount_cents": 2500 }
    }
  ],
  "validationRules": []
}
```

Schema invariants enforced at construction time:

| Condition | Rule |
|---|---|
| `reversible` | rollback must be `rollback_api` |
| `compensable` | rollback must be `compensating_action` |
| `irreversible` | rollback cannot be automated; dry-run is `mandatory` |
| `read_only` | dry-run is `not_applicable` |
| `automatic` confirmation | only for `scoped` + `read_only` |
| `broad` blast radius | confirmation must be `multi_party` or `cooling_off` |

---

## CLI

```bash
# Validate schema files
aetheris schema lint schemas/*.json

# Read from stdin
cat my_action.json | aetheris schema lint

# Scaffold a new adapter project
aetheris adapter new payments-adapter

# Run the golden-dataset evaluation harness
aetheris eval run eval/datasets/golden.jsonl

# Verify an audit chain log
aetheris audit verify /var/log/aetheris/audit.jsonl

# Print version and ABI
aetheris version
```

---

## TypeScript / Web

### Install

```bash
npm install @aetheris-iui/core @aetheris-iui/web-components
```

### Headless client

```typescript
import { AetherisClient, WebSocketTransport } from '@aetheris-iui/core';

const client = new AetherisClient(new WebSocketTransport('wss://aetheris.example.com'));
const session = await client.startSession({ actionId: 'billing.issue_refund' });

session.addEventListener('change', () => renderUI(session));

await session.fillSlot('order_id', '"ORD-9921"');
await session.fillSlot('amount_cents', '2500');
await session.preview();
await session.confirm();
```

### Web Components

```html
<script type="module" src="@aetheris-iui/web-components/dist/index.js"></script>

<aui-slot-form></aui-slot-form>
<aui-confirm-button mode="typed" label="Issue refund"></aui-confirm-button>
<aui-session-preview></aui-session-preview>
```

### React

```tsx
import { AetherisContext } from '@aetheris-iui/react';
import { useAetherisSession } from '@aetheris-iui/react';

function RefundFlow({ sessionId }: { sessionId: string }) {
  const { session, fillSlot, confirm, cancel } = useAetherisSession(sessionId);
  // ...
}

function App() {
  return (
    <AetherisContext.Provider value={client}>
      <RefundFlow sessionId="sess-001" />
    </AetherisContext.Provider>
  );
}
```

### Vue

```typescript
import { useAetherisSession } from '@aetheris-iui/vue';

const { session, fillSlot, preview, confirm } = useAetherisSession(client, sessionId);
```

### Angular

```typescript
import { AetherisModule, AETHERIS_CLIENT } from '@aetheris-iui/angular';

@NgModule({
  imports: [AetherisModule.forRoot(myClient)],
})
export class AppModule {}
```

---

## Python Binding

```python
from aetheris_iui import AetherisClient

with AetherisClient('/usr/local/bin/aetheris_stdio_server') as client:
    print(client.version())
    snapshot = client.session_snapshot(session_dict)
```

---

## Node.js Binding

```javascript
import { AetherisClient } from '@aetheris-iui/node';

const client = new AetherisClient('/usr/local/bin/aetheris_stdio_server');
await client.connect();

const { version } = await client.version();
const { snapshot } = await client.sessionSnapshot(sessionObject);

await client.close();
```

---

## C ABI (embedding)

```c
#include "aetheris/interface/c_api.h"

aetheris_context* ctx = NULL;
aetheris_create_context(&ctx);

char* json_out = NULL;
aetheris_status st = aetheris_session_snapshot_json(ctx, session_json, &json_out);
if (st.code == AETHERIS_STATUS_OK) {
    // use json_out
    aetheris_free_string(json_out);
}

aetheris_destroy_context(ctx);
```

ABI version compatibility is checked via `aetheris_abi_version()`. Every breaking change increments the integer exposed by `kStableAbiVersion`.

---

## Tooling

```bash
# Format check / auto-fix
tools/run-format.sh check
tools/run-format.sh fix

# Static analysis (cppcheck + clang-tidy)
tools/run-static-analysis.sh build

# Install pre-commit hook (runs format + analysis)
tools/install-git-hooks.sh
```

---

## Testing

```bash
ctest --preset debug --output-on-failure

# Run by label
ctest --preset debug -L unit
ctest --preset debug -L property
ctest --preset debug -L smoke
ctest --preset debug -L contract
```

Test labels:

| Label | Coverage |
|---|---|
| `unit` | Domain and infrastructure unit tests |
| `property` | RapidCheck property-based tests |
| `smoke.bootstrap` | Embedded static-lib smoke test |
| `smoke.abi_stability` | C ABI round-trip and version checks |
| `smoke.cli` | `aetheris --version` exit-0 check |
| `smoke.eval_harness` | Full eval run against golden dataset |
| `smoke.python_binding` | Python subprocess IPC smoke test |
| `smoke.node_binding` | Node.js subprocess IPC smoke test |

---

## Architecture

Clean Architecture with inward-only dependencies:

```
interface -> infrastructure -> application -> domain
```

Key domain invariants live in the `domain` layer and are never relaxed by outer layers. All ADRs are in [`docs/adr/`](docs/adr/).

| ADR | Decision |
|---|---|
| [0001](docs/adr/0001-cpp23-conan-clean-architecture.md) | C++23, Conan 2, clean architecture layers |
| [0002](docs/adr/0002-error-model-expected.md) | `std::expected`-based error model |
| [0003](docs/adr/0003-strong-domain-primitives.md) | Strong domain value types |
| [0004](docs/adr/0004-action-schema-as-central-contract.md) | Action Schema as the central contract |
| [0005](docs/adr/0005-hash-chain-instead-of-per-record-signature.md) | SHA-256 hash chain for audit integrity |
| [0006](docs/adr/0006-capability-binding-as-llm-isolation.md) | Capability binding for LLM isolation |
| [0007](docs/adr/0007-state-machine-as-explicit-entity.md) | Explicit session state machine |
| [0008](docs/adr/0008-baseline-without-vector-search.md) | Keyword baseline without vector search |
| [0009](docs/adr/0009-two-level-intent-classification.md) | Two-level LLM intent classification |
| [0010](docs/adr/0010-client-exchange-protocol.md) | TypeScript headless core + Web Components |
| [0011](docs/adr/0011-sdk-bindings-evaluation.md) | C ABI, language bindings, evaluation harness |

---

## License

[LICENSE](LICENSE)
