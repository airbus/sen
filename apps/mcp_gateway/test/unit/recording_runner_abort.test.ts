// === recording_runner_abort.test.ts ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { RecordingRunner, RecordingRunnerInputTooLargeError } from "../../src/recording_runner.js";

describe("RecordingRunner abort", () => {
  it("returns immediately with aborted:true when the signal is pre-aborted", async () => {
    const runner = new RecordingRunner({ timeoutMs: 10_000 });
    const ac = new AbortController();
    ac.abort();
    const result = await runner.run("import time\ntime.sleep(5)\n", { signal: ac.signal });
    expect(result.aborted).toBe(true);
    expect(result.spawnFailed).toBe(false);
    expect(result.timedOut).toBe(false);
    expect(result.exitCode).toBeNull();
    expect(result.durationMs).toBe(0);
  });

  it("kills a running script when the signal aborts mid-execution", async () => {
    const runner = new RecordingRunner({ timeoutMs: 10_000 });
    const ac = new AbortController();
    const pending = runner.run("import time\ntime.sleep(30)\n", { signal: ac.signal });
    setTimeout(() => ac.abort(), 50);
    const result = await pending;
    expect(result.aborted).toBe(true);
    expect(result.timedOut).toBe(false);
    expect(result.durationMs).toBeLessThan(2_000);
    // SIGTERM exit, SIGKILL escalation, or exit code set are all acceptable.
    expect(result.signal === "SIGTERM" || result.signal === "SIGKILL" || result.exitCode !== null).toBe(true);
  });

  it("rejects oversized code with a typed error before spawning", async () => {
    const runner = new RecordingRunner({ timeoutMs: 10_000, maxCodeBytes: 64 });
    const code = "x".repeat(128);
    await expect(runner.run(code)).rejects.toBeInstanceOf(RecordingRunnerInputTooLargeError);
  });

  it("drain() resolves after the active script terminates", async () => {
    const runner = new RecordingRunner({ timeoutMs: 10_000 });
    const ac = new AbortController();
    const run = runner.run("import time\ntime.sleep(30)\n", { signal: ac.signal });
    // Yield so run() crosses into semaphore.run -> spawnAndCapture.
    await new Promise<void>((resolve) => setImmediate(resolve));
    ac.abort();
    await Promise.all([run, runner.drain()]);
  });
});
