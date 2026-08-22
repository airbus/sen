# @sen/mcp-gateway

A [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server that exposes a
running [Sen](https://airbus.github.io/sen/latest/) kernel, or several at once, to an
LLM host. Spawned over stdio by the host, it connects to each kernel over JSON-RPC +
WebSocket and re-publishes the kernel's introspection (sessions, buses, types, properties,
methods, events) as MCP tools and resources.

This file is the cheatsheet for working on the gateway: where things live and how to build and
test them. [`architecture.md`](architecture.md) covers the design, and
[`docs/components/mcp_gateway.md`](../../docs/components/mcp_gateway.md) is the user-facing
page, rendered in the guide as *MCP Gateway*.

## Source layout

- `src/` -- TypeScript implementation. `index.ts` is the stdio entrypoint; everything else
  is named after the surface it implements (kernel registry, tool factories, audit sink,
  recording runner, resources, errors, instructions, util helpers). `src/baked_docs.ts` and
  `src/baked_package.ts` are build-time generated and gitignored.
- `bin/sen-mcp-gateway`, `bin/sen-mcp-gateway.cmd` -- install launchers for POSIX and Windows.
  Each resolves the bundled `.cjs` next to itself, checks that `node` is present and at least
  22.20.0, and runs it. Both are installed on every platform.
- `scripts/bake_docs.mjs` -- prebuild hook that bakes the curated `docs/` subset and
  `package.json` into the two generated TS modules above.
- `test/unit/` -- mocked, module-scoped vitest specs (no spawned processes).
- `test/integration/` -- spawn a real Sen subprocess and the real gateway over stdio;
  cover the live-data tools end-to-end and the recording-tools mechanics.
- `docker/Dockerfile` -- reference image for sandboxed deployment.

## Development

```bash
npm ci                       # install pinned deps (Node 22; package.json pins ^22.20.0)
npm run build                # bakes docs + esbuild -> dist/sen-mcp-gateway.cjs (+ .map)
npm run typecheck            # bakes docs + tsc --noEmit on src/ and test/
npm test                     # vitest unit tests
npm run test:integration     # vitest spawning Sen + the gateway (needs SEN_BINARY + SEN_CONFIG)
```

`npm run build` invokes the `prebuild` hook automatically, which runs `npm run bake` to
regenerate `src/baked_docs.ts` from `docs/users_guide/`, `docs/components/`, and selected
`docs/howto_guides/`. The generated file is gitignored.

## Inside a Sen build

The gateway builds when `jsonrpc` is on and `SEN_BUILD_MCP_GATEWAY` is `ON`, which is the
default. It also needs the JSON-RPC TypeScript client, so `-DSEN_BUILD_JSONRPC_TS_CLIENT=OFF`
is rejected at configure time unless the gateway is off as well. A build that finds no `npm`
on `PATH` skips the gateway and says so in the CMake output.
`apps/mcp_gateway/CMakeLists.txt` wires the npm flow into ctest: the bundle is rebuilt
whenever the TS source, the bake script, or any of the baked doc folders change, and
`mcp_gateway_integration` spawns a Sen subprocess against
`test/integration/configs/inheritance.yaml`. The wrapper + bundle are installed via
`install(PROGRAMS/FILES ... COMPONENT mcp_gateway)`.

When making changes that touch user-visible behaviour, env variables, or the tool surface,
update [`docs/components/mcp_gateway.md`](../../docs/components/mcp_gateway.md) in the same
commit. The user-facing doc is the canonical source for the configuration table and the
security model.
