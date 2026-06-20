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

const defaultStartupTimeoutMs = 10_000;
const defaultShutdownGraceMs = 2_000;

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

  try {
    await waitForPort(opts.port, opts.startupTimeoutMs ?? defaultStartupTimeoutMs);
  } catch (err) {
    child.kill("SIGKILL");
    throw new Error(`${String(err)}\nSen stderr so far:\n${Buffer.concat(stderrChunks).toString()}`);
  }

  return {
    process: child,
    port: opts.port,
    async stop({ graceMs = defaultShutdownGraceMs } = {}) {
      if (child.exitCode !== null) return;
      if (process.platform === "win32") {
        // Windows has no real SIGINT; child.kill() invokes TerminateProcess.
        child.kill();
        return;
      }
      child.kill("SIGINT");
      const exited = await Promise.race([
        once(child, "exit").then(() => true),
        new Promise<boolean>((r) => setTimeout(() => r(false), graceMs)),
      ]);
      if (!exited) {
        child.kill("SIGKILL");
      }
    },
  };
}
