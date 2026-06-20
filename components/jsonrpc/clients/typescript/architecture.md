# @sen/client -- Architecture

One-page mental model for contributors. For end-user docs see [`README.md`](README.md);
for the mkdocs landing page see [`docs/components/jsonrpc_ts_client.md`](../../../../docs/components/jsonrpc_ts_client.md).

## Purpose

A TypeScript library that lets a browser or Node.js program talk to a Sen process over the
jsonrpc component's wire (JSON-RPC 2.0 + WebSocket). Wraps the raw wire surface in three
layered handles -- `Client`, `InterestHandle`, `ObjectHandle` -- and exposes matching React
hooks for UI consumers.

## Layers

```
+---------------------------------------+
| React hooks  (use_object, use_property, ...) |
| Store primitives (make_store, use_cell, ...) |
+---------------------------------------+
| Handles                                |
|   Client / InterestHandle / ObjectHandle |
+---------------------------------------+
| Internal services                       |
|   protocol_client    -- typed RPC calls |
|   subscription_registry -- fan-out     |
|   type_cache         -- per-conn TypeSpec |
|   var_codec          -- encode / parse Var |
|   transport          -- WebSocket + reconnect |
+---------------------------------------+
| Generated wire types                    |
|   src/generated/*.ts (from cli_gen ts)  |
+---------------------------------------+
```

## Modules

| Path | Class / surface | Responsibility |
|---|---|---|
| `src/connect.ts` | `connect()` | Entry point. Opens the WebSocket, runs the initial handshake, returns a ready `Client`. |
| `src/client.ts` | `Client` | The connection. One per Sen process. Owns the WebSocket lifecycle, hands out `InterestHandle`s, surfaces connection-state events. |
| `src/handles.ts` | `InterestHandle`, `ObjectHandle` | High-level facades over interests + per-object state. Hide the wire and the registry from consumers. |
| `src/errors.ts` | `SenClientError` hierarchy | `JsonRpcError`, `TransportError`, `TimeoutError`. |
| `src/values.ts` | `Var`, `Quantity`, `Variant`, `VarList`, `VarMap` | Runtime value types matching the wire shape. |
| `src/narrowing.ts` | `isPrimitive`, `isStruct`, `isSequence`, `isQuantity`, `isVariant` | Type-guard helpers for walking unknown `Var`s. |
| `src/internal/transport.ts` | WebSocket adapter | Auto-reconnect with backoff. Single inbound message handler; multiplexes responses + notifications. |
| `src/internal/protocol_client.ts` | Typed RPC calls | One method per wire RPC; encodes/decodes payloads against the generated types. |
| `src/internal/subscription_registry.ts` | Refcounted subscription fan-out | One wire subscription per `(interest, object, member)`; multiple consumers share it. |
| `src/internal/type_cache.ts` | Per-connection `TypeSpec` cache | Avoids refetching class shapes; invalidated when the connection drops. |
| `src/internal/var_codec.ts` | `parseVar` + `encodeVar` | Wire <-> runtime conversion (Quantity / Variant tagging, etc). |
| `src/internal/subscribe_codec.ts` | Subscription request encoding | Builds the wire shape from a high-level `subscribe` block. |
| `src/internal/notification_validators.ts` | Shape-only validators | Cheap checks before `parseVar` runs on deep payloads. |
| `src/internal/cancel.ts` | `CancelFn` plumbing | Unified cancel semantics across handlers and AbortSignal. |
| `src/internal/report_error.ts` | Error reporting hook | Configurable `ReportError` callback for transport-level surprises. |
| `src/internal/class_spec_lookup.ts` | Class spec helpers | Walks the inheritance chain when resolving a property's declared type. |
| `src/react/*` | React hooks | `useObject`, `useObjects`, `useProperty`, `useEvents`, `useInvoke`, etc. All sit on top of the handles. |
| `src/react/store/*` | Store primitives | `makeStore`, `makeBufferStore`, `useCell`, `useView` -- fine-grained subscription primitives for grids / tables. |
| `src/generated/*` | Wire types | Emitted at build time by `cli_gen ts` from `components/jsonrpc/stl/jsonrpc.stl`. Never edit by hand. |

## Key invariants

- **One Client per Sen process.** A `Client` owns one WebSocket. Sharing it across multiple
  components is safe and expected.
- **Handles are owned facades, not values.** `InterestHandle` and `ObjectHandle` carry
  per-consumer state (callbacks, refcounts). Don't equality-compare them across boundaries
  -- look them up via `client.interest(name)` or `interest.object(name)`.
- **One wire subscription per `(interest, object, member)`.** `subscription_registry`
  refcounts consumers. 50 React components subscribing to the same property cause one wire
  subscription; the registry fans the notification out locally.
- **Transport auto-reconnects; interests re-declare automatically by default.**
  `reconnect.autoReestablish` (default on) runs `reestablishAll()` after each successful
  reconnect; apps that want fresh-state recovery opt out and drive their own. `reestablishAll`
  is single-flight -- overlapping triggers coalesce, because two interleaved runs would race
  each other's `createInterest` into "interest already exists" and strand cleared handles.
- **Wire types are generated, not hand-written.** `cli_gen ts` runs at build time. A new
  wire field flows automatically into TypeScript; a stale hand-written type cannot drift.
- **`exactOptionalPropertyTypes`** is on (`tsconfig.json`). Code that wants "absent" must
  omit the key, not set it to `undefined`. `protocol_client` carefully builds payloads to
  match.

## Extension points

- **New wire method**: add to `components/jsonrpc/stl/jsonrpc.stl` on the C++ side; the next
  build regenerates `src/generated/`. Add a typed wrapper in `protocol_client.ts` and a
  high-level facade in `handles.ts` (or `Client`) as needed.
- **New React hook**: implement it under `src/react/`, re-export from `src/react/index.ts`.
  Hooks should sit on top of the handles, not the internal layer.
- **Replace the WebSocket**: `transport.ts` types its WebSocket dependency structurally; an
  alternative transport (e.g. an in-process pair for tests) can implement the same shape.
  `test/test_helpers/mock_websocket.ts` does exactly this.
- **Per-connection observers**: add a callback registry on `Client`. The existing
  `onDisconnect` / `onReconnect` / `onConnectionStateChange` follow the same pattern.

## Tests

- `test/*.test.ts` -- unit tests against the handles + internal layer (mocked transport).
- `test/store/*.test.ts` -- store primitives.
- `test/integration/*.test.ts` -- spawned-Sen integration: a real Sen process, a real
  WebSocket. Slower but catches wire-shape regressions the unit tests miss.
- `test/test_helpers/mock_websocket.ts` -- the in-process WebSocket double used everywhere.

## Build

- `src/generated/` is produced by the CMake target `jsonrpc_ts_client_codegen`. Run a
  CMake build of the Sen tree at least once before any `npm` command; the directory is
  gitignored and `npm test` / `npm run typecheck` will fail with unresolved imports of
  `./generated/index.js` until it is populated.
- `npm run build` -- esbuild bundles `src/index.ts` -> `dist/index.js` (ESM, source maps).
- `npm run typecheck` -- `tsc --noEmit` against the production tsconfig + the test tsconfig.
- `npm test` -- vitest, unit only.
- `npm run test:integration` -- vitest, integration. Needs `SEN_BINARY` + `SEN_CONFIG` env
  vars pointing at a built Sen + a yaml config exposing the test fixtures.
