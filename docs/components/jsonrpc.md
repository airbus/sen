# The JSON-RPC Component

The `jsonrpc` component exposes a running Sen process to outside programs without forcing
them to link against the Sen kernel. A browser UI, a Python script, a Node.js service, or any
other process can connect, inspect the live object graph, subscribe to property changes and
events, and invoke methods.

## What JSON-RPC and WebSockets are

If those terms aren't already familiar:

- **JSON-RPC 2.0** is a small request/response message format. A request is a JSON object
  saying *"call this method with these arguments, here's an id"*; the response is another
  JSON object pairing that id with a result (or an error). It is just a convention for what
  the JSON looks like -- it does not specify how the bytes travel between client and server.
- **WebSocket** is a connection-oriented bidirectional message channel kept open between a
  browser (or any program with a WebSocket library) and a server. Once the connection is
  established, either side can push messages without polling. Each side reads framed
  messages out of the socket as they arrive.

`jsonrpc` puts those two together: a long-lived WebSocket carries JSON-RPC request/response
pairs in both directions, plus server-initiated *notifications* the kernel pushes whenever
matched objects, properties, or events change.

## When to reach for jsonrpc

It is one of the ways to talk to a running Sen process from the outside:

| Component | Transport | Best for |
|---|---|---|
| `shell` | Local TCP, text protocol | Quick interactive poking from a terminal |
| `jsonrpc` | WebSocket + JSON-RPC 2.0 | Long-lived programmatic sessions, streaming notifications |

The wire grammar is defined by the component's STL
([`components/jsonrpc/stl/jsonrpc.stl`](https://github.com/airbus/sen/blob/main/components/jsonrpc/stl/jsonrpc.stl)).
Both the C++ dispatcher and the bundled TypeScript client are generated from it, so they
cannot drift.

## Running the component

```yaml title="Example jsonrpc.yaml"
load:
  - name: jsonrpc
    group: 3
    address: 127.0.0.1
    port: 8080
```

`address` and `port` are the only required fields. The full set of configuration knobs:

| Field | Default | Notes |
|---|---|---|
| `address` | -- | IP / hostname to bind. Use `0.0.0.0` to listen on all interfaces. |
| `port` | -- | TCP port. |
| `updateFreqHz` | `60` | Dispatcher tick rate. Higher = faster notification turnaround, more CPU. |
| `connectionLimits.idleTimeoutSeconds` | `120` | uWS closes a connection idle for this long. |
| `connectionLimits.maxPayloadBytes` | `16384` | Reject any inbound frame larger than this. |
| `connectionLimits.maxBackpressureBytes` | `65536` | Hard outbound buffer ceiling per connection -- crossing it drops the connection. |
| `connectionLimits.highBackpressureBytes` | `32768` | Soft threshold: above this, the server starts dropping unreliable notifications and emits `notificationsDropped` on recovery. See [Backpressure](#backpressure) below. |
| `controlBusName` | `"jsonrpc_control"` | Sen bus on `local.` where the static-file-server registry is exposed. See [Static file server](#static-file-server) below. |

The `tls` field is reserved in the schema but not yet wired through the uWebSockets SSL
backend. Setting it fails component start. Front the component with a TLS-terminating proxy
(nginx, Caddy, etc.) when `wss://` is needed.

## Wire surface

A typical session flows like this:

1. **Connect** -- the client opens the WebSocket; the server runs the configured
   `Authenticator` on the upgrade.
2. **Discover** -- the client calls `listTopology` to see what sessions and buses exist, and
   `getTypes` / `getType` to fetch class shapes.
3. **Declare an interest** -- `createInterest` opens a live query (a Sen Query Language
   string) and returns the current match-set. From there the client owns a logical
   *interest* it can read from, write to, and subscribe to. **Ordering note:** the
   server fills the initial match-set by firing `interestUpdate.added` *before* the
   `createInterest` response lands on the wire. Clients must be ready to receive
   pushes for an interest whose response hasn't acked yet; the `@sen/client` library
   handles this transparently. Hand-written clients should queue inbound notifications
   keyed by `interestName` until the response is observed. A query may name a bus that
   does not exist yet: the interest is accepted, starts empty, and resolves live once a
   publisher joins that bus — which is what makes re-declaring interests right after a
   server restart race-free. The flip side is that a typo'd bus name yields a
   forever-empty interest rather than an error; use `listTopology` to check what exists.
   A connection can hold at most 256 concurrent interests.
4. **Read / write / invoke / subscribe** -- per matched object: `getProperty`,
   `setProperty`, `invoke`, `subscribeProperty`, `subscribeEvent`, etc.
5. **Receive notifications** -- as the kernel ticks, the server pushes `interestUpdate`,
   `propertyChanged`, `eventTriggered` notifications matching the client's subscriptions.
6. **Release** -- `releaseInterest` tears down subscriptions associated with the interest;
   closing the WebSocket tears down everything.

Methods (request / response):

- `ping` -- round-trip check.
- `listTopology`, `subscribeTopology`, `unsubscribeTopology` -- discover sessions / buses.
- `getTypes`, `getType` -- enumerate and fetch the kernel's registered custom types.
- `createInterest`, `releaseInterest`, `listObjects` -- manage live queries.
- `getProperty`, `setProperty` -- read / write a property value on a matched object.
- `subscribeProperty`, `unsubscribeProperty`, `subscribeEvent`, `unsubscribeEvent`,
  `subscribeAll`, `unsubscribeAll` -- manage per-(interest, object, member) subscriptions.
- `invoke` -- call a method on a matched object.
- `getObjectsBatchState` -- read every property of one or many matched objects in a single
  round-trip.

Server-pushed notifications:

- `interestUpdate` -- objects matched by an interest came or went; carries the
  `CustomTypeSpec`s the client hasn't seen yet.
- `propertyChanged` -- one or more properties changed on a matched object.
- `eventTriggered` -- an event fired on a matched object.
- `notificationsDropped` -- emitted on recovery from a high-backpressure window,
  with the cumulative count of unreliable notifications the dispatcher dropped during it.
- `topologyChanged` -- session / bus topology shifted (for subscribers).

The full envelope shapes are defined in `jsonrpc.stl`.

## Static file server

`jsonrpc` ships an optional HTTP static-file responder that runs on the same listening
socket as the WebSocket. Other components register URL prefixes -> on-disk roots at runtime
over the control bus (default `local.jsonrpc_control`); the responder serves files under
those prefixes with appropriate MIME types. The same origin hosts the WebSocket, so a
browser-side client loaded from `http://host:8080/explorer/` can open a WebSocket to
`ws://host:8080` without crossing origins.

The canonical consumer is the `webexplorer` component: it subscribes to the control bus,
finds the `StaticFileServer` object, and calls its `registerBundle(urlPrefix, diskRoot)`
method to expose its compiled-and-bundled frontend at, e.g., `/explorer/`.

The feature is dormant when nothing registers a bundle. There is no plain-HTTP API exposed
by default.

## Backpressure

Sen pushes notifications eagerly. A slow client that subscribes to thousands of high-rate
properties can fall behind the server's send rate, causing the outbound buffer to grow
without bound. Two thresholds defend against this:

- **Soft (`highBackpressureBytes`, default 32 KiB)**: when the outbound buffer crosses
  this size, the dispatcher marks the connection as "high" and silently drops further
  *unreliable* notifications (sticky subscription updates, etc.). Reliable replies and
  events keep flowing. When the buffer drains back below the threshold, the server emits
  one `notificationsDropped {count: N}` notification carrying the cumulative drop count
  for that window so the client can know it missed updates and refresh.
- **Hard (`maxBackpressureBytes`, default 64 KiB)**: crossing this ceiling drops the
  connection. The client must reconnect and re-establish state.

Both thresholds are per-connection. A misbehaving client cannot affect well-behaved
neighbors.

## Clients

A TypeScript client (`@sen/client`) ships in-tree at
[`components/jsonrpc/clients/typescript/`](https://github.com/airbus/sen/tree/main/components/jsonrpc/clients/typescript).
It wraps the wire surface in idiomatic browser / Node.js code and ships matching React
bindings (`@sen/client/react`). See the dedicated TypeScript client documentation for
install instructions, the `Client` / `InterestHandle` / `ObjectHandle` lifecycle, error
handling, reconnection, and the React hook surface.

The wire is JSON-RPC 2.0, so a client in any other language can be written against
`jsonrpc.stl` directly. The Python ecosystem has `python-jsonrpc-client` and similar
options; the Sen project does not maintain one in-tree.

## Authentication

A pluggable seam in `auth.h` runs on every WebSocket upgrade. The default `Authenticator`
implementation (`NoAuth`) accepts every upgrade.

To require auth, subclass `Authenticator`, read the `Authorization` header (or any other
upgrade-request fields), validate, and return an `Identity` (or reject with `401`). The
returned `Identity` is forwarded into every handler call. Until a non-default authenticator
is wired, gate `jsonrpc` at the network layer (firewall, reverse-proxy auth) when exposing
it beyond `127.0.0.1`.

## Component architecture

See [`components/jsonrpc/architecture.md`](https://github.com/airbus/sen/blob/main/components/jsonrpc/architecture.md)
for the contributor-facing overview: threading model, layer diagram, per-module
responsibilities, key invariants.
