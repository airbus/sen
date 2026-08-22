// === sen_tools.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Fixture (inheritance.yaml, spawned by globalSetup): instance1 on test.primary and
// instance2 on test.secondary, both with a static `prop1` for deterministic reads.

import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import { senUrl } from "./helpers.js";

const GATEWAY_ENTRYPOINT = process.env["GATEWAY_ENTRYPOINT"];
if (!GATEWAY_ENTRYPOINT) {
  throw new Error("GATEWAY_ENTRYPOINT not set; run via the CMake mcp_gateway_integration target.");
}

type ToolText = { type: "text"; text: string };

function textOf(result: { content: unknown }): string {
  const arr = result.content as ToolText[];
  expect(arr).toBeDefined();
  expect(arr.length).toBeGreaterThan(0);
  return arr[0]!.text;
}

function jsonOf<T = unknown>(result: { content: unknown }): T {
  return JSON.parse(textOf(result)) as T;
}

describe("sen-mcp-gateway against a real Sen", () => {
  // Shared gateway; each test owns a unique interest name and releases it.
  let client: Client;
  let transport: StdioClientTransport;

  beforeAll(async () => {
    transport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT!],
      env: { ...(process.env as Record<string, string>) },
    });
    client = new Client({ name: "sen-mcp-gateway-test", version: "0.0.0" }, { capabilities: {} });
    await client.connect(transport);
    // Single kernel for the whole file; tools omit `kernel` and auto-resolve.
    await client.callTool({ name: "connectToKernel", arguments: { name: "test", url: senUrl } });
  });

  afterAll(async () => {
    await client.close();
  });

  it("listSessions returns the buses configured in the fixture", async () => {
    const result = await client.callTool({ name: "listSessions", arguments: {} });
    expect(result.isError ?? false).toBe(false);
    const sessions = jsonOf<Array<{ name: string; buses: string[] }>>(result);
    // Buses are session-relative on the wire; qualify for the query check.
    const fqBuses = sessions.flatMap((s) => s.buses.map((b) => `${s.name}.${b}`));
    expect(fqBuses).toContain("test.primary");
    expect(fqBuses).toContain("test.secondary");
  });

  it("getTypes lists the fixture's class types alongside the wire types", async () => {
    const result = await client.callTool({ name: "getTypes", arguments: {} });
    expect(result.isError ?? false).toBe(false);
    const names = jsonOf<string[]>(result);
    expect(names).toContain("my_package.MyClass");
    expect(names).toContain("inheritance.MySubClass");
  });

  it("getType returns the spec only when withSchema is omitted", async () => {
    const result = await client.callTool({
      name: "getType",
      arguments: { qualifiedName: "my_package.MyClass" },
    });
    expect(result.isError ?? false).toBe(false);
    const payload = jsonOf<{ spec: { qualifiedName: string }; schema?: unknown }>(result);
    expect(payload.spec.qualifiedName).toBe("my_package.MyClass");
    expect(payload.schema).toBeUndefined();
  });

  it("getType with withSchema:true returns both the spec and the parsed JSON-Schema", async () => {
    const result = await client.callTool({
      name: "getType",
      arguments: { qualifiedName: "my_package.MyClass", withSchema: true },
    });
    if (result.isError) throw new Error(`getType failed: ${textOf(result)}`);
    const payload = jsonOf<{ spec: { qualifiedName: string }; schema: Record<string, unknown> }>(result);
    expect(payload.spec.qualifiedName).toBe("my_package.MyClass");
    expect(payload.schema).toBeDefined();
    expect(typeof payload.schema).toBe("object");
  });

  it("declareInterest -> listObjects -> getProperty -> releaseInterest happy path", async () => {
    const declared = await client.callTool({
      name: "declareInterest",
      arguments: { name: "primary", query: "SELECT * FROM test.primary" },
    });
    expect(declared.isError ?? false).toBe(false);
    const declaredPayload = jsonOf<{ name: string; matched: Array<{ name: string; className: string }> }>(declared);
    expect(declaredPayload.name).toBe("primary");
    expect(declaredPayload.matched).toEqual(
      expect.arrayContaining([{ name: "instance1", className: "my_package.MyClass" }]),
    );

    const listed = await client.callTool({ name: "listObjects", arguments: { name: "primary" } });
    expect(listed.isError ?? false).toBe(false);
    const listedPayload = jsonOf<{ interest: string; objects: Array<{ name: string; className: string }> }>(listed);
    expect(listedPayload.objects).toEqual(
      expect.arrayContaining([{ name: "instance1", className: "my_package.MyClass" }]),
    );

    // prop1 is static; the yaml sets it to "instance1-static".
    const prop = await client.callTool({
      name: "getProperty",
      arguments: { interestName: "primary", objectName: "instance1", propertyName: "prop1" },
    });
    expect(prop.isError ?? false).toBe(false);
    const propPayload = jsonOf<{ object: string; property: string; value: string }>(prop);
    expect(propPayload).toEqual({ object: "instance1", property: "prop1", value: "instance1-static" });

    const released = await client.callTool({ name: "releaseInterest", arguments: { name: "primary" } });
    expect(released.isError ?? false).toBe(false);
    expect(jsonOf<{ released: string; drainedEvents: unknown[] }>(released)).toEqual({
      released: "primary",
      drainedEvents: [],
    });

    const afterRelease = await client.callTool({ name: "listObjects", arguments: { name: "primary" } });
    expect(afterRelease.isError).toBe(true);
    expect(textOf(afterRelease)).toContain("no interest open with name");
  });

  it("declareInterest with a name already in use returns isError without disturbing the prior interest", async () => {
    const first = await client.callTool({
      name: "declareInterest",
      arguments: { name: "dup", query: "SELECT * FROM test.primary" },
    });
    expect(first.isError ?? false).toBe(false);

    const conflict = await client.callTool({
      name: "declareInterest",
      arguments: { name: "dup", query: "SELECT * FROM test.secondary" },
    });
    expect(conflict.isError).toBe(true);
    expect(textOf(conflict)).toContain("already open");

    const stillThere = await client.callTool({ name: "listObjects", arguments: { name: "dup" } });
    expect(stillThere.isError ?? false).toBe(false);
    const payload = jsonOf<{ objects: Array<{ name: string }> }>(stillThere);
    expect(payload.objects.map((o) => o.name)).toContain("instance1");

    await client.callTool({ name: "releaseInterest", arguments: { name: "dup" } });
  });

  it("listInterests reflects open/release transitions", async () => {
    // Delta-based since the shared client gives no ordering guarantees across tests.
    const interestNames = async (): Promise<Set<string>> => {
      const entries = jsonOf<Array<{ kernel: string; name: string }>>(
        await client.callTool({ name: "listInterests", arguments: {} }),
      );
      return new Set(entries.map((e) => e.name));
    };
    expect((await interestNames()).has("tracked")).toBe(false);

    await client.callTool({
      name: "declareInterest",
      arguments: { name: "tracked", query: "SELECT * FROM test.primary" },
    });
    expect((await interestNames()).has("tracked")).toBe(true);

    await client.callTool({ name: "releaseInterest", arguments: { name: "tracked" } });
    expect((await interestNames()).has("tracked")).toBe(false);
  });

  it("setProperty round-trips through getProperty", async () => {
    // prop7 is i32 [writable] and stable (unlike prop5 which auto-increments). The impl
    // gates accepted sets to [-5, 5] via prop7AcceptsSet; 3 is in range.
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "rw", query: "SELECT * FROM test.primary", withSchemas: true },
    });
    try {
      const setResult = await client.callTool({
        name: "setProperty",
        arguments: { interestName: "rw", objectName: "instance1", propertyName: "prop7", value: 3 },
      });
      expect(setResult.isError ?? false).toBe(false);
      expect(jsonOf(setResult)).toEqual({ ok: true });
      // drain-update-commit lands the value on the next tick (30 Hz fixture).
      await new Promise((r) => setTimeout(r, 100));
      const after = jsonOf<{ value: number }>(
        await client.callTool({
          name: "getProperty",
          arguments: { interestName: "rw", objectName: "instance1", propertyName: "prop7" },
        }),
      );
      expect(after.value).toBe(3);
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "rw" } });
    }
  });

  it("setProperty on a static property returns isError without changing state", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "ro", query: "SELECT * FROM test.primary" },
    });
    try {
      // prop1 is [static]; the wire setter rejects writes.
      const result = await client.callTool({
        name: "setProperty",
        arguments: { interestName: "ro", objectName: "instance1", propertyName: "prop1", value: "nope" },
      });
      expect(result.isError).toBe(true);
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "ro" } });
    }
  });

  it("invokeMethod returns the parsed return value", async () => {
    // addNumbers(a:i32, b:i32) -> i32 is inherited by instance2.
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "calc", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    try {
      const result = await client.callTool({
        name: "invokeMethod",
        arguments: {
          interestName: "calc",
          objectName: "instance2",
          methodName: "addNumbers",
          args: { a: 3, b: 4 },
        },
      });
      expect(result.isError ?? false).toBe(false);
      const payload = jsonOf<{ object: string; method: string; result: number }>(result);
      expect(payload).toEqual({ object: "instance2", method: "addNumbers", result: 7 });
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "calc" } });
    }
  });

  it("subscribeEvent -> invokeMethod -> pollEvents flow captures the event", async () => {
    // doSomethingElse(arg) fires somethingElseHappened(arg); invoking keeps delivery
    // deterministic (no tick-driven autonomous emit).
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "ev", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    try {
      const subResult = await client.callTool({
        name: "subscribeEvent",
        arguments: { interestName: "ev", objectName: "instance2", eventName: "somethingElseHappened" },
      });
      expect(subResult.isError ?? false).toBe(false);
      expect(jsonOf<{ newSubscription: boolean }>(subResult).newSubscription).toBe(true);

      const dup = jsonOf<{ newSubscription: boolean }>(
        await client.callTool({
          name: "subscribeEvent",
          arguments: { interestName: "ev", objectName: "instance2", eventName: "somethingElseHappened" },
        }),
      );
      expect(dup.newSubscription).toBe(false);

      await client.callTool({
        name: "invokeMethod",
        arguments: {
          interestName: "ev",
          objectName: "instance2",
          methodName: "doSomethingElse",
          args: { arg: 99 },
        },
      });
      // The event crosses kernel tick -> dispatcher -> wire -> handler, so it lags invokeMethod.
      // Poll instead of sleeping a fixed span; pollEvents drains, so keep the first non-empty batch.
      type PollResult = {
        interest: string;
        events: Array<{ objectName: string; eventName: string; args: unknown[]; timestamp: string }>;
      };
      let polled!: PollResult;
      const deadline = Date.now() + 10_000;
      for (;;) {
        polled = jsonOf<PollResult>(await client.callTool({ name: "pollEvents", arguments: { interestName: "ev" } }));
        if (polled.events.length > 0 || Date.now() > deadline) break;
        await new Promise((r) => setTimeout(r, 50));
      }
      expect(polled.interest).toBe("ev");
      expect(polled.events.length).toBeGreaterThanOrEqual(1);
      const match = polled.events.find((e) => e.eventName === "somethingElseHappened");
      expect(match).toBeDefined();
      expect(match!.objectName).toBe("instance2");
      expect(match!.args).toEqual([99]);
      // RFC 3339 with a nanosecond fraction.
      expect(match!.timestamp).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z$/);

      const empty = jsonOf<{ events: unknown[] }>(
        await client.callTool({ name: "pollEvents", arguments: { interestName: "ev" } }),
      );
      expect(empty.events).toEqual([]);
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "ev" } });
    }
  });

  it("getObjectsState returns each matched object's full property map in one call", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "snap", query: "SELECT * FROM test.primary", withSchemas: true },
    });
    try {
      const result = await client.callTool({
        name: "getObjectsState",
        arguments: { interestName: "snap" },
      });
      expect(result.isError ?? false).toBe(false);
      const payload = jsonOf<{
        interest: string;
        objects: Array<{ name: string; className: string; properties: Record<string, unknown> }>;
      }>(result);
      expect(payload.interest).toBe("snap");
      const inst1 = payload.objects.find((o) => o.name === "instance1");
      expect(inst1).toBeDefined();
      expect(inst1!.className).toBe("my_package.MyClass");
      expect(inst1!.properties["prop1"]).toBe("instance1-static");
      expect(Object.keys(inst1!.properties)).toEqual(expect.arrayContaining(["prop1", "prop5"]));
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "snap" } });
    }
  });

  it("getObjectsState honours objectNames and propertyNames filters", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "filt", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    try {
      const result = await client.callTool({
        name: "getObjectsState",
        arguments: { interestName: "filt", objectNames: ["instance2"], propertyNames: ["prop1"] },
      });
      const payload = jsonOf<{
        objects: Array<{ name: string; properties: Record<string, unknown> }>;
      }>(result);
      expect(payload.objects).toHaveLength(1);
      expect(payload.objects[0]!.name).toBe("instance2");
      expect(Object.keys(payload.objects[0]!.properties)).toEqual(["prop1"]);
      expect(payload.objects[0]!.properties["prop1"]).toBe("instance2-static");
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "filt" } });
    }
  });

  it("getObjectsState lists unmatched names under notFound", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "miss", query: "SELECT * FROM test.primary" },
    });
    try {
      const result = await client.callTool({
        name: "getObjectsState",
        arguments: { interestName: "miss", objectNames: ["instance1", "does-not-exist"] },
      });
      const payload = jsonOf<{
        objects: Array<{ name: string; properties: object; errors: object }>;
        notFound?: string[];
      }>(result);
      const present = payload.objects.find((o) => o.name === "instance1");
      expect(present?.properties).toBeDefined();
      expect(payload.objects.find((o) => o.name === "does-not-exist")).toBeUndefined();
      expect(payload.notFound).toEqual(["does-not-exist"]);
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "miss" } });
    }
  });

  it("unsubscribeAllEvents cancels every subscription on the interest", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "unsubAll", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    try {
      await client.callTool({
        name: "subscribeEvent",
        arguments: { interestName: "unsubAll", objectName: "instance2", eventName: "somethingElseHappened" },
      });
      const cancelled = jsonOf<{ cancelled: number }>(
        await client.callTool({ name: "unsubscribeAllEvents", arguments: { interestName: "unsubAll" } }),
      );
      expect(cancelled.cancelled).toBe(1);

      const second = jsonOf<{ cancelled: number }>(
        await client.callTool({ name: "unsubscribeAllEvents", arguments: { interestName: "unsubAll" } }),
      );
      expect(second.cancelled).toBe(0);

      const resub = jsonOf<{ newSubscription: boolean }>(
        await client.callTool({
          name: "subscribeEvent",
          arguments: { interestName: "unsubAll", objectName: "instance2", eventName: "somethingElseHappened" },
        }),
      );
      expect(resub.newSubscription).toBe(true);
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "unsubAll" } });
    }
  });

  it("unsubscribeAllEvents errors when the interest is unknown", async () => {
    const result = await client.callTool({
      name: "unsubscribeAllEvents",
      arguments: { interestName: "does-not-exist" },
    });
    expect(result.isError).toBe(true);
    expect(textOf(result)).toContain("no interest open with name");
  });

  it("concurrent subscribeEvent on the same key shares the in-flight ack on rejection", async () => {
    // Parallel subscribes share one in-flight ack; both rejections come from one response.
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "share", query: "SELECT * FROM test.primary", withSchemas: true },
    });
    try {
      const [r1, r2] = await Promise.all([
        client.callTool({
          name: "subscribeEvent",
          arguments: { interestName: "share", objectName: "instance1", eventName: "noSuchEvent" },
        }),
        client.callTool({
          name: "subscribeEvent",
          arguments: { interestName: "share", objectName: "instance1", eventName: "noSuchEvent" },
        }),
      ]);
      expect(r1.isError).toBe(true);
      expect(r2.isError).toBe(true);
      expect(textOf(r1)).toEqual(textOf(r2));
    } finally {
      await client.callTool({ name: "releaseInterest", arguments: { name: "share" } });
    }
  });

  it("declareInterest rejects once the per-kernel interest cap is reached", async () => {
    // INTEREST_CAP_PER_KERNEL = 64.
    const opened: string[] = [];
    try {
      for (let i = 0; i < 64; i++) {
        const name = `cap${i}`;
        const r = await client.callTool({
          name: "declareInterest",
          arguments: { name, query: "SELECT * FROM test.primary" },
        });
        expect(r.isError ?? false).toBe(false);
        opened.push(name);
      }
      const overflow = await client.callTool({
        name: "declareInterest",
        arguments: { name: "cap_overflow", query: "SELECT * FROM test.primary" },
      });
      expect(overflow.isError).toBe(true);
      expect(textOf(overflow)).toContain("interest count limit reached");
    } finally {
      for (const name of opened) {
        await client.callTool({ name: "releaseInterest", arguments: { name } }).catch(() => undefined);
      }
    }
  });

  it("releaseInterest tears down event subscriptions and drops the buffer", async () => {
    await client.callTool({
      name: "declareInterest",
      arguments: { name: "teardown", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    await client.callTool({
      name: "subscribeEvent",
      arguments: { interestName: "teardown", objectName: "instance2", eventName: "somethingElseHappened" },
    });
    await client.callTool({ name: "releaseInterest", arguments: { name: "teardown" } });

    const polled = await client.callTool({ name: "pollEvents", arguments: { interestName: "teardown" } });
    expect(polled.isError).toBe(true);
    expect(textOf(polled)).toContain("no interest open with name");
  });
});
