# ADR-0010 - Client Exchange Protocol and Verification Layer UI

**Status:** Accepted  
**Date:** 2026-05-05  
**Deciders:** Aetheris-IUI core team

---

## Context

The Aetheris-IUI engine produces `IntentSession` objects that must be surfaced
to human operators through a UI.  The UI must:

1. Present session state in a framework-agnostic way.
2. Allow operators to fill slots, preview actions, and confirm or cancel.
3. Support five distinct confirmation modes (automatic, single, typed,
   multi-party, cooling-off) that carry meaningful risk-mitigation semantics.
4. Be embeddable in web apps built with React, Vue, Angular, or plain HTML.

## Decision

### TypeScript Headless Core (`@aetheris-iui/core`)

A framework-agnostic TypeScript package owns:

- **Transport abstraction** - `Transport` interface with `WebSocketTransport`
  (production) and `MockTransport` (tests).  Callers never touch the wire
  directly.
- **`AetherisClient`** - stateful client that converts `ServerMessage` events
  into `AetherisSession` instances and exposes intent operations
  (`processIntent`, `fillSlot`, `preview`, `confirm`, `cancel`).
- **`AetherisSession`** - `EventTarget` subclass; fires `change` events so
  framework adapters can re-render on state transitions without polling.
- **`computeDiff` / `changedEntries`** - pure functions for diffing any two
  JSON values, used by `<aui-diff-view>` and available to callers building
  custom UIs.

### Web Components (`@aetheris-iui/web-components`)

Four Custom Elements provide a zero-dependency UI layer:

| Element | Purpose |
|---|---|
| `<aui-confirm-button>` | All five confirmation modes in one element |
| `<aui-slot-form>` | Dynamic slot-filling form with validation state |
| `<aui-diff-view>` | Table / tree / JSON diff renderer |
| `<aui-session-preview>` | Read-only session summary for the preview state |

All elements use Shadow DOM, emit `composed: true` custom events, and carry no
runtime dependencies beyond `@aetheris-iui/core`.

### Framework Adapters

Thin glue packages wrap the headless core for each framework:

- **`@aetheris-iui/react`** - `AetherisContext` + `useAetherisSession` hook.
  The hook subscribes to `AetherisSession.change` events and triggers
  React re-renders.
- **`@aetheris-iui/vue`** - `useAetherisSession` composable returning reactive
  refs and action callbacks.
- **`@aetheris-iui/angular`** - `AetherisService` (Injectable) backed by
  RxJS `BehaviorSubject<AetherisSession|null>` per session.  `AetherisModule`
  provides `forRoot(client)` for DI wiring.

### C++ Interface Layer

The C++ side exposes two new artefacts:

1. **`SessionSnapshot`** - a UI-ready projection of `IntentSession` with all
   enums stringified and timestamps in microseconds-since-epoch (matching the
   TypeScript `Session` interface).
2. **`aetheris_stdio_server`** - a JSON-lines stdio process that acts as an IPC
   bridge for integration testing and for host environments (Electron, native
   shells) that cannot use WebSockets directly.
3. **`aetheris_session_snapshot_json()` / `aetheris_free_string()`** - stable
   C ABI additions for FFI callers.

### IPC Protocol

The stdio server speaks newline-delimited JSON.  Message types:

| Direction | Type | Purpose |
|---|---|---|
| C->S | `ping` | Liveness check |
| S->C | `pong` | Version echoed |
| C->S | `version` | Query library version + ABI |
| C->S | `snapshot` | Project a session object |
| S->C | `snapshot` | Re-serialised SessionSnapshot |
| C->S | `quit` | Graceful shutdown |
| S->C | `error` | Protocol or parse error |

This is intentionally simple; a WebSocket proxy wrapping the stdio server is
the recommended production path for browser clients.

## Consequences

- The five confirmation modes in `ConfirmationMode` (domain enum) are mirrored
  exactly in the TypeScript `ConfirmationMode` type and the
  `<aui-confirm-button>` `mode` attribute - a single source of truth via the
  JSON wire format.
- Snapshot serialisation lives in the interface layer, keeping the domain and
  application layers free of JSON/string concerns.
- The headless core is independently testable (Vitest, fast-check) without a
  running C++ server; the mock transport makes unit tests fast and hermetic.
- The stdio server is not a production WebSocket server; teams integrating
  Aetheris-IUI into a production stack should wrap it behind a proper WS proxy
  or implement a native backend transport.
