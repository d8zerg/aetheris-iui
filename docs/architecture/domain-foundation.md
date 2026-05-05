# Domain Foundation

The current domain foundation is intentionally small and dependency-free. It contains the
types needed by later Action Schema, Session, Safety, and Audit work:

| Type | Purpose |
| --- | --- |
| `Result<T>` | `std::expected<T, PlatformError>` for ordinary fallible operations |
| `PlatformError` | Closed error sum for input, policy, inference, ambiguity, domain, and internal failures |
| `Identifier<TTag>` | Canonical string ids with syntax validation and strong aliases |
| `Tagged<TValue, TTag>` | Strong wrapper for trusted primitive values |
| `Quantity<TUnit>` | Non-negative typed counts for blast radius, budgets, and limits |
| `NonEmptyVector<T>` | Collection invariant for candidate sets and examples |
| `ActionSchema` | Core action contract with safety and rollback invariants |
| `ActionSchemaRegistry` | In-memory registry with version lookup and reflection helpers |
| Port concepts | Compile-time contracts for default port implementations |

Default infrastructure implementations currently cover:

- `SystemClock`
- `UuidGenerator`
- `NoopTelemetry`

The test suite includes reusable port contract helpers in `tests/support/port_contracts.hpp`.
Future infrastructure adapters should reuse these helpers before adding adapter-specific
cases.
