# Sen reactive store primitives

Small reactive-state layer for `@sen/client/react`. Two flavors covering high-frequency
mutable backing (sample buffers, event logs) and ordinary replaced-on-update state
(selection, prefs, layout).

```ts
import {
  makeStore, makeBufferStore, makeBufferCell,
  useStore, useSelector, useView, useViews, useCell,
} from "@sen/client/react";
```

## `makeStore<T>`

Immutable replacement on update. `setState((prev) => next)`; if `next` is `Object.is`-equal
to `prev`, no listener fires.

`useStore(store)` returns the whole state. `useSelector(store, sel)` re-renders only when
`sel`'s output changes by `Object.is`; return primitives, stable refs, or pre-cached
derived values on the state object.

## `makeBufferStore<K, V>`

Per-key cached views over consumer-owned mutable backing. The consumer pushes into its
own data structure and calls `invalidate(key)`; the store flushes notifications coalesced
per `requestAnimationFrame` (pass `rafBatch: false` for synchronous flush in tests).

```ts
const store = makeBufferStore<PlotKey, BufferView>({
  produce: (key) => viewOf(backing.get(key)) ?? EMPTY_VIEW,
});

pushToBuffer(buf, sample);
store.invalidate(key);

// React:
const view = useView(store, key);   // single key
const views = useViews(store, keys); // multi
```

`subscribeKey(key, fn)` fires only on that key. `subscribeAll(fn)` fires once per flush.
`dropKey(key)` clears the cache entry silently (use on eviction / unsubscribe).

## `makeBufferCell<T>`

Single-key sugar over `makeBufferStore`. `invalidate()` with no key, `useCell(cell)` to
read.

## Mutable-backing gotcha

The view returned from a buffer store wraps **live mutable data**. Reading per render or
in a draw hook is safe. Stashing it in a ref to "freeze this moment" is not; later
backing mutations will alter the data behind the ref. If you need a snapshot, copy at the
boundary:

```ts
if (paused && ref.current === null) ref.current = liveEvents.slice();
```

## Conventions

Actions are plain TS functions closing over the store: stable identity, no memoization
needed. Selectors live alongside the store and return primitives or cached refs. Stores
are module-scoped values; a Provider is only useful when the store has lifecycle (e.g. a
retention-trim timer) or per-tree isolation is required.
