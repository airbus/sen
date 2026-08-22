// === disconnect_race.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Owns its kernel lifecycle so it can connect/disconnect freely without disturbing the
// shared "test" kernel used by sen_tools.test.ts.

import { afterAll, afterEach, beforeAll, describe, expect, it } from "vitest";
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

describe("sen-mcp-gateway kernel-disconnect races", () => {
  let client: Client;
  let transport: StdioClientTransport;
  const openKernels = new Set<string>();

  beforeAll(async () => {
    transport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT!],
      env: { ...(process.env as Record<string, string>) },
    });
    client = new Client({ name: "sen-mcp-gateway-disconnect-test", version: "0.0.0" }, { capabilities: {} });
    await client.connect(transport);
  });

  afterAll(async () => {
    await client.close();
  });

  afterEach(async () => {
    for (const name of openKernels) {
      await client.callTool({ name: "disconnectFromKernel", arguments: { name } }).catch(() => undefined);
    }
    openKernels.clear();
  });

  async function connectFresh(name: string): Promise<void> {
    await client.callTool({ name: "connectToKernel", arguments: { name, url: senUrl } });
    openKernels.add(name);
  }

  // KernelNotFoundError and KernelDisconnectedError must surface as the same user-visible
  // message so the LLM sees one outcome regardless of which side of the race won.
  it("post-disconnect getProperty surfaces 'kernel is not connected'", async () => {
    await connectFresh("d1");
    await client.callTool({
      name: "declareInterest",
      arguments: { kernel: "d1", name: "i", query: "SELECT * FROM test.primary" },
    });
    await client.callTool({ name: "disconnectFromKernel", arguments: { name: "d1" } });
    openKernels.delete("d1");

    const result = await client.callTool({
      name: "getProperty",
      arguments: { kernel: "d1", interestName: "i", objectName: "instance1", propertyName: "prop1" },
    });
    expect(result.isError).toBe(true);
    expect(textOf(result)).toContain("kernel 'd1' is not connected");
  });

  it("post-disconnect getObjectsState surfaces 'kernel is not connected'", async () => {
    await connectFresh("d2");
    await client.callTool({
      name: "declareInterest",
      arguments: { kernel: "d2", name: "i", query: "SELECT * FROM test.primary" },
    });
    await client.callTool({ name: "disconnectFromKernel", arguments: { name: "d2" } });
    openKernels.delete("d2");

    const result = await client.callTool({
      name: "getObjectsState",
      arguments: { kernel: "d2", interestName: "i" },
    });
    expect(result.isError).toBe(true);
    expect(textOf(result)).toContain("kernel 'd2' is not connected");
  });

  it("disconnect tears down event subscriptions; reconnect under same name starts fresh", async () => {
    await connectFresh("d3");
    await client.callTool({
      name: "declareInterest",
      arguments: { kernel: "d3", name: "ev", query: "SELECT * FROM test.secondary", withSchemas: true },
    });
    await client.callTool({
      name: "subscribeEvent",
      arguments: { kernel: "d3", interestName: "ev", objectName: "instance2", eventName: "somethingElseHappened" },
    });
    await client.callTool({ name: "disconnectFromKernel", arguments: { name: "d3" } });
    openKernels.delete("d3");

    const polled = await client.callTool({
      name: "pollEvents",
      arguments: { kernel: "d3", interestName: "ev" },
    });
    expect(polled.isError).toBe(true);
    expect(textOf(polled)).toContain("kernel 'd3' is not connected");

    await connectFresh("d3");
    const listed = await client.callTool({ name: "listInterests", arguments: { kernel: "d3" } });
    expect(textOf(listed)).toBe(JSON.stringify([], null, 2));
  });

  it("concurrent disconnect during an in-flight getProperty either succeeds or fails cleanly", async () => {
    await connectFresh("d4");
    await client.callTool({
      name: "declareInterest",
      arguments: { kernel: "d4", name: "i", query: "SELECT * FROM test.primary" },
    });

    const inflight = client.callTool({
      name: "getProperty",
      arguments: { kernel: "d4", interestName: "i", objectName: "instance1", propertyName: "prop1" },
    });
    // Yield so the getProperty request leaves the host before the disconnect is processed.
    await Promise.resolve();
    await client.callTool({ name: "disconnectFromKernel", arguments: { name: "d4" } });
    openKernels.delete("d4");

    const result = await inflight;
    // Forbidden shape: a generic transport error. Any structured isError is fine.
    if (result.isError) {
      const text = textOf(result);
      const cleanError =
        text.includes("is not connected") ||
        text.includes("kernel disconnected mid-call") ||
        text.includes("operation was aborted") ||
        text.includes("object not in interest match-set");
      expect(cleanError, `unexpected raw error shape: ${text}`).toBe(true);
    }
  });

  it("gateway SIGTERM during an in-flight tool call rejects the host promise and the process exits", async () => {
    // Local transport so killing the gateway doesn't affect the shared one.
    const localTransport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT!],
      env: { ...(process.env as Record<string, string>) },
    });
    const localClient = new Client({ name: "sen-mcp-sigterm-test", version: "0.0.0" }, { capabilities: {} });
    await localClient.connect(localTransport);
    try {
      await localClient.callTool({ name: "connectToKernel", arguments: { name: "sig", url: senUrl } });
      await localClient.callTool({
        name: "declareInterest",
        arguments: { kernel: "sig", name: "i", query: "SELECT * FROM test.primary" },
      });

      // Arm before kill, to avoid racing transport teardown.
      const closed = new Promise<void>((resolve) => {
        localTransport.onclose = (): void => resolve();
      });

      const inflight = localClient.callTool({
        name: "getProperty",
        arguments: { kernel: "sig", interestName: "i", objectName: "instance1", propertyName: "prop1" },
      });
      await Promise.resolve();
      const pid = localTransport.pid;
      expect(pid).not.toBeNull();
      process.kill(pid!, "SIGTERM");

      // Must settle promptly via either path: host promise rejection (transport closed
      // mid-call) or structured isError (gateway caught the abort first).
      const settled = await Promise.race([
        inflight.then((r) => ({ kind: "resolved" as const, r })).catch((e) => ({ kind: "rejected" as const, e })),
        new Promise<{ kind: "timeout" }>((resolve) => setTimeout(() => resolve({ kind: "timeout" }), 3000)),
      ]);
      expect(settled.kind).not.toBe("timeout");
      if (settled.kind === "resolved") {
        expect(settled.r.isError).toBe(true);
      }

      await Promise.race([
        closed,
        new Promise<void>((_resolve, reject) => setTimeout(() => reject(new Error("gateway did not exit")), 3000)),
      ]);
    } finally {
      await localClient.close().catch(() => undefined);
    }
  });
});
