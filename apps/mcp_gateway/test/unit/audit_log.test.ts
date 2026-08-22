// === audit_log.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { makeAuditChannel } from "../../src/audit_log.js";

describe("makeAuditChannel", () => {
  let tmpRoot: string;
  let logs: string[];
  const recordLog = (msg: string): void => {
    logs.push(msg);
  };

  beforeEach(() => {
    tmpRoot = mkdtempSync(join(tmpdir(), "audit-test-"));
    logs = [];
  });

  afterEach(() => {
    rmSync(tmpRoot, { recursive: true, force: true });
  });

  it("appends one JSON line per entry to the configured file, with ts and pid", async () => {
    const path = join(tmpRoot, "audit.jsonl");
    const { sink, close } = makeAuditChannel(path, recordLog);
    sink({ tool: "setProperty", kernel: "k", propertyName: "p" });
    sink({ tool: "invokeMethod", kernel: "k", methodName: "m" });
    await close();

    const lines = readFileSync(path, "utf8").trimEnd().split("\n");
    expect(lines).toHaveLength(2);
    const first = JSON.parse(lines[0]!);
    const second = JSON.parse(lines[1]!);
    expect(first.tool).toBe("setProperty");
    expect(first.propertyName).toBe("p");
    expect(typeof first.ts).toBe("string");
    expect(first.pid).toBe(process.pid);
    expect(second.tool).toBe("invokeMethod");
  });

  it("falls back to stderr when the file path is undefined", () => {
    const { sink } = makeAuditChannel(undefined, recordLog);
    const stderrSpy = vi.spyOn(process.stderr, "write").mockImplementation(() => true);
    try {
      sink({ tool: "connectToKernel", name: "k" });
      expect(stderrSpy).toHaveBeenCalledTimes(1);
      const written = stderrSpy.mock.calls[0]?.[0];
      expect(typeof written).toBe("string");
      expect(written as string).toMatch(/^audit: /);
    } finally {
      stderrSpy.mockRestore();
    }
  });

  it("falls back to stderr (and warns) when the file path can't be opened", () => {
    // Path inside a regular file makes openSync throw ENOTDIR.
    const blocker = join(tmpRoot, "not-a-dir");
    writeFileSync(blocker, "");
    const path = join(blocker, "audit.jsonl");
    const { sink } = makeAuditChannel(path, recordLog);
    const stderrSpy = vi.spyOn(process.stderr, "write").mockImplementation(() => true);
    try {
      sink({ tool: "runRecordingScript" });
      expect(stderrSpy).toHaveBeenCalledTimes(1);
      expect(logs.some((m) => m.includes("audit-log:"))).toBe(true);
    } finally {
      stderrSpy.mockRestore();
    }
  });

  it("emits the open-failure warning exactly once even after many sink calls", () => {
    const blocker = join(tmpRoot, "blocker");
    writeFileSync(blocker, "");
    const path = join(blocker, "audit.jsonl");
    const { sink } = makeAuditChannel(path, recordLog);
    const stderrSpy = vi.spyOn(process.stderr, "write").mockImplementation(() => true);
    try {
      sink({ tool: "setProperty" });
      sink({ tool: "setProperty" });
      sink({ tool: "setProperty" });
      expect(logs.filter((m) => m.includes("audit-log:"))).toHaveLength(1);
      expect(stderrSpy).toHaveBeenCalledTimes(3);
    } finally {
      stderrSpy.mockRestore();
    }
  });
});
