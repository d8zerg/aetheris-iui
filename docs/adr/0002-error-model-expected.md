# ADR 0002: Error Model Through std::expected

Status: accepted

## Context

The platform plan requires typed error classes across all language bindings:
`InputError`, `PolicyError`, `InferenceError`, `AmbiguityError`, `DomainError`, and
`InternalError`. Hot-path operations should not rely on exceptions for ordinary failures.

## Decision

Domain and application APIs return `aetheris::domain::Result<T>`, an alias for
`std::expected<T, PlatformError>`. `PlatformError` is a closed `std::variant` over the
six public error classes. Exceptions remain reserved for bootstrap boundaries and
unrecoverable allocation failures at outer interfaces.

## Consequences

Use cases can compose fallible operations without hidden control flow. Bindings can map
the closed error set into native language errors without parsing strings. New error
classes require an explicit ADR update because they affect every public boundary.
