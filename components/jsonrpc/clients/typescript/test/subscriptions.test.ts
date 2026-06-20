// === subscriptions.test.ts ===========================================================================================
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
      events: [
        {
          name: "landed",
          description: "",
          args: [{ name: "speed", description: "", type: "f64" }],
          transportMode: "unicast",
        },
      ],
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
    reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1 },
      onError: () => undefined,
    webSocketFactory: factory,
  });
  await tick();
  MockWebSocket.lastInstance!.simulateOpen();
  const client = await pending;
  const socket = MockWebSocket.lastInstance!;

  // Declare an interest and ack it.
  const declarePending = client.declareInterest({ name: "i1", query: "q" });
  await tick();
  const createReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
  expect(createReq.method).toBe("createInterest");
  socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
  const interest = await declarePending;

  // Push interestUpdate.added with the class spec bundled, mimicking the server.
  const update: InterestUpdateNotification = {
    interestName: "i1",
    added: [{ objectName: "a1", qualifiedClassName: "demo.Aircraft", currentValues: [] }],
    removed: [],
    types: [aircraftClassSpec],
    typeSchemas: "",
  };
  socket.simulateMessage({ jsonrpc: "2.0", method: "interestUpdate", params: update });
  await tick();

  const obj = interest.objectByName("a1")!;
  return { client, socket, interest, obj };
}

function findRequest(socket: MockWebSocket, method: string): { id: number; params?: unknown } | undefined {
  for (let i = socket.sentFrames.length - 1; i >= 0; i--) {
    const frame = JSON.parse(socket.sentFrames[i]!);
    if (frame.method === method) return frame;
  }
  return undefined;
}

describe("ObjectHandle.onPropertyChanged -- wire-side coordination", () => {
  it("issues subscribeProperty on first consumer, unsubscribeProperty when last cancels", async () => {
    const { client, socket, obj } = await setup();

    const cancel = obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeProperty")!;
    expect(sub.params).toEqual({
      interestName: "i1",
      objectName: "a1",
      propertyName: "altitude",
      maxRateHz: null,
    });
    socket.simulateMessage({ jsonrpc: "2.0", id: sub.id, result: null });

    cancel();
    await tick();
    const unsub = findRequest(socket, "unsubscribeProperty")!;
    expect(unsub.params).toEqual({ interestName: "i1", objectName: "a1", propertyName: "altitude" });
    socket.simulateMessage({ jsonrpc: "2.0", id: unsub.id, result: null });
    client.close();
  });

  it("multi-consumer fan-out: one wire subscribe; both handlers fire on propertyChanged", async () => {
    const { client, socket, obj } = await setup();

    const a = vi.fn();
    const b = vi.fn();
    obj.onPropertyChanged("altitude", a);
    obj.onPropertyChanged("altitude", b);
    await tick();

    // exactly one subscribeProperty across both consumers
    const subscribes = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "subscribeProperty");
    expect(subscribes).toHaveLength(1);

    // simulate propertyChanged bundle
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "1234.5" }],
        timestamp: "2026-05-24T00:00:00Z",
      },
    });
    expect(a).toHaveBeenCalledWith(1234.5, expect.objectContaining({ timestamp: expect.any(String) }));
    expect(b).toHaveBeenCalledWith(1234.5, expect.objectContaining({ timestamp: expect.any(String) }));
    client.close();
  });

  it("idempotent cancel: same cancel called twice doesn't unsubscribe twice", async () => {
    const { client, socket, obj } = await setup();
    const cancel = obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const before = socket.sentFrames.length;
    cancel();
    cancel(); // idempotent
    await tick();
    const unsubs = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "unsubscribeProperty");
    expect(unsubs).toHaveLength(1);
    client.close();
  });

  it("AbortSignal abort cancels the subscription (idempotent with explicit cancel)", async () => {
    const { client, socket, obj } = await setup();
    const ctrl = new AbortController();
    const handler = vi.fn();
    const cancel = obj.onPropertyChanged("altitude", handler, { signal: ctrl.signal });
    await tick();
    ctrl.abort();
    await tick();
    // a propertyChanged after abort doesn't fire the handler
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "1" }],
        timestamp: "2026-05-24T00:00:00Z",
      },
    });
    expect(handler).not.toHaveBeenCalled();
    // explicit cancel after abort is a no-op
    cancel();
    client.close();
  });

  it("only one wire unsubscribe when the last of many consumers cancels", async () => {
    const { client, socket, obj } = await setup();
    const c1 = obj.onPropertyChanged("altitude", vi.fn());
    const c2 = obj.onPropertyChanged("altitude", vi.fn());
    await tick();

    c1();
    await tick();
    // refcount still > 0; no unsubscribe yet
    expect(findRequest(socket, "unsubscribeProperty")).toBeUndefined();

    c2();
    await tick();
    expect(findRequest(socket, "unsubscribeProperty")).toBeDefined();
    client.close();
  });

  it("multiple properties: each gets its own wire subscribe and dispatch", async () => {
    const { client, socket, obj } = await setup();
    const altHandler = vi.fn();
    const labelHandler = vi.fn();
    obj.onPropertyChanged("altitude", altHandler);
    obj.onPropertyChanged("label", labelHandler);
    await tick();

    const subs = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "subscribeProperty");
    expect(subs).toHaveLength(2);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [
          { propertyName: "altitude", value: "100" },
          { propertyName: "label", value: '"hello"' },
        ],
        timestamp: "2026-05-24",
      },
    });
    expect(altHandler).toHaveBeenCalledWith(100, expect.objectContaining({ timestamp: expect.any(String) }));
    expect(labelHandler).toHaveBeenCalledWith("hello", expect.objectContaining({ timestamp: expect.any(String) }));
    client.close();
  });
});

describe("ObjectHandle.onPropertyChanged -- value cache freshness", () => {
  it("after last consumer cancels, a fresh subscribe does not replay the stale cached value", async () => {
    const { client, socket, obj } = await setup();

    // First subscriber: install, receive a value, cancel.
    const first = vi.fn();
    const cancelFirst = obj.onPropertyChanged("altitude", first);
    await tick();
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "100" }],
        timestamp: "2026-05-24T00:00:00Z",
      },
    });
    expect(first).toHaveBeenCalledWith(100, expect.anything());
    cancelFirst();
    await tick();

    // Second subscriber: must NOT receive the stale 100 via replay; the wire will deliver
    // a fresh value next.
    const second = vi.fn();
    obj.onPropertyChanged("altitude", second);
    await tick();
    expect(second).not.toHaveBeenCalled();

    // Simulate the fresh wire-pushed value after the re-subscribe.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "200" }],
        timestamp: "2026-05-24T00:00:01Z",
      },
    });
    expect(second).toHaveBeenCalledWith(200, expect.anything());
    client.close();
  });

  it("subscribeAll keeps the per-property cache alive across per-property unsub+resub", async () => {
    const { client, socket, obj } = await setup();

    // onAnyChange keeps a wire subscribeAll active, which keeps valueCache fresh even while
    // per-property has no consumer.
    obj.onAnyChange(vi.fn());
    await tick();

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "55" }],
        timestamp: "2026-05-24T00:00:00Z",
      },
    });

    // No per-property sub has run yet; install + cancel.
    const probe = vi.fn();
    const cancelProbe = obj.onPropertyChanged("altitude", probe);
    await tick();
    expect(probe).toHaveBeenCalledWith(55, expect.anything()); // replay from cache
    cancelProbe();
    await tick();

    // Resubscribe; the cached value should still replay because subscribeAll has been
    // keeping it fresh.
    const after = vi.fn();
    obj.onPropertyChanged("altitude", after);
    await tick();
    expect(after).toHaveBeenCalledWith(55, expect.anything());
    client.close();
  });
});

describe("ObjectHandle.onEventTriggered -- wire-side coordination", () => {
  it("issues subscribeEvent + unsubscribeEvent with idempotent cancel", async () => {
    const { client, socket, obj } = await setup();
    const cancel = obj.onEventTriggered("landed", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeEvent")!;
    expect(sub.params).toEqual({ interestName: "i1", objectName: "a1", eventName: "landed" });
    cancel();
    cancel();
    await tick();
    const unsub = findRequest(socket, "unsubscribeEvent")!;
    expect(unsub).toBeDefined();
    client.close();
  });

  it("fan-out: dispatch parses argsJson against the event's arg types", async () => {
    const { client, socket, obj } = await setup();
    const a = vi.fn();
    const b = vi.fn();
    obj.onEventTriggered("landed", a);
    obj.onEventTriggered("landed", b);
    await tick();

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "eventTriggered",
      params: {
        interestName: "i1",
        objectName: "a1",
        eventName: "landed",
        args: "[120.5]",
        timestamp: "2026-05-24",
      },
    });

    expect(a).toHaveBeenCalledWith([120.5], expect.objectContaining({ timestamp: expect.any(String) }));
    expect(b).toHaveBeenCalledWith([120.5], expect.objectContaining({ timestamp: expect.any(String) }));
    client.close();
  });

  it("awaitEventSubscribed resolves only after the server acks the subscribe", async () => {
    const { client, socket, obj } = await setup();
    obj.onEventTriggered("landed", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeEvent")!;

    let resolved = false;
    const waiter = obj.awaitEventSubscribed("landed").then(() => {
      resolved = true;
    });
    await tick();
    expect(resolved).toBe(false);

    socket.simulateMessage({ jsonrpc: "2.0", id: sub.id, result: null });
    await waiter;
    expect(resolved).toBe(true);
    client.close();
  });

  it("awaitEventSubscribed is a noop when no one is subscribed", async () => {
    const { client, obj } = await setup();
    await expect(obj.awaitEventSubscribed("landed")).resolves.toBeUndefined();
    client.close();
  });

  it("awaitEventSubscribed propagates the wire rejection", async () => {
    const { client, socket, obj } = await setup();
    obj.onEventTriggered("landed", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeEvent")!;

    socket.simulateMessage({
      jsonrpc: "2.0",
      id: sub.id,
      error: { code: -32000, message: "no such event" },
    });
    await expect(obj.awaitEventSubscribed("landed")).rejects.toThrow();
    client.close();
  });

  it("awaitPropertySubscribed resolves only after the server acks subscribeProperty", async () => {
    const { client, socket, obj } = await setup();
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeProperty")!;

    let resolved = false;
    const waiter = obj.awaitPropertySubscribed("altitude").then(() => {
      resolved = true;
    });
    await tick();
    expect(resolved).toBe(false);

    socket.simulateMessage({ jsonrpc: "2.0", id: sub.id, result: null });
    await waiter;
    expect(resolved).toBe(true);
    client.close();
  });

  it("awaitPropertySubscribed is a noop when no one is subscribed", async () => {
    const { client, obj } = await setup();
    await expect(obj.awaitPropertySubscribed("altitude")).resolves.toBeUndefined();
    client.close();
  });

  it("awaitPropertySubscribed propagates the wire rejection", async () => {
    const { client, socket, obj } = await setup();
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const sub = findRequest(socket, "subscribeProperty")!;

    socket.simulateMessage({
      jsonrpc: "2.0",
      id: sub.id,
      error: { code: -32000, message: "no such property" },
    });
    await expect(obj.awaitPropertySubscribed("altitude")).rejects.toThrow();
    client.close();
  });
});

describe("Subscription stickiness across object remove/add", () => {
  it("a returning object replays the wire subscribe; the handler still fires", async () => {
    const { client, socket, interest, obj } = await setup();

    const handler = vi.fn();
    obj.onPropertyChanged("altitude", handler);
    await tick();
    const subsBefore = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "subscribeProperty").length;
    expect(subsBefore).toBe(1);

    // Remove the object
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    expect(interest.objectByName("a1")).toBeUndefined();

    // Re-add with the same name; same class
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.Aircraft", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    await tick();

    // a second subscribeProperty should have fired (replayed)
    const subsAfter = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "subscribeProperty").length;
    expect(subsAfter).toBe(2);

    // a propertyChanged for the new object delivers to the original handler
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "999" }],
        timestamp: "2026-05-24",
      },
    });
    expect(handler).toHaveBeenCalledWith(999, expect.objectContaining({ timestamp: expect.any(String) }));
    client.close();
  });
});

describe("ObjectHandle.onAnyChange", () => {
  it("first consumer triggers wire subscribeAll; last cancel triggers unsubscribeAll", async () => {
    const { client, socket, obj } = await setup();
    const cancel = obj.onAnyChange(vi.fn());
    await tick();
    const subAll = findRequest(socket, "subscribeAll")!;
    // Layer 2 always sends maxRateHz (null when no per-object rate is set).
    expect(subAll.params).toEqual({ interestName: "i1", objectName: "a1", maxRateHz: null });

    cancel();
    await tick();
    const unsubAll = findRequest(socket, "unsubscribeAll")!;
    expect(unsubAll.params).toEqual({ interestName: "i1", objectName: "a1" });
    client.close();
  });

  it("multi-consumer fan-out: one wire subscribeAll; both handlers receive the bundle map", async () => {
    const { client, socket, obj } = await setup();
    const a = vi.fn();
    const b = vi.fn();
    obj.onAnyChange(a);
    obj.onAnyChange(b);
    await tick();

    const subAlls = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((r: { method: string }) => r.method === "subscribeAll");
    expect(subAlls).toHaveLength(1);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [
          { propertyName: "altitude", value: "1234" },
          { propertyName: "label", value: '"tower"' },
        ],
        timestamp: "2026-05-25",
      },
    });

    expect(a).toHaveBeenCalledTimes(1);
    expect(b).toHaveBeenCalledTimes(1);
    const aMap = a.mock.calls[0]![0] as Map<string, unknown>;
    expect(aMap.get("altitude")).toBe(1234);
    expect(aMap.get("label")).toBe("tower");
    client.close();
  });

  it("idempotent cancel; AbortSignal cancels too", async () => {
    const { client, socket, obj } = await setup();
    const ctrl = new AbortController();
    const handler = vi.fn();
    const cancel = obj.onAnyChange(handler, { signal: ctrl.signal });
    await tick();
    ctrl.abort();
    cancel(); // no-op after abort
    await tick();
    // Next propertyChanged should not fire the handler.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "propertyChanged",
      params: {
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "altitude", value: "1" }],
        timestamp: "2026-05-25",
      },
    });
    expect(handler).not.toHaveBeenCalled();
    client.close();
  });
});

describe("ObjectHandle.setMaxRateHz", () => {
  it("re-issues subscribeProperty for one active property with the new rate", async () => {
    const { client, socket, obj } = await setup();
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    // Ack the first (rate-less) subscribeProperty so its promise settles.
    const initialSub = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: initialSub.id, result: null });
    const before = socket.sentFrames.length;

    const pendingRate = obj.setMaxRateHz(30);
    await tick();
    // Ack the re-issued subscribeProperty so the awaited setMaxRateHz settles within the test
    // timeout instead of hitting the default 2s wire timeout.
    const reissue = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: reissue.id, result: null });
    await pendingRate;

    const ratedSubscribes = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .filter(
        (r: { method: string; params: { maxRateHz: unknown } }) =>
          r.method === "subscribeProperty" && r.params.maxRateHz === 30,
      );
    expect(ratedSubscribes.length).toBeGreaterThanOrEqual(1);
    client.close();
  });

  it("applies the per-object rate to subsequent property subscribes", async () => {
    const { client, socket, obj } = await setup();
    await obj.setMaxRateHz(60);
    await tick();
    const before = socket.sentFrames.length;
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();
    const subscribe = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "subscribeProperty") as
      | { params: { maxRateHz: unknown } }
      | undefined;
    expect(subscribe?.params.maxRateHz).toBe(60);
    client.close();
  });

  it("setMaxRateHz with no active property subs is a no-op on the wire", async () => {
    const { client, socket, obj } = await setup();
    // Only an event subscription, no property subs.
    obj.onEventTriggered("landed", vi.fn());
    await tick();
    const before = socket.sentFrames.length;
    await obj.setMaxRateHz(45);
    await tick();
    // No new wire frames - the rate sits dormant until a property subscribe lands.
    expect(socket.sentFrames.length).toBe(before);
    client.close();
  });

  it("re-issues subscribeProperty for EVERY active property with the new rate", async () => {
    const { client, socket, obj } = await setup();
    obj.onPropertyChanged("altitude", vi.fn());
    obj.onPropertyChanged("speed", vi.fn());
    await tick();
    // Ack both initial (rate-less) subscribeProperty calls so their promises settle.
    for (const f of socket.sentFrames.slice(-2)) {
      const req = JSON.parse(f);
      if (req.method === "subscribeProperty") {
        socket.simulateMessage({ jsonrpc: "2.0", id: req.id, result: null });
      }
    }
    const before = socket.sentFrames.length;

    const pending = obj.setMaxRateHz(20);
    await tick();
    // Ack each re-issued subscribe so the awaited setMaxRateHz settles.
    for (const f of socket.sentFrames.slice(before)) {
      const req = JSON.parse(f);
      socket.simulateMessage({ jsonrpc: "2.0", id: req.id, result: null });
    }
    await pending;

    const reissues = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .filter((r: { method: string }) => r.method === "subscribeProperty");
    const names = reissues.map((r: { params: { propertyName: string } }) => r.params.propertyName).sort();
    expect(names).toEqual(["altitude", "speed"]);
    for (const r of reissues) {
      expect(r.params.maxRateHz).toBe(20);
    }
    client.close();
  });

  it("re-issues subscribeAll for an active onAnyChange with the new rate", async () => {
    const { client, socket, obj } = await setup();
    obj.onAnyChange(vi.fn());
    await tick();
    // Ack the initial subscribeAll.
    const initial = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(initial.method).toBe("subscribeAll");
    socket.simulateMessage({ jsonrpc: "2.0", id: initial.id, result: null });
    const before = socket.sentFrames.length;

    const pending = obj.setMaxRateHz(15);
    await tick();
    const reissue = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(reissue.method).toBe("subscribeAll");
    expect(reissue.params.maxRateHz).toBe(15);
    socket.simulateMessage({ jsonrpc: "2.0", id: reissue.id, result: null });
    await pending;
    client.close();
  });
});

describe("Client.reestablishAll", () => {
  // Reestablish is idempotent per connection epoch: these scenarios bounce the transport
  // first (the recovery situation the API exists for) and then drive/observe the pass.
  async function bounce() {
    MockWebSocket.lastInstance!.simulateClose(1006);
    await tick(20);
    const next = MockWebSocket.lastInstance!;
    next.simulateOpen();
    await tick();
    return next;
  }

  it("re-issues createInterest for every registered interest with the original args", async () => {
    const { client } = await setup();

    const socket2 = await bounce(); // autoReestablish (default) fires the pass
    const pending = client.reestablishAll(); // coalesces with the in-flight auto pass

    const recreates = socket2.sentFrames
      .map((f) => JSON.parse(f))
      .filter((r: { method: string }) => r.method === "createInterest");
    expect(recreates).toHaveLength(1);
    expect(recreates[0].params).toEqual({ interestName: "i1", query: "q", subscribe: null, withSchemas: null });

    // ack
    socket2.simulateMessage({ jsonrpc: "2.0", id: recreates[0].id, result: null });
    await pending;
    client.close();
  });

  it("subscriptions replay automatically when the post-reestablish interestUpdate arrives", async () => {
    const { client, obj } = await setup();
    obj.onPropertyChanged("altitude", vi.fn());
    await tick();

    const socket2 = await bounce(); // auto pass fires
    const recreate = socket2.sentFrames
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "createInterest")!;
    socket2.simulateMessage({ jsonrpc: "2.0", id: recreate.id, result: null });
    await client.reestablishAll(); // coalesces; resolves with the auto pass

    // New interestUpdate.added for the same object name
    socket2.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.Aircraft", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    await tick();

    const replayedSub = socket2.sentFrames
      .map((f) => JSON.parse(f))
      .filter((r: { method: string }) => r.method === "subscribeProperty");
    expect(replayedSub.length).toBeGreaterThanOrEqual(1);
    client.close();
  });

  it("uses allSettled: a single failing interest doesn't block the rest, errors flow to onError", async () => {
    // Set up two interests; one will fail on reestablish, the other will succeed.
    const errors: Error[] = [];
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1, autoReestablish: false },
      onError: (err) => errors.push(err),
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    const socket = MockWebSocket.lastInstance!;

    // Declare both interests, ack both.
    const declareA = client.declareInterest({ name: "good", query: "q1" });
    await tick();
    const reqA = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: reqA.id, result: null });
    await declareA;
    const declareB = client.declareInterest({ name: "bad", query: "q2" });
    await tick();
    const reqB = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: reqB.id, result: null });
    await declareB;

    const socket2 = await bounce(); // autoReestablish off: nothing fires on its own

    // Reestablish both: ack the first, error the second.
    const reestablish = client.reestablishAll();
    await tick();
    const recent = socket2.sentFrames
      .map((f) => JSON.parse(f))
      .filter((r: { method: string }) => r.method === "createInterest")
      .slice(-2);
    expect(recent).toHaveLength(2);
    socket2.simulateMessage({ jsonrpc: "2.0", id: recent[0]!.id, result: null });
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: recent[1]!.id,
      error: { code: -32602, message: "server rejected" },
    });
    // The failed re-declare probes listObjects before giving up; report the interest gone.
    await tick();
    const probe = socket2.sentFrames
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "listObjects")!;
    expect(probe).toBeDefined();
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: probe.id,
      error: { code: -32011, message: "listObjects: unknown interest: bad" },
    });
    // reestablishAll resolves regardless of the per-interest failure.
    await expect(reestablish).resolves.toBeUndefined();
    expect(errors.length).toBeGreaterThanOrEqual(1);
    expect(errors.some((e) => /server rejected/.test(e.message))).toBe(true);
    client.close();
  });
});
