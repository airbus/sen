// === recording_runner.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { spawn } from "node:child_process";

import { Semaphore } from "./util/semaphore.js";

const DEFAULT_STDOUT_CAP = 64 * 1024;
const DEFAULT_STDERR_CAP = 16 * 1024;
const DEFAULT_TIMEOUT_MS = 60_000;
const DEFAULT_MAX_PARALLEL = 4;
// Window between the child's exit and giving up on its stdio. 'close' waits for every writer
// on the inherited pipes, which a surviving grandchild can hold open indefinitely.
const STDIO_FLUSH_GRACE_MS = 250;
// Matches the typical Linux pipe-buffer size; oversized inputs stall on proc.stdin.end
// before the timeout has a chance to fire.
const DEFAULT_MAX_CODE_BYTES = 64 * 1024;

const TRUNCATION_SENTINEL = "\n... (output truncated)\n";
const TRUNCATION_SENTINEL_BYTES = Buffer.from(TRUNCATION_SENTINEL, "utf8");

// Allowlist for the python child env. Drops credentials and anything else the launching
// shell injected; operators needing a specific var pass it through via the recordings volume.
const ALLOWED_ENV_KEYS = ["PATH", "PYTHONPATH", "HOME", "LANG", "LC_ALL", "TZ", "TMPDIR"] as const;

export interface RecordingRunnerConfig {
  pythonPath?: string;
  cwd?: string;
  stdoutCapBytes?: number;
  stderrCapBytes?: number;
  timeoutMs?: number;
  maxParallel?: number;
  maxCodeBytes?: number;
}

export class RecordingRunnerInputTooLargeError extends Error {
  constructor(public readonly codeBytes: number, public readonly maxCodeBytes: number) {
    super(`runRecordingScript: code too large (${codeBytes} bytes; max ${maxCodeBytes})`);
    this.name = "RecordingRunnerInputTooLargeError";
  }
}

export interface RunResult {
  stdout: string;
  stderr: string;
  exitCode: number | null;
  signal: NodeJS.Signals | null;
  spawnFailed: boolean;
  timedOut: boolean;
  aborted: boolean;
  durationMs: number;
  stdoutTruncated: boolean;
  // The child exited but something else still held its pipes, so whatever that wrote is missing
  // from stdout. stdoutTruncated means the opposite problem, a full buffer.
  stdioAbandoned: boolean;
  stderrTruncated: boolean;
}

export interface RunOptions {
  /** SIGTERM + 500ms grace + SIGKILL, mirroring the timeout path. */
  signal?: AbortSignal;
}

export class RecordingRunner {
  readonly pythonPath: string;
  readonly stdoutCap: number;
  readonly stderrCap: number;
  readonly timeoutMs: number;
  readonly maxParallel: number;
  readonly maxCodeBytes: number;

  private readonly cwd: string | undefined;
  private readonly slots: Semaphore;

  constructor(config: RecordingRunnerConfig = {}) {
    this.pythonPath = config.pythonPath ?? "python3";
    this.cwd = config.cwd;
    this.stdoutCap = config.stdoutCapBytes ?? DEFAULT_STDOUT_CAP;
    this.stderrCap = config.stderrCapBytes ?? DEFAULT_STDERR_CAP;
    this.timeoutMs = config.timeoutMs ?? DEFAULT_TIMEOUT_MS;
    this.maxParallel = config.maxParallel ?? DEFAULT_MAX_PARALLEL;
    this.maxCodeBytes = config.maxCodeBytes ?? DEFAULT_MAX_CODE_BYTES;
    this.slots = new Semaphore(this.maxParallel);
  }

  async run(code: string, opts: RunOptions = {}): Promise<RunResult> {
    if (opts.signal?.aborted) return makeAbortedResult();
    const codeBytes = Buffer.byteLength(code, "utf8");
    if (codeBytes > this.maxCodeBytes) {
      throw new RecordingRunnerInputTooLargeError(codeBytes, this.maxCodeBytes);
    }
    try {
      return await this.slots.run(() => this.spawnAndCapture(code, opts.signal), opts.signal);
    } catch (err) {
      // Abort while queued: surface as a regular aborted RunResult so the tool handler
      // doesn't have to distinguish "queued and dropped" from "spawned and killed".
      if (err instanceof Error && err.name === "AbortError") return makeAbortedResult();
      throw err;
    }
  }

  // Caller must abort first; otherwise this waits the full timeoutMs per slot.
  drain(): Promise<void> {
    return this.slots.awaitIdle();
  }

  private spawnAndCapture(code: string, signal: AbortSignal | undefined): Promise<RunResult> {
    return new Promise((resolve) => {
      const startedAt = Date.now();
      // detached: true gives the child its own process group. On POSIX killGroup signals that
      // group, so grandchildren still in it are reached; one that leaves by starting its own
      // session is not. Windows has no groups at all, so killGroup walks the tree instead.
      const proc = spawn(this.pythonPath, ["-"], {
        cwd: this.cwd,
        env: sanitizeEnv(process.env),
        stdio: ["pipe", "pipe", "pipe"],
        detached: true,
      });

      const stdout = new BoundedCapture(this.stdoutCap);
      const stderr = new BoundedCapture(this.stderrCap);

      proc.stdout.on("data", (chunk: Buffer) => stdout.push(chunk));
      proc.stderr.on("data", (chunk: Buffer) => stderr.push(chunk));
      // EPIPE once finish() drops the read ends under a still-writing grandchild.
      proc.stdout.on("error", () => undefined);
      proc.stderr.on("error", () => undefined);

      let timedOut = false;
      let aborted = false;
      let spawnFailed = false;
      let escalated = false;
      let escalation: NodeJS.Timeout | undefined;
      const killGroup = (sig: NodeJS.Signals): void => {
        const pid = proc.pid;
        if (pid === undefined) return;
        // The child is gone, so the pid is free to be reused and the group may now be someone
        // else's. Survivors that left the group were never reachable this way in any case.
        if (proc.exitCode !== null || proc.signalCode !== null) return;
        // Windows has no process groups and libuv rejects a negative pid, so the POSIX call
        // below is a no-op there; taskkill walks the child tree instead. Neither signal is
        // catchable on Windows, so both map to the same forced kill.
        if (process.platform === "win32") {
          const killer = spawn("taskkill", ["/pid", String(pid), "/t", "/f"], { stdio: "ignore" });
          killer.on("error", () => proc.kill());
          killer.unref();
          return;
        }
        try {
          process.kill(-pid, sig);
        } catch {
          // ESRCH: group already gone.
        }
      };
      // Timeout and abort can both fire inside the grace window; a second escalation would
      // strand the first timer, which then signals a recycled pgid long after finish().
      const escalate = (): void => {
        if (escalated) return;
        escalated = true;
        killGroup("SIGTERM");
        escalation = setTimeout(() => {
          if (proc.exitCode === null && proc.signalCode === null) killGroup("SIGKILL");
        }, 500);
      };
      const timer = setTimeout(() => {
        timedOut = true;
        escalate();
      }, this.timeoutMs);

      const onAbort = (): void => {
        if (settled) return;
        aborted = true;
        escalate();
      };
      if (signal !== undefined) signal.addEventListener("abort", onAbort, { once: true });

      let settled = false;
      let flushGrace: NodeJS.Timeout | undefined;
      let stdioAbandoned = false;
      const finish = (exitCode: number | null, exitSignal: NodeJS.Signals | null): void => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        if (escalation !== undefined) clearTimeout(escalation);
        if (flushGrace !== undefined) clearTimeout(flushGrace);
        if (signal !== undefined) signal.removeEventListener("abort", onAbort);
        // Drop our read ends: a grandchild still holding the inherited pipes would otherwise
        // keep these handles (and the event loop) alive past the slot release.
        proc.stdout.destroy();
        proc.stderr.destroy();
        resolve({
          stdout: stdout.toString(),
          stderr: stderr.toString(),
          exitCode,
          signal: exitSignal,
          spawnFailed,
          timedOut,
          aborted,
          durationMs: Date.now() - startedAt,
          stdoutTruncated: stdout.truncated,
          stdioAbandoned,
          stderrTruncated: stderr.truncated,
        });
      };

      proc.on("error", (err: Error) => {
        spawnFailed = true;
        stderr.push(Buffer.from(`spawn failed: ${err.message}\n`, "utf8"));
        finish(null, null);
      });

      // 'close' alone leaks the semaphore slot forever when a grandchild inherits the pipes:
      // it fires only after exit AND stdio EOF. Settle on 'exit' once the flush window
      // lapses; 'close' still short-circuits it for the ordinary case.
      proc.on("exit", (exitCode, exitSignal) => {
        flushGrace = setTimeout(() => {
          stdioAbandoned = true;
          finish(exitCode, exitSignal);
        }, STDIO_FLUSH_GRACE_MS);
      });
      proc.on("close", (exitCode, exitSignal) => finish(exitCode, exitSignal));

      // EPIPE / sync throw if the child died before consuming: non-fatal; close handler
      // delivers the captured stderr.
      proc.stdin.on("error", () => undefined);
      try {
        proc.stdin.end(code, "utf8");
      } catch {
        // ignore
      }
    });
  }
}

// Surface as a normal RunResult so callers don't branch on abort-vs-spawn-vs-kill.
function makeAbortedResult(): RunResult {
  return {
    stdout: "",
    stderr: "",
    exitCode: null,
    signal: null,
    spawnFailed: false,
    timedOut: false,
    stdioAbandoned: false,
    aborted: true,
    durationMs: 0,
    stdoutTruncated: false,
    stderrTruncated: false,
  };
}

export function sanitizeEnv(parent: NodeJS.ProcessEnv): NodeJS.ProcessEnv {
  const out: NodeJS.ProcessEnv = {};
  for (const key of ALLOWED_ENV_KEYS) {
    const v = parent[key];
    if (v !== undefined) out[key] = v;
  }
  return out;
}

// Append-and-drop with a visible sentinel on overflow. Output is <= cap bytes; the sentinel
// share is reserved up front so the trailing marker never pushes total length over the cap.
export class BoundedCapture {
  truncated = false;
  private readonly chunks: Buffer[] = [];
  private readonly contentCap: number;
  private size = 0;

  constructor(private readonly cap: number) {
    // cap below sentinel length: no room for content or marker; first push trips truncated
    // and emits nothing.
    this.contentCap = Math.max(0, cap - TRUNCATION_SENTINEL_BYTES.length);
  }

  push(chunk: Buffer): void {
    if (this.truncated) return;
    if (this.size + chunk.length <= this.contentCap) {
      this.chunks.push(chunk);
      this.size += chunk.length;
      return;
    }
    const remaining = this.contentCap - this.size;
    if (remaining > 0) {
      this.chunks.push(chunk.subarray(0, remaining));
      this.size += remaining;
    }
    if (this.cap > TRUNCATION_SENTINEL_BYTES.length) {
      this.chunks.push(TRUNCATION_SENTINEL_BYTES);
      this.size += TRUNCATION_SENTINEL_BYTES.length;
    }
    this.truncated = true;
  }

  toString(): string {
    return Buffer.concat(this.chunks, this.size).toString("utf8");
  }
}
