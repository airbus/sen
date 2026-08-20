# @sen/client

A TypeScript client for [Sen](https://github.com/airbus/sen) over JSON-RPC 2.0 / WebSocket.
Connect to a running Sen process, query its objects, subscribe to property changes and events,
and invoke methods. Works in browsers and in Node.js (>=22).

```bash
npm install @sen/client
```

## Hello, animals

In one terminal, start a Sen process exposing the bundled `animals` package:

```bash
sen run components/jsonrpc/clients/typescript/examples/browser/animals.yaml
```

That spins up two objects on the bus `my.tutorial`: `rufus` (a Cat) and `elon` (a Dog), both
extending the base class `Animal`. The JSON-RPC server listens on `ws://127.0.0.1:8080`.

In a second terminal, connect to it:

```ts
import { connect } from "@sen/client";

const client = await connect({ url: "ws://127.0.0.1:8080" });

// Declare an interest: a live query against the Sen object graph.
const interest = await client.declareInterest({
  name: "cats",
  query: "SELECT animals.Cat FROM my.tutorial",
});

// React to every Cat the interest matches now or in the future. The handler fires
// immediately for every object already present and again for each later arrival.
interest.onObjectAdded(async (cat) => {
  console.log(`got ${cat.className} named ${cat.name}`);

  // Subscribe to the cat's position. The handler fires once with the current value
  // and again every time it changes.
  cat.onPropertyChanged("position", (pos) => {
    console.log("position:", pos);
  });

  // Call a method. Arguments are passed by name.
  await cat.invoke("jumpToLocation", { x: 42, y: 17 });
});

// Stay open for a few seconds so subscribed updates have time to arrive, then clean up.
// In a real app you'd keep the client open for as long as you need it (e.g. until SIGINT).
await new Promise((r) => setTimeout(r, 3_000));

await interest.release();
client.close();
```

You should see the initial position print, then the post-jump position, before the script
exits. The `onObjectAdded` callback handles the initial match set and live churn uniformly,
which is the whole point of the API shape.

## Concepts

The library has three layered handles:

- **`Client`**: the connection. One per Sen process. Owns the WebSocket, hands out interests.
- **`InterestHandle`**: a live query. Keeps a local cache of matching objects in sync with the
  server. Notifies you when objects come and go.
- **`ObjectHandle`**: one object in an interest's match set. The unit of work for property
  reads/writes, event subscriptions, and method calls.

You only ever construct a `Client`. Interests and objects come from it.

### Queries

`declareInterest` takes a Sen query string. The simplest form is `SELECT <Class> FROM <bus>`,
but the query language supports joins, filters, and inheritance traversal. See the Sen
[interest documentation](https://airbus.github.io/sen/latest/) for the full grammar.

Register `onObjectAdded` to drive work; it fires for every currently-matched object on
registration and again for every later arrival.

```ts
interest.onObjectAdded((obj) => console.log("added:", obj.className, obj.name));
interest.onObjectRemoved((name) => console.log("removed:", name));
```

`interest.objects()` returns a synchronous snapshot of currently-matched objects (combine
with `onObjectAdded` to avoid missing late arrivals). `for await (const obj of interest)`
is a thin wrapper over `onObjectAdded`.

`await interest.release()` drops the server-side state and clears local handlers; idempotent.

#### Pre-subscribing

By default an interest only tracks object membership; you pay a round-trip on the first
`obj.get(prop)`. Pass a `subscribe` block to ask the server to pre-subscribe property values
(and optionally events) as part of the same call, so matched objects arrive with their values
already cached:

```ts
const fleet = await client.declareInterest({
  name: "fleet",
  query: "SELECT aircraft.Aircraft FROM ops",
  subscribe: {
    properties: ["altitude", "speed"],   // or "*" for every property
    events: ["landed", "takenOff"],      // or "*", or omit
    maxRateHz: 10,                       // optional throttle on property pushes
  },
});
```

After pre-subscribing, the first `obj.get("altitude")` returns from the local cache without a
wire trip, and any `obj.onPropertyChanged("altitude", ...)` registered later sees the value the
moment a push arrives.

### Properties

Read once:

```ts
const pos = await rufus.get("position");
```

`get` returns a value from the local cache if there is one (kept fresh by active subscriptions);
pass `{ fresh: true }` to force a server round-trip.

Write (only on properties marked `[writable]` in the STL):

```ts
await rufus.set("name", "Rufus the Magnificent");
```

Writes to non-writable properties throw `JsonRpcError`.

Subscribe to changes:

```ts
const cancel = rufus.onPropertyChanged("position", (pos) => {
  console.log("position:", pos);
});

// later:
cancel();
```

Multiple consumers can subscribe to the same property. The library issues one wire
subscription and fans out; when the last consumer cancels, it unsubscribes on the wire.

To watch every property in one bundle:

```ts
rufus.onAnyChange((changes) => {
  for (const [name, value] of changes) console.log(name, "=", value);
});
```

### Events

Events are like properties but fire-and-forget; the handler receives the positional args:

```ts
rufus.onEventTriggered("madeSound", (args) => {
  const [content] = args;
  console.log("the cat said:", content);
});
```

### Methods

`invoke` takes a method name and an object of named arguments:

```ts
await rufus.invoke("jumpToLocation", { x: 42, y: 17 });
const reply = await someTeacher.invoke("ask", { question: "what time is it?" });
```

Arguments are validated locally against the method's declared signature before the call goes
on the wire, so a typo in an argument name throws immediately. The return value, if any, is
the method's declared return type; void methods return `null`.

### Runtime values

Properties, event args, and method results come back as `Var` values. Most are plain
JavaScript: strings, numbers, booleans, arrays, plain objects. Two types are runtime classes:

- **`Quantity`**: a number with units (`{value, unit, minValue?, maxValue?}`).
- **`Variant`**: a tagged union (`{type, value}`).

Use the narrowing helpers (`isPrimitive`, `isStruct`, `isSequence`, `isQuantity`, `isVariant`)
to walk an unknown value:

```ts
import { isQuantity, isVariant } from "@sen/client";

const distance = await rufus.get("travelDistance");
if (isQuantity(distance)) {
  console.log(`${distance.value} ${distance.unit.abbreviation}`);
}
```

**64-bit integers (`i64` / `u64`)** arrive as `bigint`, not `number`. The server emits them
as JSON decimal strings to preserve precision past `Number.MAX_SAFE_INTEGER` (2^53 - 1), and
the client decodes that string into a `bigint`. Values fed into APIs that need a `number`
(uPlot x-axes, `Date(ms)`, etc.) must be narrowed explicitly with `numberFromExact`, which
throws `RangeError` instead of silently losing the high bits:

```ts
import { numberFromExact } from "@sen/client";

const byteCount = await disk.get("byteCount");   // bigint
if (typeof byteCount === "bigint") {
  // safe for values up to 2^53 - 1; throws past that
  const asNumber = numberFromExact(byteCount, "byteCount");
}
```

`TimeStamp` arrives as the wire string (RFC-3339 UTC with nanosecond precision); pass it
through `parseSenTimestamp` for a `Date` (millisecond precision) plus a `nanoseconds`
remainder.

## Errors

Three classes, all extending `SenClientError` (which extends `Error`):

- **`JsonRpcError`**: the server returned a JSON-RPC error. Carries `code`, `message`, `data?`.
  Common case: writing a non-writable property, calling a method that does not exist on the
  server.
- **`TransportError`**: a connection-level problem (disconnect mid-call, malformed frame).
- **`TimeoutError`**: the call did not get a response in time. Carries `timeoutMs`.

Per-call timeout defaults come from `ClientOptions.defaultTimeoutMs`; override with
`{ timeoutMs }` on individual calls. Pass an `AbortSignal` via `{ signal }` for composable
cancellation.

## Reconnection

By default the transport reconnects on its own with exponential backoff, and after each
successful reconnect the client re-declares every interest automatically
(`reconnect.autoReestablish`, default `true`); subscriptions resume with them. While
reconnecting, in-flight calls fail with `TransportError`. The lifecycle hooks are for UI
state and custom work — don't call `reestablishAll()` from them yourself, recovery is
already wired:

```ts
client.onDisconnect(() => console.warn("connection lost"));
client.onReconnect(() => console.log("back online"));
```

For custom recovery (for example, apps where fresh state after an outage is the correct
behaviour), opt out with `reconnect: { autoReestablish: false }` and drive
`reestablishAll()` — or your own re-declares — from `onReconnect`. `reestablishAll()` is
single-flight: overlapping calls coalesce instead of racing each other's `createInterest`.

For UIs that want a single hook for every transition (connecting / open / reconnecting /
closed), use `onConnectionStateChange`:

```ts
client.onConnectionStateChange((state) => {
  statusBadge.textContent = state;
});
```

`reestablishAll()` re-declares every interest in parallel. Subscriptions on those interests
resume automatically once the interests are active again.

To disable auto-reconnect entirely (useful for short-lived scripts):

```ts
const client = await connect({
  url: "ws://127.0.0.1:8080",
  reconnect: { enabled: false },
});
```

## Batch reads

For panels that need every property of one or many objects in one shot (a dashboard refresh,
say), the wire offers a batch read that walks the inheritance chain server-side and returns
each object's full property map in a single round-trip:

```ts
const states = await interest.getObjectsBatchState({
  // both filters are optional; omit to fetch every matched object / every property
  objectNames: ["rufus", "elon"],
  propertyNames: ["position", "name"],
});

for (const state of states) {
  console.log(state.name, state.className, state.properties);
  // per-property failures (e.g. variant with no current value) land in state.errors
}
```

Each returned `ObjectBatchState` carries `properties` (successful reads, with the property's
`TypeSpec` attached for narrowing) and `errors` (per-property failure messages). Object names
that did not match the interest's current set are silently skipped -- check the returned
`name` field against your input list to detect them.

## React bindings

The package ships React hooks under the `@sen/client/react` entry point. They sit on top of
the same `Client` / `InterestHandle` / `ObjectHandle` lifecycle described above; you still
hold a `Client` instance for the lifetime of the React tree.

Pass the `Client` down via props or React context (your app code), then hand it to the
hooks:

```tsx
import type { Client, ObjectHandle } from "@sen/client";
import { useInterest, useObjects, useProperty, useInvoke } from "@sen/client/react";

function CatList({ client }: { client: Client }) {
  // Declares the interest on mount, releases on unmount.
  const { handle: cats } = useInterest(client, {
    name: "cats",
    query: "SELECT animals.Cat FROM my.tutorial",
  });
  // Live snapshot of the interest's match set; re-renders on add / remove.
  const objects = useObjects(cats);
  return (
    <ul>
      {objects.map((cat) => <CatRow key={cat.name} cat={cat} />)}
    </ul>
  );
}

function CatRow({ cat }: { cat: ObjectHandle }) {
  // Live-subscribes to the property; re-renders on each push.
  const position = useProperty(cat, "position") as { x: number; y: number } | undefined;
  const { invoke, status } = useInvoke(cat);
  return (
    <li>
      {cat.name} at {position?.x},{position?.y}{" "}
      <button
        onClick={() => invoke("jumpToLocation", { x: 42, y: 17 })}
        disabled={status === "pending"}
      >
        jump
      </button>
    </li>
  );
}
```

`Client` lives outside the React tree -- create it once with `await connect({ url })`,
then pass it down (props or context). The hooks accept `Client | null` /
`InterestHandle | null` so a pre-connect render is safe.

Hook surface:

| Hook | Purpose |
|---|---|
| `useConnectionState(client)` | Current connection state for status badges. |
| `useTopology(client)` | Discover sessions / buses. |
| `useTypeSpec(client, name)` | Fetch a class / struct / variant `TypeSpec` (cached per connection). |
| `useInterest(client, { name, query, subscribe? })` | Declares the interest on mount, releases on unmount. Returns `{ handle, error }`. |
| `useObjects(interest)` | Live snapshot of an interest's match-set. Returns `ObjectHandle[]`. |
| `useObject(interest, name)` | Single object by name from a given interest. |
| `useProperty(obj, name)` | Live-subscribed value of one property on one object. |
| `useSetProperty(obj)` | Returns `{ setProperty, status, error }` for writing a property with explicit pending / success / error states. |
| `useInvoke(obj)` | Returns `{ invoke, reset, status, result, error }` for invoking a method by name (passed at call time). |
| `useEvents(obj, name, opts?)` | Bounded ring of recent deliveries (default 200) for one named event on one object. |

For grids and tables that need cell-level updates without re-rendering the whole list,
`@sen/client/react` also exposes a small store primitive (`makeStore`, `makeBufferStore`,
`useCell`, `useView`) for fine-grained subscriptions. See the dedicated
[`react/store/README.md`](src/react/store/README.md).
