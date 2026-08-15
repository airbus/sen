# Web Explorer Frontend -- Architecture

One-page mental model for contributors. For the developer setup see [`README.md`](README.md);
for the user-facing landing page see [`docs/components/webexplorer.md`](../../../docs/components/webexplorer.md).

## Purpose

A browser UI that talks to a running Sen process over the jsonrpc component's WebSocket
wire. The user navigates buses, declares named queries against them ("interests"), pins
objects into a Cockpit, plots numeric leaves over time, and watches events stream in. The
UI ships as a single self-contained `dist/index.html` that the `webexplorer` backend bakes
into the Sen binary and serves at `/explorer`, same origin as the JSON-RPC endpoint.

## React in 60 seconds (for non-React readers)

- A **component** is a function that returns a description of DOM (`<div>...</div>`-style
  syntax, called JSX). The framework re-runs it whenever the data it reads changes; the
  return value is diffed against the previous one and only the differences are pushed to
  the real DOM.
- **Hooks** are the only sanctioned way to read external data, hold state, or run side
  effects inside a component. They look like function calls (`useState`, `useEffect`,
  `useObject`, ...) but they wire the component up to a re-render trigger.
- **Props** are the inputs a parent passes to a child. **State** is the data a component
  owns. **Refs** hold values that survive re-renders but don't trigger them.
- A component's output exists in a single in-memory tree (the **React tree**), separate
  from the DOM the user sees. The tree spans the main window and any popped-out browser
  windows; the DOM lives in whichever document its parent points to.
- **Re-render** is cheap; the framework's job is to keep it cheap. **Memoization**
  (`useMemo`, `React.memo`, store selectors) is how we tell it which work is safe to skip
  between renders.

That is the full surface used here. No class components, no Redux, no Next.js, no SSR.

## Layers

```text
+-----------------------------------------------------------+
| Workspaces        Cockpit, Plots, Events, Object Explorer |
+-----------------------------------------------------------+
| Widgets           value rendering, interest popovers,     |
|                   property editors, chart wrapper         |
+-----------------------------------------------------------+
| State stores      module-scoped state (interests, plot    |
|                   board, watches, events, UI prefs),      |
|                   localStorage persistence                |
+-----------------------------------------------------------+
| @sen/client       Client / InterestHandle / ObjectHandle, |
|                   plus React hooks (useObject, useProperty, |
|                   useEvents, ...). Owns the WebSocket and  |
|                   the per-connection subscription registry.|
+-----------------------------------------------------------+
| Browser           one WebSocket, one main document, zero  |
|                   or more popped-out documents            |
+-----------------------------------------------------------+
```

Data flows up; control flows down. A property change on the wire becomes a notification on
the Client, which fans out through the subscription registry, into the `useProperty` hook,
which re-renders the components reading that hook. A user click in a workspace dispatches
through a store action, which updates module-scoped state, which any subscribing hook
picks up on its next selector run.

## Directory map

```text
src/
  app.tsx, main.tsx       application shell, connection lifecycle
  core/                   pure helpers (keys, time, formatting, panel kinds)
  state/                  module-scoped stores + their localStorage persistence
  ui/                     visual primitives (buttons, layout, tooltip, modal, ...)
  components/             workspace-sized React trees (Plots, Cockpit, Events, ...)
  widgets/                composable sub-trees the workspaces stitch together
    interests/            bus tree, query authoring popover
    value_edit/           in-place editors for primitive / struct / sequence values
    value_render/         the renderer that turns a `Var` into DOM
  theme/                  CSS tokens + dark/light variants
  assets/                 SVGs, icons baked into the bundle
```

The split is by *layer* (primitives -> composites -> workspaces), not by feature. Most
features cut across all three.

## Key design decisions

- **Domain state lives in `@sen/client`, UI state lives in React.** The Client holds
  interests, type specs, object handles, subscription state. React only owns "which tab is
  selected, what's pinned, where the user dragged the divider." If a fact about a Sen
  object can be re-derived from the wire, do not cache it in React state.
- **Module-scoped stores via `@sen/client/react`'s `makeStore` / `useSelector`.** Each
  state module exports a store at module scope (one per `import`). React mounts and
  unmounts come and go; the store survives. This is what lets the popped-out window see
  the same Cockpit selection as the main window without prop-drilling.
- **Single React tree, multiple documents.** Popped-out browser windows render via
  `createPortal` into a `window.open` document. The React tree stays one tree, so the
  popout sees the same WebSocket, the same hooks, the same store state. The DOM is in a
  different document; everything else is shared.
- **One WebSocket per browser session.** Declared interests, subscriptions, and the type
  cache are connection-scoped. Re-using one connection avoids the kernel paying for the
  same subscription twice when the user pops a workspace out.
- **Transport auto-reconnects; interests re-declare automatically.** The app relies on
  `@sen/client`'s `autoReestablish` default -- no explicit `onReconnect` wiring, deliberately:
  a second manual `reestablishAll()` would race the built-in one. Without re-establishment
  the socket would come back but every workspace would be looking at dead handles.
- **Layout persisted to `localStorage` with versioned envelopes.** Anything the user
  arranged (panel layouts, column widths, drawer heights, retention) round-trips through a
  `{ version, payload }` JSON envelope. A future shape change bumps the version and
  ignores the old payload rather than crashing on it.
- **Server timestamps for time axes and event rows.** Plot x-axes and event-row "at" times
  come from `info.timestamp` on the wire delivery, never `Date.now()`. The browser clock
  is a UI clock; the Sen clock is the truth.
- **Schema-agnostic rendering.** No type, class, property, or unit is hardcoded. The value
  renderer walks the live `Var` and the live `TypeSpec` from `client.getType`. A new Sen
  class lights up in the UI on the next connect.
- **Production ships as one HTML file.** `vite-plugin-singlefile` inlines every JS and CSS
  asset into `dist/index.html`. The backend bakes that file into the Sen binary at compile
  time (`sen_file_to_cpp`) and serves it through the jsonrpc `StaticFileServer`. No CDN,
  no telemetry, no external network calls.

## Third-party choices

- **gridstack** for the Plots and Cockpit grid layouts: drag-to-rearrange, drop-to-resize,
  consistent column counts. Pinned to an exact version because we wrap a small number of
  its private behaviors; an asserted-at-import check fails loud if a private method
  renames. Drag/resize is disabled when the host document is not the main document (a
  popped-out grid is layout-locked; the user rearranges in the main window).
- **uPlot** for time-series rendering. Cheap, fast, render-on-update. We feed it server
  timestamps and our own retention-trimmed sample buffers; the chart itself is dumb.
- **react-virtuoso** for the events table virtualization. Required because event tables
  can grow to tens of thousands of rows before the retention trim catches up.
- **@floating-ui/react** for popovers (settings, source-filter, query-author). Handles
  the placement / collision / focus-trap concerns we do not want to reinvent.

## Extension points

- **New workspace tab.** Add the tab descriptor to the bottom-pane tabs memo in
  `app.tsx`. The pane itself is a normal component; it only needs to accept the standard
  workspace props (the active `Client`, the user's current selection if relevant).
- **New persisted UI preference.** Define a store with `makeStore`. Persist by reading
  `localStorage` once at module init (envelope: `{ version, payload }`), and writing on
  setState. The other persisted stores are the templates to follow.
- **New value-rendering case.** Extend the renderer in `widgets/value_render/`. The
  renderer walks `Var` + `TypeSpec`; adding a case is local to one switch.
- **New event filter / per-type events table.** The events drawer hosts multiple
  per-type tables driven by a tabs store. Adding a tab is a row in the tab store + a
  consumer of the existing per-type table component.
- **New first-class chart kind.** Add a `PanelKind` to `core/panels.ts`, teach the plot
  panel to render it, teach the value renderer's `+ plot` button to offer it for the
  matching leaf types.

## Tests

- `test/state/*.test.ts` -- vitest unit tests against the state modules (sample buffer,
  event store, watch + plot, color identity).
- No end-to-end harness. The development loop is `npm run dev` against a running Sen and
  manual interaction. Behavior tests live in `@sen/client`'s integration tests, which
  exercise the same wire and the same hooks against a real spawned kernel.

## Build

- `npm install` -- installs dependencies. `@sen/client` is a `file:` dep that requires the
  CMake build of the Sen tree to have run at least once (the TS client's generated code
  lands during that build).
- `npm run dev` -- Vite dev server with HMR. The hook layer survives module reloads; the
  module-scoped stores reseed from localStorage.
- `npm run build` -- production bundle. `vite-plugin-singlefile` inlines everything into
  one `dist/index.html`. The CMake target `webexplorer_generated` runs this and bakes the
  result into the `webexplorer` C++ component via `sen_file_to_cpp`.
- `npm run typecheck` -- `tsc --noEmit` across the whole tree.
- `npm test` -- vitest, unit only.
