// === client.test.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { connect } from "../src/connect.js";
import { TransportError } from "../src/errors.js";
import type { WebSocketFactory } from "../src/internal/transport.js";
import { MockWebSocket, tick } from "./test_helpers/mock_websocket.js";
import type { InterestUpdateNotification } from "../src/index.js";

/** Make a connect() call that uses the mock factory and let it open. */
async function makeConnectedClient() {
  const factory: WebSocketFactory = (url) => new MockWebSocket(url);
  const pending = connect({
    url: "ws://test",
    reconnect: { enabled: false },
      onError: () => undefined,
    webSocketFactory: factory,
  });
  // simulate open on the just-created socket
  await tick();
  MockWebSocket.lastInstance!.simulateOpen();
  const client = await pending;
  const socket = MockWebSocket.lastInstance!;
  return { client, socket };
}

describe("connect()", () => {
  it("resolves with a Client once the wire is open", async () => {
    const { client } = await makeConnectedClient();
    expect(client.connectionState).toBe("open");
    client.close();
  });

  it("rejects with TransportError if the initial connect closes before opening", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: false },
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateClose(1006, "server down");
    await expect(pending).rejects.toBeInstanceOf(TransportError);
  });

  it("rejects with TransportError if openTimeoutMs elapses", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: false },
      openTimeoutMs: 20,
      onError: () => undefined,
      webSocketFactory: factory,
    });
    // never simulate open
    await expect(pending).rejects.toBeInstanceOf(TransportError);
    await expect(pending).rejects.toThrow(/timed out/);
  });

});

describe("Client lifecycle", () => {
  it("close() transitions to 'closed' state", async () => {
    const { client } = await makeConnectedClient();
    client.close();
    expect(client.connectionState).toBe("closed");
  });

  it("onDisconnect fires on unexpected wire loss", async () => {
    const { client, socket } = await makeConnectedClient();
    const disconnected = vi.fn();
    client.onDisconnect(disconnected);
    socket.simulateClose(1006);
    expect(disconnected).toHaveBeenCalledTimes(1);
    client.close();
  });

  it("onDisconnect does NOT fire on explicit close()", async () => {
    const { client } = await makeConnectedClient();
    const disconnected = vi.fn();
    client.onDisconnect(disconnected);
    client.close();
    await tick();
    expect(disconnected).not.toHaveBeenCalled();
  });

  it("onReconnect fires after a successful reconnect, not on initial open", async () => {
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
    const reconnected = vi.fn();
    client.onReconnect(reconnected);
    expect(reconnected).not.toHaveBeenCalled();

    MockWebSocket.lastInstance!.simulateClose(1006);
    // wait past the reconnect delay
    await tick(20);
    MockWebSocket.lastInstance!.simulateOpen();
    expect(reconnected).toHaveBeenCalledTimes(1);
    client.close();
  });

  it("reestablishAll() resolves (no-op in 4a, no state to replay yet)", async () => {
    const { client } = await makeConnectedClient();
    await expect(client.reestablishAll()).resolves.toBeUndefined();
    client.close();
  });
});

describe("Client.onTopologyChanged", () => {
  it("first consumer triggers wire subscribeTopology; later consumers don't", async () => {
    const { client, socket } = await makeConnectedClient();
    socket.clearFrames();

    const h1 = vi.fn();
    client.onTopologyChanged(h1);
    await tick();
    const methods1 = socket.sentFrames.map((f) => JSON.parse(f).method);
    expect(methods1).toContain("subscribeTopology");
    expect(methods1.filter((m) => m === "subscribeTopology")).toHaveLength(1);

    const before = socket.sentFrames.length;
    const h2 = vi.fn();
    client.onTopologyChanged(h2);
    await tick();
    const methods2 = socket.sentFrames.slice(before).map((f) => JSON.parse(f).method);
    expect(methods2).not.toContain("subscribeTopology");

    client.close();
  });

  it("delivers initial snapshot + later changes to all consumers; replays cache to new ones", async () => {
    const { client, socket } = await makeConnectedClient();

    const h1 = vi.fn();
    client.onTopologyChanged(h1);
    await tick();

    // Initial snapshot from the server (sent in response to subscribeTopology).
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "topologyChanged",
      params: { sessions: [{ name: "local", buses: ["jsonrpc"] }] },
    });
    expect(h1).toHaveBeenLastCalledWith([{ name: "local", buses: ["jsonrpc"] }]);

    // New consumer joining after the snapshot is replayed from cache.
    const h2 = vi.fn();
    client.onTopologyChanged(h2);
    expect(h2).toHaveBeenCalledWith([{ name: "local", buses: ["jsonrpc"] }]);

    // Subsequent change fans out to both.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "topologyChanged",
      params: { sessions: [{ name: "local", buses: ["jsonrpc", "telemetry"] }] },
    });
    expect(h1).toHaveBeenLastCalledWith([{ name: "local", buses: ["jsonrpc", "telemetry"] }]);
    expect(h2).toHaveBeenLastCalledWith([{ name: "local", buses: ["jsonrpc", "telemetry"] }]);

    client.close();
  });

  it("cancel() stops fan-out to the cancelled consumer but keeps delivering to others", async () => {
    const { client, socket } = await makeConnectedClient();
    const h1 = vi.fn();
    const h2 = vi.fn();
    client.onTopologyChanged(h1);
    const cancel2 = client.onTopologyChanged(h2);
    await tick();

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "topologyChanged",
      params: { sessions: [{ name: "s", buses: [] }] },
    });
    expect(h1).toHaveBeenCalled();
    expect(h2).toHaveBeenCalled();

    h1.mockClear();
    h2.mockClear();
    cancel2();

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "topologyChanged",
      params: { sessions: [{ name: "s", buses: ["b"] }] },
    });
    expect(h1).toHaveBeenCalled();
    expect(h2).not.toHaveBeenCalled();

    client.close();
  });
});

describe("Client type cache pre-fill from interestUpdate", () => {
  it("interestUpdate.types primes the cache so subsequent operations don't refetch the spec", async () => {
    const { client, socket } = await makeConnectedClient();

    // Declare an interest and ack.
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    const interest = await declarePending;

    // Server pushes an interestUpdate with a class spec bundled. After this the cache holds
    // the class spec, so any operation that needs it (get / set / invoke) won't issue a wire
    // getType call.
    const classSpec: InterestUpdateNotification["types"][number] = {
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
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.Aircraft", currentValues: [] }],
        removed: [],
        types: [classSpec], typeSchemas: "",
      },
    });
    await tick();

    // Observable behavior: get() on the new object's property triggers a getProperty (cache is
    // empty for the value), but does NOT trigger a getType wire call (the class spec is
    // cached). The absence of getType is the test of "interestUpdate.types pre-filled the cache."
    const obj = interest.objectByName("a1")!;
    const before = socket.sentFrames.length;
    const pendingGet = obj.get("altitude");
    await tick();
    const recent = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .map((r: { method: string }) => r.method);
    expect(recent).toContain("getProperty");
    expect(recent).not.toContain("getType");

    // Settle the wire fetch so close() doesn't leave a hanging promise.
    const getReq = socket.sentFrames
      .slice(before)
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "getProperty");
    socket.simulateMessage({ jsonrpc: "2.0", id: getReq!.id, result: "42" });
    await expect(pendingGet).resolves.toBe(42);
    client.close();
  });
});

describe("connect() autoReestablish + initialReconnect", () => {
  it("auto-fires reestablishAll after a reconnect by default", async () => {
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

    // Declare an interest so reestablishAll has something to re-issue.
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[0]!);
    expect(createReq.method).toBe("createInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    await declarePending;

    // Drop and reconnect; the new socket should receive a fresh createInterest.
    socket.simulateClose(1006);
    await tick(20);
    const newSocket = MockWebSocket.lastInstance!;
    newSocket.simulateOpen();
    await tick();
    const replayedCreate = newSocket.sentFrames
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "createInterest");
    expect(replayedCreate).toBeDefined();
    client.close();
  });

  it("autoReestablish: false skips the auto-call after reconnect", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1, autoReestablish: false },
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    const socket = MockWebSocket.lastInstance!;

    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[0]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    await declarePending;

    socket.simulateClose(1006);
    await tick(20);
    const newSocket = MockWebSocket.lastInstance!;
    newSocket.simulateOpen();
    await tick();
    const replayedCreate = newSocket.sentFrames
      .map((f) => JSON.parse(f))
      .find((r: { method: string }) => r.method === "createInterest");
    expect(replayedCreate).toBeUndefined();
    client.close();
  });

  it("initialReconnect: true retries the initial open until a subsequent attempt succeeds", async () => {
    let attempt = 0;
    const factory: WebSocketFactory = (url) => {
      attempt++;
      return new MockWebSocket(url);
    };
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1 },
      initialReconnect: true,
      openTimeoutMs: 5_000,
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    expect(attempt).toBe(1);
    // First attempt fails before open.
    MockWebSocket.lastInstance!.simulateClose(1006);
    await tick(20);
    expect(attempt).toBe(2);
    // Second attempt succeeds.
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    expect(client.connectionState).toBe("open");
    client.close();
  });

  it("initialReconnect: false (default) still rejects on first-attempt close", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1 },
      openTimeoutMs: 5_000,
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateClose(1006);
    await expect(pending).rejects.toBeInstanceOf(TransportError);
  });
});

describe("Client.reestablishAll while transport not open", () => {
  it("returns immediately without firing any wire call when the transport is reconnecting", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 60_000, maxDelayMs: 60_000, autoReestablish: false },
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    const socket = MockWebSocket.lastInstance!;

    // Declare an interest so reestablishAll has work it would otherwise try to do.
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[0]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    await declarePending;

    // Drop the socket; backoff is 60s so we stay in "reconnecting" for this test.
    socket.simulateClose(1006);
    expect(client.connectionState).toBe("reconnecting");

    const framesBefore = socket.sentFrames.length;
    await expect(client.reestablishAll()).resolves.toBeUndefined();
    expect(socket.sentFrames.length).toBe(framesBefore);

    client.close();
  });
});

describe("Client.reestablishAll single-flight + epoch idempotence + failure resync", () => {
  /** Connect with reconnect enabled, declare one interest, and settle it. */
  async function makeReconnectingClientWithInterest(errors: string[], autoReestablish: boolean) {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1, autoReestablish },
      onError: (e) => errors.push(String(e)),
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    const socket = MockWebSocket.lastInstance!;

    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[0]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    const interest = await declarePending;
    return { client, socket, interest };
  }

  function frames(socket: MockWebSocket, method: string) {
    return socket.sentFrames
      .map((f) => JSON.parse(f) as { method?: string; id?: number })
      .filter((r) => r.method === method);
  }

  /** Bounce the connection and return the new socket (opened). */
  async function bounce() {
    MockWebSocket.lastInstance!.simulateClose(1006);
    await tick(20);
    const next = MockWebSocket.lastInstance!;
    next.simulateOpen();
    await tick();
    return next;
  }

  it("manual calls during and after the auto pass cause no extra createInterest and no churn", async () => {
    const errors: string[] = [];
    const { client, interest } = await makeReconnectingClientWithInterest(errors, true);
    const removed: string[] = [];
    interest.onObjectRemoved((name) => removed.push(name));

    const socket2 = await bounce(); // auto pass fires and sends createInterest

    // Manual call while the auto pass is in flight: coalesces (single-flight).
    const manualDuring = client.reestablishAll();
    await tick();
    const creates = frames(socket2, "createInterest");
    expect(creates.length).toBe(1);
    socket2.simulateMessage({ jsonrpc: "2.0", id: creates[0]!.id, result: null });
    await manualDuring;
    await tick(5);
    expect(frames(socket2, "createInterest").length).toBe(1);

    // Manual call after the pass settled: same epoch, already re-declared -> per-handle no-op.
    const removedBefore = removed.length;
    await client.reestablishAll();
    await tick();
    expect(frames(socket2, "createInterest").length).toBe(1);
    expect(removed.length).toBe(removedBefore);
    expect(errors).toEqual([]);
    client.close();
  });

  it("re-declare rejected but interest alive server-side: resyncs from listObjects", async () => {
    const errors: string[] = [];
    const { client, interest } = await makeReconnectingClientWithInterest(errors, false);
    const added: string[] = [];
    interest.onObjectAdded((obj) => added.push(obj.name));

    const socket2 = await bounce(); // autoReestablish off: nothing fires on its own

    const run = client.reestablishAll();
    await tick();
    const create = frames(socket2, "createInterest")[0]!;
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: create.id,
      error: { code: -32602, message: "createInterest: interest already exists: i1" },
    });
    await tick();
    const list = frames(socket2, "listObjects")[0]!;
    expect(list).toBeDefined();
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: list.id,
      result: [{ objectName: "o9", qualifiedClassName: "pkg.C" }],
    });
    await run;

    expect(interest.objects().map((o) => o.name)).toEqual(["o9"]);
    expect(added).toEqual(["o9"]);
    expect(errors).toEqual([]);
    client.close();
  });

  it("re-declare rejected and interest gone server-side: surfaces the original error", async () => {
    const errors: string[] = [];
    const { client, interest } = await makeReconnectingClientWithInterest(errors, false);

    const socket2 = await bounce();

    const run = client.reestablishAll();
    await tick();
    const create = frames(socket2, "createInterest")[0]!;
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: create.id,
      error: { code: -32602, message: "createInterest: interest already exists: i1" },
    });
    await tick();
    const list = frames(socket2, "listObjects")[0]!;
    socket2.simulateMessage({
      jsonrpc: "2.0",
      id: list.id,
      error: { code: -32011, message: "listObjects: unknown interest: i1" },
    });
    await run;

    expect(interest.objects()).toEqual([]);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain("already exists");
    client.close();
  });
});

describe("Client.onNotificationsDropped", () => {
  it("delivers the per-window count and accumulates the running total", async () => {
    const { client, socket } = await makeConnectedClient();
    const seen: number[] = [];
    client.onNotificationsDropped((count) => seen.push(count));

    expect(client.droppedNotifications).toBe(0);

    socket.simulateMessage({ jsonrpc: "2.0", method: "notificationsDropped", params: { count: 3 } });
    await tick();
    socket.simulateMessage({ jsonrpc: "2.0", method: "notificationsDropped", params: { count: 4 } });
    await tick();

    expect(seen).toEqual([3, 4]);
    expect(client.droppedNotifications).toBe(7);
    client.close();
  });

  // The server reports each backpressure window once and never repeats it, so a subscriber
  // that attaches afterwards would otherwise never learn that data was missed.
  it("keeps the total readable by a handler attached after the drop", async () => {
    const { client, socket } = await makeConnectedClient();
    socket.simulateMessage({ jsonrpc: "2.0", method: "notificationsDropped", params: { count: 9 } });
    await tick();

    expect(client.droppedNotifications).toBe(9);
    client.close();
  });

  it("stops delivering once cancelled", async () => {
    const { client, socket } = await makeConnectedClient();
    const seen: number[] = [];
    const cancel = client.onNotificationsDropped((count) => seen.push(count));
    cancel();

    socket.simulateMessage({ jsonrpc: "2.0", method: "notificationsDropped", params: { count: 5 } });
    await tick();

    expect(seen).toEqual([]);
    // The running total is connection state, not handler state, so it still moves.
    expect(client.droppedNotifications).toBe(5);
    client.close();
  });

  it("reports a malformed frame instead of counting it", async () => {
    const errors: unknown[] = [];
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const pending = connect({
      url: "ws://test",
      reconnect: { enabled: false },
      onError: (err) => errors.push(err),
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await pending;
    const socket = MockWebSocket.lastInstance!;

    socket.simulateMessage({ jsonrpc: "2.0", method: "notificationsDropped", params: { count: "lots" } });
    await tick();

    expect(errors.length).toBe(1);
    expect(client.droppedNotifications).toBe(0);
    client.close();
  });
});
