# Architecture Layers

Aetheris-IUI follows clean architecture with hexagonal boundaries. The bootstrap project
creates one target per layer:

| Layer | Target | Responsibility |
| --- | --- | --- |
| Domain | `aetheris::domain` | Pure types, invariants, errors, and ports |
| Application | `aetheris::application` | Use cases and orchestration over domain ports |
| Infrastructure | `aetheris::infrastructure` | Implementations of ports and adapters |
| Interface | `aetheris::interface` | C ABI, transports, bindings, and CLI entry points |

The intended dependency direction is inward:

```text
interface -> infrastructure -> application -> domain
```

The bootstrap code keeps domain free of I/O, concrete time sources, persistence, and
serialization formats. Infrastructure owns default implementations such as `SystemClock`,
`UuidGenerator`, and `NoopTelemetry`.

See `docs/architecture/domain-foundation.md` for the current Step 2 primitive set and
port contract test strategy.
