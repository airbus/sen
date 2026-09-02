// === audit_outcome.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import type { AuditEntry } from "../../src/audit_log.js";
import { GatewayStateError } from "../../src/errors.js";
import type { KernelRegistry } from "../../src/kernel_registry.js";
import { makeKernelTools, type GatewayContext, type Tool } from "../../src/tools.js";

const SECRET = "s3cret-payload";

class KernelStub {
  readonly name = "k1";
  readonly signal = new AbortController().signal;
  readonly interests = new Map<string, unknown>();
  setCalls = 0;

  constructor(private readonly setImpl: () => Promise<void>) {
    this.interests.set("i1", {
      objectByName: (objectName: string) =>
        objectName === "o1"
          ? {
              className: "demo.Aircraft",
              set: async (): Promise<void> => {
                this.setCalls++;
                await this.setImpl();
              },
            }
          : undefined,
    });
  }

  assertOpen(): void {}
  drainEvents(): unknown[] {
    return [{ objectName: "o1", eventName: "e1", args: [], timestamp: "2026-01-01T00:00:00Z" }];
  }
  async subscribeToEvent(): Promise<boolean> {
    return true;
  }
  unsubscribeFromEvent(): boolean {
    return true;
  }
  unsubscribeAllEventsForInterest(): number {
    return 2;
  }
}

function harness(
  readonly: boolean,
  setImpl: () => Promise<void> = async () => undefined,
): { byName: Map<string, Tool>; registry: KernelRegistry; entries: AuditEntry[]; kernel: KernelStub } {
  const entries: AuditEntry[] = [];
  const ctx: GatewayContext = {
    audit: (entry) => entries.push(entry),
    readonly,
    shutdownSignal: new AbortController().signal,
  };
  const kernel = new KernelStub(setImpl);
  const registry = { resolve: () => kernel } as unknown as KernelRegistry;
  return { byName: new Map(makeKernelTools(ctx).map((t) => [t.name, t])), registry, entries, kernel };
}

const SET_ARGS = { interestName: "i1", objectName: "o1", propertyName: "p1", value: SECRET };

describe("audit entries carry an outcome", () => {
  it("records a successful write", async () => {
    const { byName, registry, entries } = harness(false);
    await byName.get("setProperty")!.handler(registry, SET_ARGS);
    expect(entries).toHaveLength(1);
    expect(entries[0]).toMatchObject({
      tool: "setProperty",
      kernel: "k1",
      interestName: "i1",
      objectName: "o1",
      propertyName: "p1",
      outcome: "ok",
    });
  });

  // The write reached the kernel; only the response leg failed. Logging on success alone
  // would leave an applied mutation with no entry at all.
  it("records a write that failed after reaching the kernel", async () => {
    const { byName, registry, entries, kernel } = harness(false, () => {
      throw new GatewayStateError("kernel disconnected mid-call");
    });
    await expect(byName.get("setProperty")!.handler(registry, SET_ARGS)).rejects.toThrow(GatewayStateError);
    expect(kernel.setCalls).toBe(1);
    expect(entries).toHaveLength(1);
    expect(entries[0]).toMatchObject({ tool: "setProperty", outcome: "failed", error: "GatewayStateError" });
  });

  it("records a write refused by read-only mode and never reaches the kernel", async () => {
    const { byName, registry, entries, kernel } = harness(true);
    await expect(byName.get("setProperty")!.handler(registry, SET_ARGS)).rejects.toThrow(GatewayStateError);
    expect(kernel.setCalls).toBe(0);
    expect(entries).toHaveLength(1);
    expect(entries[0]).toMatchObject({ tool: "setProperty", outcome: "denied" });
  });

  it("keeps values and error messages out of every entry", async () => {
    const { byName, registry, entries } = harness(false, () => {
      throw new Error(`rejected value ${SECRET}`);
    });
    await expect(byName.get("setProperty")!.handler(registry, SET_ARGS)).rejects.toThrow();
    expect(JSON.stringify(entries)).not.toContain(SECRET);
  });
});

describe("event tools are audited", () => {
  const cases: Array<[string, Record<string, unknown>]> = [
    ["subscribeEvent", { interestName: "i1", objectName: "o1", eventName: "e1" }],
    ["unsubscribeEvent", { interestName: "i1", objectName: "o1", eventName: "e1" }],
    ["unsubscribeAllEvents", { interestName: "i1" }],
    ["pollEvents", { interestName: "i1" }],
  ];

  for (const [name, args] of cases) {
    it(`${name} writes one entry`, async () => {
      const { byName, registry, entries } = harness(false);
      await byName.get(name)!.handler(registry, args);
      expect(entries).toHaveLength(1);
      expect(entries[0]).toMatchObject({ tool: name, kernel: "k1", interestName: "i1", outcome: "ok" });
    });
  }
});

// Wiring, not the helper: redact_url.test.ts proves the function redacts, this proves the
// connectToKernel handler actually calls it. Dropping the call at the call site leaves the
// helper's own tests green.
describe("connectToKernel does not audit the URL verbatim", () => {
  function connectHarness(): { tool: Tool; registry: KernelRegistry; entries: AuditEntry[] } {
    const entries: AuditEntry[] = [];
    const ctx: GatewayContext = {
      audit: (entry) => entries.push(entry),
      readonly: false,
      shutdownSignal: new AbortController().signal,
    };
    const registry = { connect: async () => undefined } as unknown as KernelRegistry;
    const tool = makeKernelTools(ctx).find((t) => t.name === "connectToKernel")!;
    return { tool, registry, entries };
  }

  it("strips userinfo before the entry reaches the sink", async () => {
    const { tool, registry, entries } = connectHarness();
    await tool.handler(registry, { name: "k1", url: `ws://operator:${SECRET}@host:8080` });
    expect(entries).toHaveLength(1);
    expect(entries[0]).toMatchObject({ tool: "connectToKernel", name: "k1", outcome: "ok" });
    expect(entries[0]!["url"]).toBe("ws://host:8080 (redacted)");
    expect(JSON.stringify(entries[0])).not.toContain(SECRET);
  });

  it("strips a query-string credential", async () => {
    const { tool, registry, entries } = connectHarness();
    await tool.handler(registry, { name: "k1", url: `ws://host:8080?token=${SECRET}` });
    expect(JSON.stringify(entries[0])).not.toContain(SECRET);
  });

  it("leaves an ordinary URL readable, so the log stays useful", async () => {
    const { tool, registry, entries } = connectHarness();
    await tool.handler(registry, { name: "k1", url: "ws://127.0.0.1:8080" });
    expect(entries[0]!["url"]).toBe("ws://127.0.0.1:8080");
  });
});
