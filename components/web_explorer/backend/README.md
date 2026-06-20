# webexplorer (backend)

The `webexplorer` Sen component bakes the production frontend bundle into its shared
library at build time and registers it with the `jsonrpc` component's static file server
at runtime. Together with `jsonrpc` it serves the Sen Web Explorer at `/explorer` on the
same origin as the JSON-RPC WebSocket endpoint.

The component itself is small: one `run()` method that watches the jsonrpc control bus
for a `StaticFileServer`, calls `registerStaticBundle` on the first one it sees, and
unregisters when the server is detached. The frontend bytes travel inside `libwebexplorer.so`;
no on-disk files, no separate web server.

## Config

```yaml
load:
  - name: jsonrpc
    address: "0.0.0.0"
    port: 8080
  - name: webexplorer
    # controlBusName: jsonrpc_control   # optional; defaults to "jsonrpc_control"
```

`controlBusName` selects the jsonrpc control bus to subscribe to on `local.`. The default
matches `jsonrpc`'s own default, so the option is only useful if you have renamed the
control bus on the `jsonrpc` side.

## Behavior

- Subscribes to `local.<controlBusName>` for `StaticFileServer` objects.
- On the first match: decompresses the baked frontend bundle, builds a `StaticBundle` with
  `urlPrefix = "/explorer"` and `indexFileName = "index.html"`, calls
  `registerStaticBundle` on the server.
- If a second `StaticFileServer` appears on the same bus: logs an error and ignores it.
  Multiple servers would make the served bundle non-deterministic across reloads.
- On detach: drops the cached server pointer. A future server reappearing re-registers.

There is no per-tick work; the component runs a slow (0.1 Hz) keepalive loop only to hold
the kernel frame open while subscription callbacks fire.

## Build dependency

The CMakeLists wires `webexplorer` against the frontend bundle: the npm production build
(`components/web_explorer/frontend/npm run build`) produces `dist/index.html`; the
`webexplorer_bundle` CMake target depends on that file; `sen_file_to_cpp` compresses it
into a C byte array; the component decompresses at runtime via
`sen::decompressSymbolToString`. A `cmake --build` of the Sen tree runs the npm build
automatically.

After editing the frontend, rebuild the `webexplorer` target (or the whole Sen tree) and
restart the running Sen so the new shared library loads.

## See also

- [`../frontend/architecture.md`](../frontend/architecture.md) -- the React app's
  contributor-facing mental model.
- [`../frontend/README.md`](../frontend/README.md) -- frontend developer setup, including
  the Vite dev loop that bypasses the bake during iteration.
- `docs/components/jsonrpc.md` -- the static file server's public surface.
