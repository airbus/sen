// === recording_runner_lifecycle.test.ts ==============================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { afterEach, describe, expect, it, vi } from "vitest";
import { RecordingRunner } from "../../src/recording_runner.js";

// Exits immediately after starting a grandchild that inherits stdout/stderr and outlives it.
// Node fires 'close' only once every writer on those pipes is gone, so settling on 'close'
// alone never releases the semaphore slot.
const ORPHAN_GRANDCHILD = [
  "import subprocess, sys",
  "p = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(20)'])",
  "print(p.pid, flush=True)",
  "sys.exit(0)",
].join("\n");

// Ignores SIGTERM so it survives the escalation grace, keeping the run unsettled while both
// the timeout and the abort fire.
const IGNORES_SIGTERM = [
  "import signal, time",
  "signal.signal(signal.SIGTERM, signal.SIG_IGN)",
  "time.sleep(30)",
].join("\n");

const strays: number[] = [];

function reapStray(stdout: string): void {
  const pid = Number(stdout.trim().split("\n")[0]);
  if (Number.isInteger(pid) && pid > 0) strays.push(pid);
}

afterEach(() => {
  for (const pid of strays.splice(0)) {
    try {
      process.kill(pid, "SIGKILL");
    } catch {
      // already gone
    }
  }
});

describe("RecordingRunner child lifecycle", () => {
  it("settles when the child exits even though a grandchild holds the pipes open", async () => {
    const runner = new RecordingRunner({ timeoutMs: 30_000 });
    const startedAt = Date.now();
    const result = await runner.run(ORPHAN_GRANDCHILD);
    reapStray(result.stdout);
    expect(result.exitCode).toBe(0);
    expect(result.timedOut).toBe(false);
    expect(Date.now() - startedAt).toBeLessThan(5_000);
  }, 20_000);

  it("releases the slot so a later run is not blocked by the orphan", async () => {
    const runner = new RecordingRunner({ timeoutMs: 30_000, maxParallel: 1 });
    const startedAt = Date.now();
    reapStray((await runner.run(ORPHAN_GRANDCHILD)).stdout);
    const second = await runner.run("print('second')\n");
    expect(second.exitCode).toBe(0);
    expect(second.stdout).toContain("second");
    // The only slot must come back while the orphan is still alive, not once it happens to
    // exit and release the pipes.
    expect(Date.now() - startedAt).toBeLessThan(8_000);
  }, 30_000);

  it("drain() resolves while the orphan is still alive", async () => {
    const runner = new RecordingRunner({ timeoutMs: 30_000, maxParallel: 1 });
    reapStray((await runner.run(ORPHAN_GRANDCHILD)).stdout);
    await expect(
      Promise.race([
        runner.drain(),
        new Promise((_, reject) => setTimeout(() => reject(new Error("drain hung")), 5_000)),
      ]),
    ).resolves.toBeUndefined();
  }, 20_000);

  it("reports a normal exit code and captured stdout", async () => {
    const runner = new RecordingRunner({ timeoutMs: 10_000 });
    const result = await runner.run("print('hello')\n");
    expect(result.exitCode).toBe(0);
    expect(result.stdout).toContain("hello");
    expect(result.timedOut).toBe(false);
    expect(result.aborted).toBe(false);
  }, 20_000);
});

// Windows takes the taskkill path, which is not observable through process.kill.
describe.skipIf(process.platform === "win32")("RecordingRunner escalation", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("escalates once when the timeout and the abort both fire inside the grace window", async () => {
    const realKill = process.kill.bind(process);
    const groupSignals: Array<string | number | undefined> = [];
    vi.spyOn(process, "kill").mockImplementation(((pid: number, sig?: string | number) => {
      if (pid < 0) groupSignals.push(sig);
      return realKill(pid, sig as NodeJS.Signals);
    }) as typeof process.kill);

    const runner = new RecordingRunner({ timeoutMs: 100 });
    const ac = new AbortController();
    const pending = runner.run(IGNORES_SIGTERM, { signal: ac.signal });
    // Inside the 500ms SIGTERM->SIGKILL grace that the timeout at 100ms opened.
    setTimeout(() => ac.abort(), 250);
    const result = await pending;

    expect(result.timedOut).toBe(true);
    expect(result.aborted).toBe(true);
    // Two escalations would strand the first SIGKILL timer, which then signals whatever owns
    // the pgid by the time it fires.
    expect(groupSignals.filter((s) => s === "SIGTERM")).toHaveLength(1);
    expect(groupSignals.filter((s) => s === "SIGKILL").length).toBeLessThanOrEqual(1);
  }, 20_000);
});
