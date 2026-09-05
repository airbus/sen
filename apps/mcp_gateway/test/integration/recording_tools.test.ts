// === recording_tools.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Mechanics tests run against an empty tmp dir; runIf-gated tests need RECORDING_ROOT
// pointing at a real recording on disk.

import { existsSync, mkdtempSync, statSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";

import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const GATEWAY_ENTRYPOINT = process.env.GATEWAY_ENTRYPOINT;
if (!GATEWAY_ENTRYPOINT) {
  throw new Error("GATEWAY_ENTRYPOINT not set.");
}

const REAL_RECORDING_ROOT = process.env.RECORDING_ROOT;
const haveRealRecording =
  REAL_RECORDING_ROOT !== undefined && existsSync(REAL_RECORDING_ROOT) && statSync(REAL_RECORDING_ROOT).isDirectory();

const PYTHONPATH = process.env.SEN_DB_PYTHON_PATH;

interface JsonText {
  type: "text";
  text: string;
}

function parseTextResult<T>(content: unknown): T {
  const arr = content as JsonText[];
  return JSON.parse(arr[0]!.text) as T;
}

// A signal-killed child reports a null exit code on POSIX. Windows has no signals: the runner
// uses taskkill and node reports a plain non-zero exit.
function expectKilled(exitCode: number | null): void {
  if (process.platform === "win32") expect(exitCode).not.toBe(0);
  else expect(exitCode).toBeNull();
}

describe("sen-mcp-gateway recording tools (mechanics)", () => {
  let client: Client;
  let transport: StdioClientTransport;
  let tmpRoot: string;

  beforeAll(async () => {
    tmpRoot = mkdtempSync(join(tmpdir(), "sen-mcp-rec-"));
    const env: Record<string, string> = { ...(process.env as Record<string, string>) };
    if (PYTHONPATH !== undefined) env["PYTHONPATH"] = PYTHONPATH;
    transport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT],
      env,
    });
    client = new Client({ name: "sen-mcp-gateway-test", version: "0.0.0" }, { capabilities: {} });
    await client.connect(transport);
  });

  afterAll(async () => {
    await client.close();
  });

  it("registers the three recording-analysis tools", async () => {
    const { tools } = await client.listTools();
    const names = tools.map((t) => t.name);
    expect(names).toContain("listRecordings");
    expect(names).toContain("runRecordingScript");
    expect(names).toContain("getRecordingDocs");
  });

  it("listRecordings returns an empty array for an empty root", async () => {
    const res = await client.callTool({ name: "listRecordings", arguments: { root: tmpRoot } });
    expect(res.isError ?? false).toBe(false);
    expect(parseTextResult<unknown[]>(res.content)).toEqual([]);
  });

  it("listRecordings reports a clear error for a missing root", async () => {
    const res = await client.callTool({
      name: "listRecordings",
      arguments: { root: "/does/not/exist/anywhere" },
    });
    expect(res.isError).toBe(true);
    const arr = res.content as JsonText[];
    expect(arr[0]!.text).toContain("listRecordings failed");
  });

  it("listRecordings drops out-of-bounds symlinks (containment)", async () => {
    const rootDir = mkdtempSync(join(tmpdir(), "sen-mcp-rec-symlink-"));
    const outside = mkdtempSync(join(tmpdir(), "sen-mcp-rec-outside-"));
    const inside = join(rootDir, "real_recording");
    writeFileSync(inside, "fake recording bytes");
    symlinkSync(inside, join(rootDir, "in_bounds_link"));
    symlinkSync(outside, join(rootDir, "out_of_bounds_link"));

    const res = await client.callTool({ name: "listRecordings", arguments: { root: rootDir } });
    expect(res.isError ?? false).toBe(false);
    const entries = parseTextResult<Array<{ path: string }>>(res.content);
    const names = entries.map((e) => basename(e.path)).sort();
    expect(names).toEqual(["in_bounds_link", "real_recording"]);
    expect(names).not.toContain("out_of_bounds_link");
  });

  it("runRecordingScript executes a hello-world script", async () => {
    const res = await client.callTool({
      name: "runRecordingScript",
      arguments: { code: "print('hello from python')" },
    });
    expect(res.isError ?? false).toBe(false);
    const payload = parseTextResult<{ stdout: string; exitCode: number }>(res.content);
    expect(payload.stdout).toContain("hello from python");
    expect(payload.exitCode).toBe(0);
  });

  it("runRecordingScript surfaces stderr + non-zero exit on an exception", async () => {
    const res = await client.callTool({
      name: "runRecordingScript",
      arguments: { code: "raise RuntimeError('boom')" },
    });
    expect(res.isError ?? false).toBe(false);
    const payload = parseTextResult<{ stdout: string; stderr: string; exitCode: number }>(res.content);
    expect(payload.exitCode).not.toBe(0);
    expect(payload.stderr).toContain("RuntimeError");
    expect(payload.stderr).toContain("boom");
  });

  it("runRecordingScript caps stdout at 64 KiB and sets the truncation flag + sentinel", async () => {
    const res = await client.callTool({
      name: "runRecordingScript",
      arguments: { code: "import sys\nsys.stdout.write('x' * 200000)\n" },
    });
    const payload = parseTextResult<{ stdout: string; stdoutTruncated: boolean }>(res.content);
    expect(payload.stdoutTruncated).toBe(true);
    expect(payload.stdout.length).toBeGreaterThanOrEqual(64 * 1024);
    expect(payload.stdout.length).toBeLessThan(64 * 1024 + 200);
    expect(payload.stdout).toContain("(output truncated)");
  });

  it("runRecordingScript kills a hung script at the wall-clock timeout", async () => {
    const tightEnv: Record<string, string> = {
      ...(process.env as Record<string, string>),
      SEN_RECORDING_TIMEOUT_MS: "500",
    };
    if (PYTHONPATH !== undefined) tightEnv["PYTHONPATH"] = PYTHONPATH;
    const tightTransport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT],
      env: tightEnv,
    });
    const tightClient = new Client({ name: "sen-mcp-gateway-test", version: "0.0.0" }, { capabilities: {} });
    await tightClient.connect(tightTransport);
    try {
      const res = await tightClient.callTool({
        name: "runRecordingScript",
        arguments: { code: "import time\ntime.sleep(3)\nprint('should not print')" },
      });
      const payload = parseTextResult<{
        stdout: string;
        stderr: string;
        exitCode: number | null;
        timedOut: boolean;
        durationMs: number;
        stdoutTruncated: boolean;
      }>(res.content);
      expect(payload.timedOut).toBe(true);
      expectKilled(payload.exitCode);
      expect(payload.stdout).not.toContain("should not print");
      expect(payload.durationMs).toBeGreaterThanOrEqual(500);
      expect(payload.durationMs).toBeLessThan(3000);
    } finally {
      await tightClient.close();
    }
  });

  it("runRecordingScript escalates to SIGKILL when the child ignores SIGTERM", async () => {
    const tightEnv: Record<string, string> = {
      ...(process.env as Record<string, string>),
      SEN_RECORDING_TIMEOUT_MS: "500",
    };
    if (PYTHONPATH !== undefined) tightEnv["PYTHONPATH"] = PYTHONPATH;
    const tightTransport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT],
      env: tightEnv,
    });
    const tightClient = new Client({ name: "sen-mcp-gateway-test", version: "0.0.0" }, { capabilities: {} });
    await tightClient.connect(tightTransport);
    try {
      const res = await tightClient.callTool({
        name: "runRecordingScript",
        arguments: {
          code:
            "import signal, time\n" +
            "signal.signal(signal.SIGTERM, lambda *a: None)\n" +
            "time.sleep(10)\n",
        },
      });
      const payload = parseTextResult<{
        stderr: string;
        exitCode: number | null;
        timedOut: boolean;
        signal: NodeJS.Signals | null;
        durationMs: number;
      }>(res.content);
      expect(payload.timedOut).toBe(true);
      expectKilled(payload.exitCode);
      expect(payload.durationMs).toBeGreaterThanOrEqual(500);
      expect(payload.durationMs).toBeLessThan(5000);
    } finally {
      await tightClient.close();
    }
  });

  it("runRecordingScript queues parallel calls past the concurrency cap", async () => {
    // Cap=4: firing 5 x sleep(1) -> first 4 finish in ~1s, the 5th in ~2s.
    const fire = (): Promise<{ stdout: string; durationMs: number }> =>
      client
        .callTool({
          name: "runRecordingScript",
          arguments: { code: "import time\ntime.sleep(1)\nprint('ok')" },
        })
        .then((res) => parseTextResult<{ stdout: string; durationMs: number }>(res.content));
    const startedAt = Date.now();
    const results = await Promise.all([fire(), fire(), fire(), fire(), fire()]);
    const elapsed = Date.now() - startedAt;
    for (const r of results) {
      expect(r.stdout).toContain("ok");
    }
    expect(elapsed).toBeGreaterThan(1500);
    expect(elapsed).toBeLessThan(8000);
  });

  it("getRecordingDocs returns the sen_db_python reference markdown", async () => {
    const res = await client.callTool({ name: "getRecordingDocs", arguments: {} });
    expect(res.isError ?? false).toBe(false);
    const arr = res.content as JsonText[];
    const text = arr[0]!.text;
    expect(text).toContain("sen::db Python bindings");
    expect(text).toContain("import sen_db_python as sen");
  });
});

describe.runIf(haveRealRecording)("sen-mcp-gateway recording tools (with real recording)", () => {
  let client: Client;
  let transport: StdioClientTransport;

  beforeAll(async () => {
    const env: Record<string, string> = { ...(process.env as Record<string, string>) };
    if (PYTHONPATH !== undefined) env["PYTHONPATH"] = PYTHONPATH;
    transport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT],
      env,
    });
    client = new Client({ name: "sen-mcp-gateway-test", version: "0.0.0" }, { capabilities: {} });
    await client.connect(transport);
  });

  afterAll(async () => {
    await client.close();
  });

  it("listRecordings returns at least one entry under the real root", async () => {
    const res = await client.callTool({
      name: "listRecordings",
      arguments: { root: REAL_RECORDING_ROOT! },
    });
    const entries = parseTextResult<Array<{ path: string; sizeBytes: number; mtime: string }>>(res.content);
    expect(entries.length).toBeGreaterThan(0);
    expect(entries[0]?.sizeBytes).toBeGreaterThanOrEqual(0);
    expect(entries[0]?.mtime).toMatch(/^\d{4}-\d{2}-\d{2}T/);
  });

  it("Tier 1 walk: cursor walk + variant unpack + chrono", async () => {
    const listed = parseTextResult<Array<{ path: string }>>(
      (await client.callTool({ name: "listRecordings", arguments: { root: REAL_RECORDING_ROOT! } })).content,
    );
    const path = listed[0]!.path;
    const code = [
      "import sen_db_python as sen",
      "from collections import defaultdict",
      `inp = sen.Input(${JSON.stringify(path)})`,
      "counts = defaultdict(int)",
      "cursor = inp.begin()",
      "cursor.advance()",
      "while not cursor.atEnd:",
      "    counts[type(cursor.entry.payload).__name__] += 1",
      "    cursor.advance()",
      "print(dict(counts))",
    ].join("\n");
    const res = await client.callTool({ name: "runRecordingScript", arguments: { code } });
    const payload = parseTextResult<{ stdout: string; exitCode: number; stderr: string }>(res.content);
    expect(payload.exitCode).toBe(0);
    expect(payload.stdout).toMatch(/PropertyChange|Event|Keyframe|Creation/);
  });

  it("Tier 2: TypeRegistry cross-reference walks the parent chain", async () => {
    const listed = parseTextResult<Array<{ path: string }>>(
      (await client.callTool({ name: "listRecordings", arguments: { root: REAL_RECORDING_ROOT! } })).content,
    );
    const path = listed[0]!.path;
    const code = [
      "import sen_db_python as sen",
      `inp = sen.Input(${JSON.stringify(path)})`,
      "reg = inp.getTypes()",
      "names = sorted(reg.classNames)",
      "print('class count:', len(names))",
      "if names:",
      "    spec = reg.getTypeSpec(names[0])",
      "    print('first class data.type:', spec['data']['type'])",
    ].join("\n");
    const res = await client.callTool({ name: "runRecordingScript", arguments: { code } });
    const payload = parseTextResult<{ stdout: string; exitCode: number }>(res.content);
    expect(payload.exitCode).toBe(0);
    expect(payload.stdout).toContain("class count:");
    expect(payload.stdout).toMatch(/data\.type: sen\.kernel\.\w+TypeSpec/);
  });
});
