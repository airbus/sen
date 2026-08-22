// === ping.test.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Sanity round-trip without a Sen subprocess: ping resolves without @sen/client.

import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const GATEWAY_ENTRYPOINT = process.env.GATEWAY_ENTRYPOINT;
if (!GATEWAY_ENTRYPOINT) {
  throw new Error(
    "GATEWAY_ENTRYPOINT not set. Run via the CMake `mcp_gateway_integration` test target, or " +
      "set it manually to apps/mcp_gateway/dist/index.js.",
  );
}

describe("sen-mcp-gateway over stdio (ping)", () => {
  let client: Client;
  let transport: StdioClientTransport;

  beforeAll(async () => {
    transport = new StdioClientTransport({
      command: "node",
      args: [GATEWAY_ENTRYPOINT],
    });
    client = new Client(
      { name: "sen-mcp-gateway-test", version: "0.0.0" },
      { capabilities: {} },
    );
    await client.connect(transport);
  });

  afterAll(async () => {
    await client.close();
  });

  it("advertises the expected tool surface", async () => {
    const { tools } = await client.listTools();
    const names = tools.map((t) => t.name);
    expect(names).toContain("ping");
    expect(names).toContain("listSessions");
    expect(names).toContain("getTypes");
    expect(names).toContain("getType");
    const ping = tools.find((t) => t.name === "ping");
    expect(ping?.description).toContain("pong");
  });

  it("returns 'pong' when ping is called", async () => {
    const result = await client.callTool({ name: "ping", arguments: {} });
    expect(result.isError ?? false).toBe(false);
    expect(result.content).toEqual([{ type: "text", text: "pong" }]);
  });

  it("returns isError for an unknown tool", async () => {
    const result = await client.callTool({ name: "nonexistent", arguments: {} });
    expect(result.isError).toBe(true);
    const content = result.content as Array<{ type: string; text: string }>;
    expect(content[0]?.text).toContain("unknown tool");
  });

  it("lists and reads baked documentation resources", async () => {
    const { resources } = await client.listResources();
    const uris = resources.map((r) => r.uri);
    expect(uris).toContain("sen://docs/users-guide/mental-model");
    expect(uris).toContain("sen://docs/users-guide/sql");
    expect(uris).toContain("sen://docs/components/jsonrpc");
    expect(uris).not.toContain("sen://docs/components/rest");
    const read = await client.readResource({ uri: "sen://docs/users-guide/sql" });
    expect(read.contents).toHaveLength(1);
    const first = read.contents[0] as { uri: string; mimeType: string; text: string };
    expect(first.mimeType).toBe("text/markdown");
    expect(first.text).toContain("Sen Query Language");
  });

  it("ships an instructions block covering Sen's core concepts", async () => {
    const serverInfo = client.getServerVersion();
    expect(serverInfo?.name).toBe("sen-mcp-gateway");
    const instructions = client.getInstructions();
    expect(instructions).toBeDefined();
    expect(instructions).toContain("Sen Query Language");
    expect(instructions).toContain("declareInterest");
    expect(instructions).toContain("staticRO");
    expect(instructions).not.toContain("[NOT YET EXPOSED]");
    expect(instructions).toContain("setProperty");
    expect(instructions).toContain("invokeMethod");
    expect(instructions).toContain("subscribeEvent");
  });
});
