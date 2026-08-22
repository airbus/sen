// === readonly_gate.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import type { AuditEntry } from "../../src/audit_log.js";
import { RecordingRunner } from "../../src/recording_runner.js";
import { makeKernelTools, makeRecordingTools, type GatewayContext } from "../../src/tools.js";

function makeContext(readonly: boolean): { ctx: GatewayContext; entries: AuditEntry[] } {
  const entries: AuditEntry[] = [];
  return {
    ctx: {
      audit: (entry) => entries.push(entry),
      readonly,
      shutdownSignal: new AbortController().signal,
    },
    entries,
  };
}

describe("read-only mode and the recording tools", () => {
  it("advertises all three recording tools when writes are allowed", () => {
    const { ctx } = makeContext(false);
    const names = makeRecordingTools(new RecordingRunner(), ctx).map((t) => t.name);
    expect(names).toEqual(["listRecordings", "runRecordingScript", "getRecordingDocs"]);
  });

  // runRecordingScript executes caller-supplied Python: leaving it advertised would make
  // read-only mode a paper gate, and the model would still be shown the tool.
  it("advertises none of them in read-only mode", () => {
    const { ctx } = makeContext(true);
    expect(makeRecordingTools(new RecordingRunner(), ctx)).toEqual([]);
  });

  it("still advertises the kernel tools in read-only mode", () => {
    const { ctx } = makeContext(true);
    const names = makeKernelTools(ctx).map((t) => t.name);
    expect(names).toContain("getProperty");
    expect(names).toContain("setProperty");
    expect(names).not.toContain("runRecordingScript");
  });
});

describe("read-only mode and invokeMethod", () => {
  // `constant` says a method does not modify its own object, not that calling it is harmless:
  // Sen's shell declares `fn shutdown() [const]` and it stops the kernel. So the tool is
  // withdrawn rather than filtered per method, and this pins that it really leaves the surface.
  it("withdraws invokeMethod when writes are refused", () => {
    const allowed = makeKernelTools(makeContext(false).ctx).map((tool) => tool.name);
    const refused = makeKernelTools(makeContext(true).ctx).map((tool) => tool.name);

    expect(allowed).toContain("invokeMethod");
    expect(refused).not.toContain("invokeMethod");
    expect(allowed.filter((n) => n !== "invokeMethod")).toEqual(refused);
  });

  // setProperty is the deliberate exception: a blanket refusal of every write is a check that
  // means what it says, so the tool stays visible and explains itself.
  it("keeps setProperty visible so a refusal can say why", () => {
    const refused = makeKernelTools(makeContext(true).ctx).map((tool) => tool.name);
    expect(refused).toContain("setProperty");
  });
});
