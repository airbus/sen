# The TypeScript client (`@sen/client`)

A TypeScript library that lets a browser or Node.js program talk to a running Sen process
over the [`jsonrpc`](jsonrpc.md) component's wire (JSON-RPC 2.0 + WebSocket). Connect, walk
the live object graph, subscribe to property changes and events, invoke methods, and (in
React apps) drive a UI off the same data without writing your own subscription bookkeeping.

It ships in-tree at
[`components/jsonrpc/clients/typescript/`](https://github.com/airbus/sen/tree/main/components/jsonrpc/clients/typescript)
and is the primary client for the [Sen web explorer](webexplorer.md) and for any custom operator UI
built against Sen.

## When to use it

| Want to ... | Reach for |
|---|---|
| Build a browser UI that drives Sen interactively | `@sen/client` + `@sen/client/react` |
| Write a Node.js script that polls / scripts a running Sen | `@sen/client` (no React) |
| Build a backend service that bridges Sen to another protocol | `@sen/client` |
| Talk to Sen from a non-TypeScript language | Write a client against `jsonrpc.stl` directly |

## Hello, animals

In one terminal, start a Sen process exposing the bundled `animals` package:

```bash
sen run components/jsonrpc/clients/typescript/examples/browser/animals.yaml
```

That spins up `rufus` (a `Cat`) and `elon` (a `Dog`) on `my.tutorial`. Only the cat matches the
query below; the dog is there so the interest has something to filter out. The JSON-RPC server
listens on `ws://127.0.0.1:8080`.

In a second terminal:

```ts
import { connect } from "@sen/client";

const client = await connect({ url: "ws://127.0.0.1:8080" });

const cats = await client.declareInterest({
  name: "cats",
  query: "SELECT animals.Cat FROM my.tutorial",
});

cats.onObjectAdded(async (cat) => {
  console.log(`got ${cat.className} named ${cat.name}`);
  cat.onPropertyChanged("position", (pos) => console.log("position:", pos));
  await cat.awaitPropertySubscribed("position");
  await cat.invoke("jumpToLocation", { x: 42, y: 17 });
});
```

You should see the initial position print, then the post-jump position. The
`onObjectAdded` handler fires uniformly for objects matched at declare-time and for late
arrivals.

`onPropertyChanged` returns as soon as it has registered the handler, while the subscribe
it triggers travels to the server and back. Without the `awaitPropertySubscribed` line the
jump can land first, and then the only value you see is the one after the jump. Use the
same pattern for events, where the partner call is `awaitEventSubscribed`.

## Concepts

The handles are layered:

- **`Client`** -- the connection. One per Sen process. Owns the WebSocket, hands out
  interests, surfaces connection-state events.
- **`InterestHandle`** -- a live query. Keeps a local cache of matching objects in sync
  with the server. Tells you when objects appear or disappear.
- **`ObjectHandle`** -- one object in an interest's match set. The unit of work for
  property reads/writes, event subscriptions, method calls.

You only ever construct a `Client`. Interests and objects come from it.

## Full reference

The package's
[README](https://github.com/airbus/sen/blob/main/components/jsonrpc/clients/typescript/README.md) is
the comprehensive user reference, covering:

- Queries + pre-subscribing properties / events at declare time
- Property reads, writes, and subscriptions (and `onAnyChange`)
- Events (`onEventTriggered`)
- Method invocation (`invoke`)
- Runtime value types (`Quantity`, `Variant`) + narrowing helpers
- Error model (`JsonRpcError`, `TransportError`, `TimeoutError`)
- Reconnection + `reestablishAll`
- Batch reads (`getObjectsBatchState`) for dashboard-style refresh
- React bindings (`@sen/client/react`): `useObject`, `useObjects`, `useProperty`,
  `useEvents`, `useInvoke`, `useSetProperty`, `useTopology`, `useTypeSpec`, and the
  `makeStore` / `useCell` primitives for fine-grained subscriptions.

## Browser example

[`examples/browser/`](https://github.com/airbus/sen/tree/main/components/jsonrpc/clients/typescript/examples/browser)
in the package is a static HTML page that loads the built bundle and runs the hello-world above in a
real browser. Use it as a sanity check after `npm run build`, or as a starting point for a custom
browser UI.

## Architecture

For contributors: see
[`components/jsonrpc/clients/typescript/architecture.md`](https://github.com/airbus/sen/blob/main/components/jsonrpc/clients/typescript/architecture.md)
for the layer diagram, per-module responsibilities, key invariants, and extension points.
