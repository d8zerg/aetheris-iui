# ADR 0001: C++23, Conan 2, and Clean Architecture

Status: accepted

## Context

Aetheris-IUI is intended to run as an embedded library, sidecar process, or standalone
service. The core must keep latency and memory overhead predictable while remaining
portable across Linux, macOS, and Windows.

The product concept also requires strict architectural boundaries: domain types and
invariants must not depend on infrastructure, and outer layers must be replaceable.

## Decision

The repository uses C++23 as the baseline language standard, CMake as the target-based
build system, and Conan 2 for third-party dependency resolution. The source tree is split
into clean architecture layers:

- `domain`
- `application`
- `infrastructure`
- `interface`

Dependencies point inward: `interface -> infrastructure -> application -> domain`.
The current bootstrap keeps each layer as a separately linkable CMake target.

## Consequences

The first implementation increment favors explicit target wiring and small public headers
over framework-level convenience. CI builds the project on Linux, macOS, and Windows, and
CTest is the single entry point for test execution.
