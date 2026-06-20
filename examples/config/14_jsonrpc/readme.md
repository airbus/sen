# JSON-RPC configs

Sen instances that load an example package and expose it over JSON-RPC on `ws://0.0.0.0:8080`.
Useful for poking at the JSON-RPC surface manually (with `wscat`, the browser DevTools, etc.).

| File | Package | Shape | Best for |
| ---- | ------- | ----- | -------- |
| `1_fibonacci.yaml` | `fibonacci` | one Manager + three Workers on one bus | event-driven flows; work dispatched by Manager, results surfaced as events |
| `2_animals.yaml` | `animals` | one Cat + one Dog | the simplest surface; small class hierarchy + method with arguments (`Cat::jumpToLocation`) |
| `3_explorer.yaml` | (jsonrpc only) | jsonrpc + web_explorer side by side | smoke-testing the web_explorer backend; `curl http://127.0.0.1:8080/explorer/` returns the bundled HTML |

The `@sen/client` integration suite uses its own YAML fixture under
`components/jsonrpc/clients/typescript/test/integration/configs/`; it is not part of this tree.

## Running one

From the build directory:

```bash
./bin/sen run ../../examples/config/14_jsonrpc/1_fibonacci.yaml
```

Then connect with anything that speaks JSON-RPC 2.0 over WebSocket. The `@sen/client` TS library
is the canonical consumer; see `components/jsonrpc/clients/typescript/README.md` for a quick tour.

## Why no shell

These configs deliberately omit `../base/shell.yaml` (which most other example configs include).
The audience is automated clients connecting over the wire; an interactive shell on stdin gets in
the way of subprocess-based test runners. Add it back locally if you want one-shot manual
inspection.
