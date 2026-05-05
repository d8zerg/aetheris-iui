# ADR 0003: Strong Domain Primitives

Status: accepted

## Context

Aetheris-IUI routes operator intent into domain actions that may affect real enterprise
systems. Primitive values such as identifiers, quotas, budgets, and blast-radius limits
must not be interchangeable by accident.

## Decision

The domain layer uses small strong primitives:

- `Identifier<TTag>` for public ids such as `ActionId`, `SessionId`, and `TenantId`
- `Tagged<TValue, TTag>` for lightweight type separation over trusted values
- `Quantity<TUnit>` for non-negative counts and budgets whose unit is part of the type
- `NonEmptyVector<T>` for collections that must have at least one value

Parsing and validation return `Result<T>` so callers receive typed `PlatformError`
instances instead of exceptions for ordinary invalid input.

## Consequences

The domain API is slightly more explicit than a primitive-based API, but misuse becomes
visible at compile time. Integration code must parse or construct these values at the
boundary before handing them to use cases.
