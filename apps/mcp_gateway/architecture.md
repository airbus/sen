# mcp_gateway -- Architecture

One-page mental model for contributors. For end-user docs see
[`docs/components/mcp_gateway.md`](../../docs/components/mcp_gateway.md).

## Purpose

A Node.js MCP server that an LLM host spawns over stdio. Receives MCP tool / resource
requests, translates them into `@sen/client` calls against one or many Sen kernels, and
returns the results. Also exposes an offline-recording analysis path: spawns a sandboxed
python child that imports `sen_db_python` and runs caller-supplied scripts.

## Process model

- **One gateway process per host conversation.** The MCP host launches the gateway over
  stdio; when the conversation ends the host closes the pipe and the gateway exits.
- **Zero, one, or many Sen kernels per gateway.** Kernels are registered by caller-chosen
  names via `connectToKernel`; each `Kernel` owns its own `Client` and per-interest state.
- **At most one python child per `runRecordingScript` call.** A `RecordingRunner` rejects
  scripts over 64 KiB, spawns the child, enforces stdout/stderr/wall-clock caps, and returns a
  structured `RunResult`. Concurrency is capped via a `Semaphore`. Killing escalates
  SIGTERM -> SIGKILL over the child's process group on POSIX, and walks the process tree on
  Windows, which has no groups. A run settles on the child's exit rather than on stdio EOF, so
  a grandchild that outlives it cannot hold a semaphore slot open.

## Layers

```text
+---------------------------------------------------------------+
| MCP transport       stdio framing (provided by @mcp/sdk)      |
+---------------------------------------------------------------+
| Tool dispatcher     bootstraps server, routes CallTool by     |
|                     name to a handler, wraps thrown errors    |
+---------------------------------------------------------------+
| Tool implementations  one handler per registered MCP tool     |
|                       validates args, resolves the kernel,    |
|                       calls @sen/client, formats response,    |
|                       audits state-changing calls             |
+---------------------------------------------------------------+
| Kernel surface      Kernel + KernelRegistry                   |
|                     wraps @sen/client.Client, tracks open     |
|                     interests + event buffers, holds the      |
|                     AbortController for shutdown              |
+---------------------------------------------------------------+
| Side surfaces       RecordingRunner (spawn + sandbox)         |
|                     AuditSink (file or stderr fallback)       |
|                     resources (sen:// baked docs)             |
+---------------------------------------------------------------+
```

## Source layout

`src/` is flat, with each layer above in its own module. Grep for the surface keyword to find
one: `KernelRegistry`, `RecordingRunner`, `AuditSink`, `makeKernelTools`. Two subdirectories:

- **`src/util/`** -- small library-style helpers used across layers (ring buffer for the
  event buffers, semaphore for the recording-runner concurrency cap, an env-allowlist pass).
  Covered by `test/unit/` directly so changes there don't need to wait for an integration
  spin-up.
- **`src/baked_docs.ts`, `src/baked_package.ts`** -- generated at build time. The bake
  script (`scripts/bake_docs.mjs`) reads the curated `docs/` subset and `package.json`,
  emits TypeScript modules into `src/`, and is invoked from the `prebuild` npm hook. The
  generated files are gitignored; to change what the LLM sees, edit the bake script's
  manifest.

The install ships two launchers from `bin/`: `sen-mcp-gateway` for POSIX and
`sen-mcp-gateway.cmd` for Windows, whose MCP hosts cannot spawn a shell script. Each resolves
`dist/sen-mcp-gateway.cjs` next to itself and rejects a missing `node`, or one older than
22.20.0, with a message. The bundle targets node22, so an older runtime otherwise fails
somewhere inside a bundled dependency with nothing naming the version. The check is a floor;
newer is allowed. The Docker image lives in `docker/`.

## Key invariants

- **One thread of control.** Node single-threaded; the only concurrency is the python child
  in `RecordingRunner` and `await`-suspended tool handlers. The `Semaphore` caps how many
  recording scripts can run at once.
- **Abort propagates through.** Each `Kernel` owns an `AbortController`; every
  `@sen/client` call carries its signal. Shutdown aborts the controller first, so in-flight
  RPCs reject promptly with a structured error instead of hanging on the closing socket.
- **Audit log never blocks state-changing tools.** A write failure falls back to stderr
  with a one-shot warning; the tool still returns success. Auditing is observability, not
  a precondition.
- **Recording sandbox is best-effort, not adversarial.** The env allowlist + caps + SIGKILL
  escalation defend against accident, not malice. Real isolation requires running the
  gateway in a container (the security section of the user-facing doc covers this).
- **Read-only withdraws the recording group.** `makeRecordingTools` returns `[]` under
  read-only rather than gating each handler: `runRecordingScript` runs caller-supplied Python,
  which no per-call check can constrain the way `setProperty` is constrained. A new recording
  tool inherits this by construction; one added to a different factory would not.
- **Baked docs are the only doc source.** The gateway has no runtime read of `docs/`; the
  build bakes a curated subset into `src/baked_docs.ts`. Editing the live docs alone does
  not update what the LLM sees. The bake script's manifest is the gate.

## Extension points

- **New MCP tool.** Add a `Tool` entry in the relevant `make*Tools` factory (kernel tools,
  recording tools), or introduce a new factory if the surface is conceptually distinct. The
  dispatcher registers everything those factories return.
- **New resource.** Add the file to the baker manifest in `scripts/bake_docs.mjs`; the next
  build pulls it in. The baked-docs unit tests under `test/unit/` cover the URI shape.
- **New audit field.** Extend the `AuditEntry` payload at the call site. The sink is
  structurally typed; it accepts any object.
- **Alternative kernel source.** Today every `Kernel` wraps a WebSocket `Client`. The
  registry's `connect` path could grow alternative transports (e.g. in-process kernels for
  tests) by introducing a transport seam on the `Kernel` constructor.

## Tests

- `test/unit/*.test.ts` -- module-scoped, no spawned processes (mock spawn, mock audit
  sink, mock `@sen/client`). These exercise the helpers and the pure logic in the tool
  handlers.
- `test/integration/*.test.ts` -- spawn a real Sen subprocess and the real gateway process
  over stdio. Cover the live-data tools end-to-end and the recording-tools mechanics,
  including the kernel-disconnect / SIGTERM-mid-call races and a `runIf(haveRealRecording)`
  block that hits a real `sen_db_python` recording when `RECORDING_ROOT` env points at one.

## Build + packaging

- `npm run build` -- esbuild produces `dist/sen-mcp-gateway.cjs` + sourcemap.
- `npm run typecheck` -- `tsc --noEmit` against both the prod tsconfig and the test tsconfig.
- `npm test` -- vitest, unit only.
- `npm run test:integration` -- vitest, integration; needs `GATEWAY_ENTRYPOINT`,
  `SEN_BINARY`, `SEN_CONFIG` env vars.
- `apps/mcp_gateway/CMakeLists.txt` installs the `.cjs` + the `bin/sen-mcp-gateway` wrapper
  to `${CMAKE_INSTALL_PREFIX}/bin/`. Conan picks them up via `cmake.install()`.
