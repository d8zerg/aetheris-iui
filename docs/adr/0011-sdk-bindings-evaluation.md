# ADR-0011 - SDK, Language Bindings, and Evaluation Harness

Status: accepted  

## Context

Step 10 completes the platform by providing external integrators with:

1. A stable, language-agnostic C ABI;
2. First-party Python and Node.js bindings;
3. A unified CLI for day-to-day development tasks;
4. An evaluation harness for validating and comparing schema-lint behaviour;
5. Smoke tests that verify all three deployment formats (embedded, sidecar,
   standalone) against a single build.

## Decisions

### C ABI Stability Contract

The public C ABI in `c_api.h` is versioned through `aetheris_abi_version()` and
`kStableAbiVersion` (application layer constant).  Every breaking change
increments this integer.  Language bindings must check the ABI version at
startup against the version they were compiled against; a mismatch signals an
incompatible upgrade.

The `aetheris_context` opaque struct isolates callers from internal layout
changes (PIMPL-equivalent at C level).

### Python Binding Strategy

The binding ships as a pure-Python package (`aetheris_iui`) that communicates
with the `aetheris_stdio_server` sidecar via subprocess JSON-lines IPC.

**Rationale:** pybind11 bindings would compile the entire C++ stack into a
native Python extension, requiring a C++ toolchain in the integrator's
environment.  The subprocess approach:

- Zero native compilation for the Python consumer;
- Reuses the already-audited stdio server protocol;
- Works identically across CPython, PyPy, and embedded runtimes.

The pybind11 native binding source (`bindings/python/pybind11_stub.cpp`) is
provided as a reference for performance-critical integrations.

### Node.js Binding Strategy

Same rationale as Python: the binding wraps the stdio server via Node.js
`child_process` IPC rather than N-API native compilation.  The `AetherisClient`
class in `bindings/node/index.mjs` exposes the same surface as the Python
client.

The N-API native binding path remains available for integrators that require
in-process embedding (e.g., Electron main process with sub-millisecond latency
requirements) by compiling the C ABI as a shared library and loading it via
`ffi-napi`.

### Unified CLI (`aetheris`)

A single binary replaces the ad-hoc `aetheris-lint` and `aetheris-audit` tools
as the primary UX for integrators.  Subcommands:

| Command | Purpose |
|---|---|
| `aetheris version` | Print library version and ABI |
| `aetheris schema lint [FILE...]` | Validate Action Schema files |
| `aetheris schema generate <openapi>` | Stub OpenAPI -> Action Schema (delegates to npm package) |
| `aetheris adapter new <name>` | Scaffold a new adapter project |
| `aetheris eval run <dataset.jsonl>` | Run evaluation harness |
| `aetheris audit verify <log.jsonl>` | Verify audit chain integrity |

The focused single-purpose tools (`aetheris-lint`, `aetheris-audit`) remain for
scripts that depend on them.

### Evaluation Harness

The harness (`eval/runner.py`) operates on JSONL golden datasets where each
line specifies a schema, the expected validity outcome, and (optionally) the
expected warnings.  This design enables:

- **Regression testing**: run the same dataset against two binary versions to
  detect behaviour changes;
- **Drift detection**: alert when a previously-valid schema becomes invalid after
  a schema-version upgrade;
- **Shadow mode**: run in parallel with production and compare outcomes.

The golden dataset in `eval/datasets/golden.jsonl` covers all confirmation
modes, blast-radius classes, and known invalid schema combinations.

### Deployment Format Smoke Tests

Three deployment formats are verified by CTest:

| Label | Format | Test |
|---|---|---|
| `smoke.bootstrap` | embedded (static lib) | `aetheris_smoke_test` binary |
| `smoke.abi_stability` | embedded (C ABI) | `aetheris_abi_tests` GTest binary |
| `smoke.cli` | standalone (CLI process) | `aetheris --version` exit-0 check |
| `smoke.eval_harness` | standalone (CLI + eval) | `aetheris eval run golden.jsonl` |
| `smoke.python_binding` | sidecar (stdio IPC) | Python smoke test against stdio server |
| `smoke.node_binding` | sidecar (stdio IPC) | Node.js smoke test against stdio server |

## Consequences

- The ABI version integer must be incremented for every breaking C API change;
  the `smoke.abi_stability` test will catch accidental bumps vs. omissions.
- The subprocess-based bindings add a dependency on the `aetheris_stdio_server`
  binary being present; integrators who embed as a static library need the
  pybind11 or N-API native paths instead.
- The evaluation harness is Python-only; it does not participate in the C++
  build graph, keeping the C++ build hermetic.
- The golden dataset in `eval/datasets/` is the canonical regression baseline;
  changes to schema validation logic must update this dataset first (golden
  update workflow).
