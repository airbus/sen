// === value_cache.test.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { connect } from "../src/connect.js";
import type { WebSocketFactory } from "../src/internal/transport.js";
import { MockWebSocket, tick } from "./test_helpers/mock_websocket.js";
import type { CustomTypeSpec, InterestUpdateNotification } from "../src/index.js";

const aircraftClassSpec: CustomTypeSpec = {
  name: "Aircraft",
  qualifiedName: "demo.Aircraft",
  description: "",
  data: {
    type: "sen.kernel.ClassTypeSpec",
    value: {
      properties: [
        {
          name: "altitude",
          description: "",
          category: "dynamicRO",
          type: "f64",
          transportMode: "unicast",
          tags: [],
          checkedSet: false,
        },
        {
          name: "label",
          description: "",
          category: "dynamicRW",
          type: "string",
          transportMode: "unicast",
          tags: [],
          checkedSet: false,
        },
      ],
      methods: [],
      events: [],
      constructor: {
        name: "Aircraft",
        description: "",
        args: [],
        transportMode: "unicast",
        constness: "nonConstant",
        deferred: false,
        returnType: "",
        propertyRelation: "nonPropertyRelated",
        localOnly: false,
      },
      parents: [],
      isInterface: false,
    },
  },
};

async function setup() {
  const factory: WebSocketFactory = (url) => new MockWebSocket(url);
  const pending = connect({
    url: "ws://test",
    reconnect: { enabled: false },
      onError: () => undefined,
    webSocketFactory: factory,
  });
  await tick();
  MockWebSocket.lastInstance!.simulateOpen();
  const client = await pending;
  const socket = MockWebSocket.lastInstance!;

  const declarePending = client.declareInterest({ name: "i1", query: "q" });
  await tick();
  const createReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
  socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
  const interest = await declarePending;
  return { client, socket, interest };
}

function addObject(socket: MockWebSocket, withCurrentValues = false): void {
  const update: InterestUpdateNotification = {
    interestName: "i1",
    added: [
      {
        objectName: "a1",
        qualifiedClassName: "demo.Aircraft",
        currentValues: withCurrentValues
          ? [
              { propertyName: "altitude", value: "9999" },
              { propertyName: "label", value: '"initial"' },
            ]
          : [],
      },
    ],
    removed: [],
    types: [aircraftClassSpec],
    typeSchemas: "",
  };
  socket.simulateMessage({ jsonrpc: "2.0", method: "interestUpdate", params: update });
}

function countCalls(socket: MockWebSocket, method: string, sinceIndex: number): number {
  return socket.sentFrames
    .slice(sinceIndex)
    .map((f) => JSON.parse(f))
    .filter((req: { method: string }) => req.method === method).length;
}

describe("ObjectHandle.get -- value cache", () => {
  it("first get hits the wire, second hits the cache", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket);
    const obj = interest.objectByName("a1")!;
    const before = socket.sentFrames.length;

    const firstPending = obj.get("altitude");
    await tick();
    const getReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: getReq.id, result: "1234" });
    await expect(firstPending).resolves.toBe(1234);
    expect(countCalls(socket, "getProperty", before)).toBe(1);

    // Second get should hit cache; no new wire calls.
    await expect(obj.get("altitude")).resolves.toBe(1234);
    expect(countCalls(socket, "getProperty", before)).toBe(1);
    client.close();
  });

  it("propertyChanged delivery updates the cache (no wire fetch needed)", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket);
    const obj = interest.objectByName("a1")!;
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const before = socket.sentFrames.length;

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "777" }],
        timestamp: "2026-05-24",
      },
    });

    await expect(obj.get("altitude")).resolves.toBe(777);
    expect(countCalls(socket, "getProperty", before)).toBe(0);
    client.close();
  });

  it("interestUpdate.added.currentValues seeds the cache for immediate reads", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket, /* withCurrentValues */ true);
    const obj = interest.objectByName("a1")!;
    const before = socket.sentFrames.length;

    await expect(obj.get("altitude")).resolves.toBe(9999);
    await expect(obj.get("label")).resolves.toBe("initial");
    expect(countCalls(socket, "getProperty", before)).toBe(0);
    client.close();
  });

  it("{ fresh: true } forces a wire round-trip even when cached", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket, /* withCurrentValues */ true);
    const obj = interest.objectByName("a1")!;
    const before = socket.sentFrames.length;

    const pending = obj.get("altitude", { fresh: true });
    await tick();
    const req = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(req.method).toBe("getProperty");
    socket.simulateMessage({ jsonrpc: "2.0", id: req.id, result: "42" });
    await expect(pending).resolves.toBe(42);
    expect(countCalls(socket, "getProperty", before)).toBe(1);

    // Subsequent get without fresh hits the updated cache.
    await expect(obj.get("altitude")).resolves.toBe(42);
    expect(countCalls(socket, "getProperty", before)).toBe(1);
    client.close();
  });

  it("set() invalidates the cache; subsequent get() falls through to wire", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket, /* withCurrentValues */ true);
    const obj = interest.objectByName("a1")!;
    // Pre-set: cache holds the seeded value; get() doesn't hit the wire.
    const before = socket.sentFrames.length;
    await expect(obj.get("label")).resolves.toBe("initial");
    expect(countCalls(socket, "getProperty", before)).toBe(0);

    const setPending = obj.set("label", "new-label");
    await tick();
    const setReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: setReq.id, result: null });
    await setPending;

    // Cache invalidated by set(); next get() must wire-fetch.
    const afterSet = socket.sentFrames.length;
    const pendingGet = obj.get("label");
    await tick();
    const getReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(getReq.method).toBe("getProperty");
    socket.simulateMessage({ jsonrpc: "2.0", id: getReq.id, result: '"new-label"' });
    await expect(pendingGet).resolves.toBe("new-label");
    expect(countCalls(socket, "getProperty", afterSet)).toBe(1);
    client.close();
  });

  it("cache is cleared when the object leaves the match set", async () => {
    const { client, socket, interest } = await setup();
    addObject(socket, /* withCurrentValues */ true);
    const objBefore = interest.objectByName("a1")!;
    // Cache is seeded; verify via cache-hit (no wire trip).
    const baseline = socket.sentFrames.length;
    await expect(objBefore.get("altitude")).resolves.toBe(9999);
    expect(countCalls(socket, "getProperty", baseline)).toBe(0);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    // Cache cleared on removal: next get() on the stale handle wire-fetches.
    const afterRemove = socket.sentFrames.length;
    const pendingGet = objBefore.get("altitude");
    await tick();
    const req = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(req.method).toBe("getProperty");
    // The handle is stale (object removed); server would normally reject. For this unit-level
    // test we just confirm the cache was invalidated by sending a synthetic response.
    socket.simulateMessage({ jsonrpc: "2.0", id: req.id, result: "0" });
    await expect(pendingGet).resolves.toBe(0);
    expect(countCalls(socket, "getProperty", afterRemove)).toBe(1);
    client.close();
  });
});
