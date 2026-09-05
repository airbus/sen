// === subprocess.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Shared spawn/teardown logic for Sen subprocesses. Used by globalSetup.ts for the suite-wide
// Sen and by tests that need to manage their own Sen (notably the reconnect test, which needs
// to kill and respawn one mid-test on a separate port).

import { spawn, type ChildProcess } from "node:child_process";
import { existsSync } from "node:fs";
import { Socket } from "node:net";
import { once } from "node:events";

// Startup is the whole of it: binding the port, then serving an upgrade. An instrumented build
// takes far longer over the second than a plain one, and the probe below returns as soon as the
// server answers, so this ceiling only costs time when something is actually wrong.
const defaultStartupTimeoutMs = 30_000;
const defaultShutdownGraceMs = 2_000;
// Per attempt, not for the whole wait: a server that is bound but not serving accepts the
// connection and then says nothing, so an attempt has to be abandoned to make room for the next.
const probeAttemptMs = 1_000;

// child.kill() terminates one process. On Windows the kernel's children inherit both the
// listening socket and the stdio pipes, so killing only the parent leaves the port bound and
// the pipes open -- the port wait then times out, and whatever is reading those pipes waits
// for a writer that never goes. taskkill walks the tree; POSIX has the process group instead.
function killTree(child: ChildProcess, signal: NodeJS.Signals): void {
  const pid = child.pid;
  if (pid === undefined || child.exitCode !== null || child.signalCode !== null) return;
  if (process.platform === "win32") {
    const killer = spawn("taskkill", ["/pid", String(pid), "/t", "/f"], { stdio: "ignore" });
    killer.on("error", () => child.kill(signal));
    killer.unref();
    return;
  }
  child.kill(signal);
}

export interface SpawnSenOptions {
  binary: string;
  config: string;
  port: number;
  startupTimeoutMs?: number;
}

export interface SenHandle {
  readonly process: ChildProcess;
  readonly port: number;
  stop(opts?: { graceMs?: number }): Promise<void>;
}

export async function waitForPort(port: number, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let lastErr: unknown;
  while (Date.now() < deadline) {
    try {
      await new Promise<void>((resolveProbe, rejectProbe) => {
        const socket = new Socket();
        socket.once("error", (err) => {
          socket.destroy();
          rejectProbe(err);
        });
        socket.once("connect", () => {
          socket.end();
          resolveProbe();
        });
        socket.connect(port, "127.0.0.1");
      });
      return;
    } catch (err) {
      lastErr = err;
      await new Promise((r) => setTimeout(r, 100));
    }
  }
  throw new Error(
    `Sen did not open port ${port} within ${timeoutMs}ms. Last connect error: ${String(lastErr)}`,
  );
}

function closeQuietly(socket: WebSocket): void {
  try {
    socket.close();
  } catch {
    // closing a socket that never opened is not an error worth reporting
  }
}

// A connect proves the port is bound, not that anything is serving: the TCP handshake completes
// from the listen backlog before the component accepts. Waiting on the port alone starts the suite
// against a server that is not answering, and the first client pays for it out of its own budget.
export async function waitForWebSocket(port: number, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let lastErr: unknown;
  while (Date.now() < deadline) {
    try {
      await new Promise<void>((resolveProbe, rejectProbe) => {
        const socket = new WebSocket(`ws://127.0.0.1:${port}`);
        const timer = setTimeout(() => {
          closeQuietly(socket);
          rejectProbe(new Error(`no upgrade within ${probeAttemptMs}ms`));
        }, probeAttemptMs);
        socket.addEventListener("open", () => {
          clearTimeout(timer);
          closeQuietly(socket);
          resolveProbe();
        });
        socket.addEventListener("error", () => {
          clearTimeout(timer);
          closeQuietly(socket);
          rejectProbe(new Error("upgrade refused"));
        });
      });
      return;
    } catch (err) {
      lastErr = err;
      await new Promise((r) => setTimeout(r, 100));
    }
  }
  throw new Error(
    `Sen did not serve a WebSocket upgrade on port ${port} within ${timeoutMs}ms. ` +
      `Last probe error: ${String(lastErr)}`,
  );
}

export async function waitForPortClosed(port: number, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const open = await new Promise<boolean>((resolveProbe) => {
      const socket = new Socket();
      socket.once("error", () => {
        socket.destroy();
        resolveProbe(false);
      });
      socket.once("connect", () => {
        socket.end();
        resolveProbe(true);
      });
      socket.connect(port, "127.0.0.1");
    });
    if (!open) return;
    await new Promise((r) => setTimeout(r, 50));
  }
  throw new Error(`Port ${port} did not close within ${timeoutMs}ms.`);
}

export async function spawnSen(opts: SpawnSenOptions): Promise<SenHandle> {
  if (!existsSync(opts.binary)) {
    throw new Error(
      `Sen binary not found at ${opts.binary}. Build the 'sen' target (cmake --build <build-dir> --target sen).`,
    );
  }
  if (!existsSync(opts.config)) {
    throw new Error(`Sen config not found at ${opts.config}.`);
  }

  const child = spawn(opts.binary, ["run", opts.config], {
    stdio: ["ignore", "pipe", "pipe"],
  });

  // Capture stderr so a port-open timeout error includes the server's logs instead of just
  // ECONNREFUSED. Bounded by the startup window of normal log output.
  const stderrChunks: Buffer[] = [];
  child.stderr?.on("data", (chunk: Buffer) => stderrChunks.push(chunk));

  // One budget across both phases, so a slow bind eats into the wait for a served upgrade rather
  // than granting a second full one. Whichever phase runs out names itself in the error.
  const deadline = Date.now() + (opts.startupTimeoutMs ?? defaultStartupTimeoutMs);
  try {
    await waitForPort(opts.port, deadline - Date.now());
    await waitForWebSocket(opts.port, deadline - Date.now());
  } catch (err) {
    killTree(child, "SIGKILL");
    throw new Error(`${String(err)}\nSen stderr so far:\n${Buffer.concat(stderrChunks).toString()}`);
  }

  return {
    process: child,
    port: opts.port,
    async stop({ graceMs = defaultShutdownGraceMs } = {}) {
      if (child.exitCode !== null) return;
      // Wait for the process to actually go: returning once the signal is delivered
      // leaves the listening socket held, and a caller waiting for the port to close
      // sees it still open.
      killTree(child, "SIGINT");
      const exited = await Promise.race([
        once(child, "exit").then(() => true),
        new Promise<boolean>((r) => setTimeout(() => r(false), graceMs)),
      ]);
      if (!exited) {
        killTree(child, "SIGKILL");
        await Promise.race([
          once(child, "exit"),
          new Promise((r) => setTimeout(r, graceMs)),
        ]);
      }
    },
  };
}
