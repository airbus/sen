# jsonrpc -- Architecture

One-page mental model for contributors. For end-user docs see
[`docs/components/jsonrpc.md`](../../docs/components/jsonrpc.md); for the wire grammar see
[`stl/jsonrpc.stl`](stl/jsonrpc.stl).

## Purpose

Expose a running Sen process to outside programs over a JSON-RPC 2.0 + WebSocket channel,
without forcing those programs to link against the Sen kernel. Inspect the live object graph,
subscribe to property changes and events, invoke methods, optionally serve static assets
(e.g. a web UI bundle) from the same origin.

## Threading model

Two threads matter:

- **uWS event loop**: owns the listening socket, decodes WebSocket frames, queues inbound
  messages, drains outbound messages. Pinned to a dedicated `std::thread` spawned by
  `WebSocketServer`. Never touches kernel objects directly.
- **Kernel-scheduled run thread**: the same tick that drives the component's kernel
  callbacks. `Dispatcher::onTick` drains the inbound queue, dispatches each JSON-RPC
  message to its handler (which reads / writes kernel state), and batches outbound
  notifications back into the uWS queue.

Per-connection inbound / outbound queues are MoodyCamel `ConcurrentQueue`s, so the two
threads never share a lock on the hot path.

## Layers (top to bottom)

```text
+---------------------------+
| WebSocketServer            |  uWS thread: socket I/O, auth, static-file routes
+----+----------------------+
     | inbound + outbound MoodyCamel queues (cross-thread boundary)
+----v----------------------+
| Dispatcher                 |  run() thread: drains inbound, routes by method, batches outbound
+---------------------------+
| Server (per connection)    |  per-connection state: interests, identity, ObjectSubs
+---------------------------+
| subscriptions::* helpers   |  free functions: wire/rewire/drop guards on (object, member)
+---------------------------+
```

`Server` does not own `subscriptions`; both live in the per-connection state and the
helpers are called by `Server`. The boundary that actually matters is the thread split
between `WebSocketServer` and `Dispatcher`, drawn explicitly above.

## Modules

| File | Class | Responsibility |
|---|---|---|
| `ws_server.{h,cpp}` | `WebSocketServer` | uWS event loop + listening socket + per-connection in/out queues. RAII-stopped. |
| `dispatcher.{h,cpp}` | `Dispatcher` | Drains inbound per tick, routes by method name, batches outbound notifications. Owns one `JsonGenerator` for opt-in type-schema serialization. |
| `server.{h,cpp}` | `Server` | Per-connection facade. Tracks declared interests, holds the authenticated `Identity`, owns per-object subscription state, builds `interestUpdate` deltas. |
| `subscriptions.{h,cpp}` | `ObjectSubs` and friends | One wire subscription per `(object, member)` regardless of consumer count. Refcounted. Sticky across object remove/add cycles within an interest, so subscribers keep firing across the gap. |
| `topology_service.{h,cpp}` | `SessionInfoProvider` | Backs `listTopology` / `subscribeTopology`. Walks the kernel's session + bus tree. |
| `static_file_server.{h,cpp}` | `StaticFileServer`, `BundleRegistry` | Optional. Serves static assets from URL prefixes registered at runtime via the control bus. Used by `webexplorer` to ship its frontend bundle on the same origin as the WebSocket. |
| `loaded_bundle.{h,cpp}` | `LoadedBundle` | One registered static-file bundle. Holds the on-disk root + URL prefix + cached MIME-type table. |
| `server_registry.{h,cpp}` | `ObjectSource` registry helpers | Bridge between the kernel's object-graph view and what each `Server` instance exposes. |
| `auth.{h,cpp}` | `Authenticator`, `NoAuth` | Pluggable seam called on the WebSocket upgrade. Default `NoAuth` accepts every upgrade. A future `BearerAuth` (or similar) drops in here without touching `WebSocketServer`. |

## Key invariants

- **One thread owns the wire**: the uWS thread is the only one that touches socket FDs.
  The dispatcher tick observes from a queue; if the queue is empty, the tick is a no-op.
- **One subscription per `(object, member)`**: `Subscriptions` refcounts consumers. A
  property that 50 clients subscribe to triggers exactly one wire subscription. When the
  underlying object is removed and a fresh one matches the same interest, the subscription
  "sticks" -- consumers keep firing without re-issuing `subscribeProperty`.
- **Interests are sticky end-to-end — including the bus.** `createInterest` against a bus
  that does not exist yet succeeds (kernel `getSource` is get-or-create; it cannot fail for
  a well-formed address — there is no "unknown bus" error): the subscription attaches to the
  freshly created empty bus and resolves when a publisher joins the address. Reconnecting
  clients re-declaring before the domain rebuilds depend on this. The per-connection
  interest count is capped (`maxInterestsPerConnection`) — it is the one remote-controlled
  resource nothing else bounds.
- **Outbound is bounded**: each connection has a soft (`highBackpressureBytes`) and hard
  (`maxBackpressureBytes`) ceiling. The uWS thread synthesises an internal
  `BackpressureUpdate` inbound message when the soft threshold is crossed; the dispatcher
  notes the connection as "high" and starts dropping unreliable notifications. On recovery
  the dispatcher emits a `notificationsDropped` wire notification with the cumulative drop
  count. Crossing the hard ceiling closes the connection. (`backpressureUpdate` is the
  internal-message name only; clients see `notificationsDropped`.)
- **Wire grammar is the only source of truth**: every method and notification shape lives
  in `stl/jsonrpc.stl`. The dispatcher dispatch table and the TypeScript client's wire
  types are both generated from it -- they cannot drift.

## Method dispatch

Inbound JSON-RPC methods flow through one of two paths:

- **Generic meta-dispatch (default):** `Dispatcher::dispatchViaMeta` looks the method up in the
  generated `JsonRpcServer` meta and forwards to the typed `*Impl` override on `Server`. Most
  methods go this way (`ping`, `createInterest`, `getProperty`, `subscribeProperty`, etc.).
- **Hand-routed exception:** `setProperty` and `invoke` are matched by name in
  `Dispatcher::handleTextFrame` *before* the meta-dispatch fallback. The reason is async-tick
  latency: both need to thread the kernel callback's response back to the client in the same
  tick, which the generic meta path can't do for a method whose typed return is
  `JsonRpcServerBase::Result<T>`. The typed `setPropertyImpl` / `invokeImpl` overrides in
  `Server` are required by the generated abstract base but are unreachable at runtime; hitting
  them fires `SEN_ASSERT` via `unreachableHandRouted` so a broken dispatcher refactor crashes
  loudly instead of bubbling up as a generic `internalError`.

| Method | Path | Why hand-routed |
|---|---|---|
| `setProperty` | `Dispatcher::handleSetProperty` | invokeUntyped + kernel callback in one tick. |
| `invoke` | `Dispatcher::handleInvoke` | parses JSON-encoded positional args, then invokeUntyped + callback. |

Add a third hand-routed method only if the latency reason genuinely applies. Update this
table, the matching `unreachableHandRouted` comment in `server.cpp`, and the doc block above
the routing arm in `dispatcher.cpp` together.

## Wire types

`util::walkVarToJson` chooses the JSON shape for each Sen scalar:

| Sen kind | JSON shape | Why |
|---|---|---|
| `bool` / `i8`..`i32` / `u8`..`u32` / `f32` / `f64` / `string` | JSON native | Fits a JS Number with no precision loss. |
| `i64` / `u64` / `Duration` (ns) | **decimal string** (`"9007199254740993"`) | JS Number is a 64-bit double; integers above 2^53 silently truncate. The decoded TS shape is `bigint`. |
| `TimeStamp` | RFC-3339 UTC with nanosecond precision (`"2026-06-19T07:40:12.345678901Z"`) | One canonical text form across the chain; consumers parse with a single regex. |

The libs/core `varToJson` keeps i64 native because the binary transports it feeds
(BSON / CBOR / msgpack / UBJSON) carry int64 directly; only this component's wire
needs the string encoding.

## Extension points

- **New methods**: add to `stl/jsonrpc.stl` + implement the handler. The dispatcher table
  is regenerated; the TypeScript client gains the typed call automatically.
- **New notifications**: same as above, plus emit from a `Server` method holding the per-
  connection state.
- **Authentication**: subclass `Authenticator`, swap the default `NoAuth` instance at
  component construction. The `Identity` you return is forwarded into every handler call.
- **Static-file consumers**: `webexplorer` is the canonical caller -- subscribe to the
  control bus (default `local.jsonrpc_control`), find the `StaticFileServer` object, call
  `registerBundle(urlPrefix, diskRoot)`.

## Tests

- `test/dispatcher_test.cpp`, `test/dispatcher_fixture.{h,cpp}` -- dispatcher routing,
  meta-dispatch, envelope shapes; the fixture lets tests push raw frames and assert on
  the outbound queue without standing up uWS.
- `test/rpc_methods_test.cpp` -- per-handler behavior for `ping`, `invoke`, `getProperty`,
  `setProperty`, `getObjectsBatchState`, etc.
- `test/subscribe_test.cpp` -- sticky-subscription state machine across object churn.
- `test/multi_connection_test.cpp` -- multi-connection scenarios (e.g. the sequential-
  connect churn case from the TS suite).
- `test/ws_server_test.cpp` -- WebSocket I/O end-to-end on a real socket.
- `test/static_file_server_test.cpp` + `test/static_file_routes_test.cpp` +
  `test/loaded_bundle_test.cpp` -- bundle registry, HTTP routing, URL normalization.
- `test/lifecycle_test.cpp` -- create / destroy ordering, abort paths.
