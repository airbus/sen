# @sen/web-explorer

Browser UI for inspecting and interacting with a running Sen process over its JSON-RPC
WebSocket. React + TypeScript, no server-side rendering, no external network calls. The
production build is a single self-contained `dist/index.html` that the `webexplorer` Sen
component bakes into its `.so` and serves at `/explorer` from the same origin as the
JSON-RPC endpoint.

What you can do with it:

- Browse buses, declare named queries against them, pin matching objects
- Inspect any object's properties live; edit writable ones in place
- Invoke methods with typed argument editors
- Watch numeric leaves on a plot board (multi-series, pan, zoom, retention-bounded)
- Watch events stream into a per-class table, filter by source, pop tables out

## Develop

In one terminal, start a Sen instance with JSON-RPC enabled. Any example that loads the
`jsonrpc` and `webexplorer` components works; the school + jsonrpc combo at
`examples/config/14_jsonrpc/3_explorer.yaml` is a good minimal target:

```bash
sen run examples/config/14_jsonrpc/3_explorer.yaml
```

In another, run the Vite dev server (with HMR):

```bash
cd components/web_explorer/frontend
npm install
npm run dev
```

Open `http://localhost:5173`. The app auto-connects to the WebSocket; there is no manual
connection bar. In dev (`import.meta.env.DEV`) the URL defaults to `ws://127.0.0.1:8080`;
in production the page trusts its own origin.

## Build the production bundle

```bash
npm run build      # writes dist/index.html, single self-contained file
npm run preview    # serves the production bundle locally for a smoke check
```

`dist/index.html` is what the `webexplorer` C++ component bakes into its shared library
via `sen_file_to_cpp`. A `cmake --build` of the Sen tree runs the npm build automatically
and refreshes the bake. After editing TypeScript and rebuilding the C++ target, restart
the running Sen so the new bytes load.

## Serving the bundle via `jsonrpc`'s static file server

The build artifact (single self-contained HTML, all assets inlined) is shaped to ride on
top of the `jsonrpc` component's static file server. At runtime, when the `jsonrpc` and
`webexplorer` components are both loaded, `webexplorer` watches the jsonrpc control bus
(`local.jsonrpc_control` by default), gets a handle to the `StaticFileServer` object, and
calls `registerStaticBundle` with the baked bytes under `urlPrefix = "/explorer"`. The
same HTTP listener that serves the WebSocket then serves the SPA at `/explorer/`, so a
browser-side client loaded from `http://host:8080/explorer/` can open a WebSocket to
`ws://host:8080` without crossing origins.

The pattern is general: any single-file static frontend that builds against `@sen/client`
can ship through `jsonrpc`'s static file server the same way. Build the bundle, bake it
into your component's `.so` with `sen_file_to_cpp`, and register it on the control bus.
See [`../backend/README.md`](../backend/README.md) for what the registration code looks
like end-to-end.

## Other commands

```bash
npm run typecheck  # tsc --noEmit
npm test           # vitest, unit
npm run test:watch # vitest in watch mode
```

## Architecture

See [`architecture.md`](architecture.md) for the contributor-facing mental model: layering,
key design decisions, extension points. The user-facing landing page (rendered into mkdocs)
lives at [`docs/components/web_explorer.md`](../../../docs/components/web_explorer.md).
