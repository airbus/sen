// === transport.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeEach, vi } from "vitest";
import { Transport, type WebSocketFactory } from "../src/internal/transport.js";
import { JsonRpcError, TimeoutError, TransportError } from "../src/errors.js";
import { MockWebSocket, tick } from "./test_helpers/mock_websocket.js";

/** Construct a Transport wired to the mock factory and let it open. */
async function makeOpenTransport(overrides?: { defaultTimeoutMs?: number }): Promise<{
  transport: Transport;
  socket: MockWebSocket;
}> {
  const factory: WebSocketFactory = (url) => new MockWebSocket(url);
  const transport = new Transport({
    url: "ws://test",
    defaultTimeoutMs: overrides?.defaultTimeoutMs ?? 1000,
    reconnect: { enabled: false },
    reportError: () => undefined,
    webSocketFactory: factory,
  });
  const socket = MockWebSocket.lastInstance!;
  socket.simulateOpen();
  await tick();
  return { transport, socket };
}

beforeEach(() => {
  MockWebSocket.lastInstance = null;
});

describe("Transport lifecycle", () => {
  it("starts in 'connecting' and moves to 'open' on socket open", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    expect(transport.connectionState).toBe("connecting");
    MockWebSocket.lastInstance!.simulateOpen();
    expect(transport.connectionState).toBe("open");
    transport.close();
  });

  it("close() transitions to 'closed' and stops accepting calls", async () => {
    const { transport } = await makeOpenTransport();
    transport.close();
    expect(transport.connectionState).toBe("closed");
    await expect(transport.call({ method: "ping" })).rejects.toBeInstanceOf(TransportError);
  });

  it("call() rejects with TransportError when the transport is not open", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    // still 'connecting'
    await expect(transport.call({ method: "ping" })).rejects.toBeInstanceOf(TransportError);
    transport.close();
  });
});

describe("Transport whenOpen()", () => {
  it("resolves on first open", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const ready = transport.whenOpen();
    MockWebSocket.lastInstance!.simulateOpen();
    await expect(ready).resolves.toBeUndefined();
    transport.close();
  });

  it("rejects with TransportError when close arrives before first open", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const ready = transport.whenOpen();
    MockWebSocket.lastInstance!.simulateClose(1006, "no server");
    await expect(ready).rejects.toBeInstanceOf(TransportError);
  });

  it("rejects with TransportError when close() is called before first open", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const ready = transport.whenOpen();
    transport.close();
    await expect(ready).rejects.toBeInstanceOf(TransportError);
  });

  it("subsequent disconnects do not re-fulfill the promise", async () => {
    const { transport, socket } = await makeOpenTransport();
    // whenOpen already resolved; calling it again returns the same resolved promise
    await expect(transport.whenOpen()).resolves.toBeUndefined();
    socket.simulateClose(1006);
    // still resolved, doesn't reject
    await expect(transport.whenOpen()).resolves.toBeUndefined();
    transport.close();
  });
});

describe("Transport request/response correlation", () => {
  it("sends a well-formed JSON-RPC request and resolves with the result", async () => {
    const { transport, socket } = await makeOpenTransport();
    const pending = transport.call({ method: "ping", params: { x: 1 } });
    expect(socket.sentFrames).toHaveLength(1);
    const sent = JSON.parse(socket.sentFrames[0]!);
    expect(sent.jsonrpc).toBe("2.0");
    expect(sent.method).toBe("ping");
    expect(sent.params).toEqual({ x: 1 });
    expect(typeof sent.id).toBe("number");

    socket.simulateMessage({ jsonrpc: "2.0", id: sent.id, result: "pong" });
    await expect(pending).resolves.toBe("pong");
    transport.close();
  });

  it("omits 'params' from the envelope when not provided", async () => {
    const { transport, socket } = await makeOpenTransport();
    // absorb the rejection that close() will trigger; only the wire frame matters here
    transport.call({ method: "ping" }).catch(() => undefined);
    const sent = JSON.parse(socket.sentFrames[0]!);
    expect(sent).not.toHaveProperty("params");
    transport.close();
  });

  it("routes multiple in-flight responses to the right promises", async () => {
    const { transport, socket } = await makeOpenTransport();
    const a = transport.call({ method: "first" });
    const b = transport.call({ method: "second" });
    const c = transport.call({ method: "third" });
    const ids = socket.sentFrames.map((f) => JSON.parse(f).id);

    // respond out of order
    socket.simulateMessage({ jsonrpc: "2.0", id: ids[1], result: "B" });
    socket.simulateMessage({ jsonrpc: "2.0", id: ids[2], result: "C" });
    socket.simulateMessage({ jsonrpc: "2.0", id: ids[0], result: "A" });

    await expect(a).resolves.toBe("A");
    await expect(b).resolves.toBe("B");
    await expect(c).resolves.toBe("C");
    transport.close();
  });

  it("rejects with JsonRpcError when the response carries an error", async () => {
    const { transport, socket } = await makeOpenTransport();
    const pending = transport.call({ method: "boom" });
    const id = JSON.parse(socket.sentFrames[0]!).id;
    socket.simulateMessage({
      jsonrpc: "2.0",
      id,
      error: { code: -32600, message: "Invalid Request", data: { detail: "oops" } },
    });
    await expect(pending).rejects.toMatchObject({
      code: -32600,
      message: "Invalid Request",
      data: { detail: "oops" },
    });
    await expect(pending).rejects.toBeInstanceOf(JsonRpcError);
    transport.close();
  });

  it("ignores a late response whose id is no longer pending", async () => {
    const { transport, socket } = await makeOpenTransport({ defaultTimeoutMs: 25 });
    const pending = transport.call({ method: "slow" });
    const id = JSON.parse(socket.sentFrames[0]!).id;
    await expect(pending).rejects.toBeInstanceOf(TimeoutError);
    // late response -- must not throw or crash
    socket.simulateMessage({ jsonrpc: "2.0", id, result: "too-late" });
    transport.close();
  });
});

describe("Transport notification dispatch", () => {
  it("fans out a matching notification to every registered handler", async () => {
    const { transport, socket } = await makeOpenTransport();
    const a = vi.fn();
    const b = vi.fn();
    transport.onNotification({ method: "tick", handler: a });
    transport.onNotification({ method: "tick", handler: b });
    socket.simulateMessage({ jsonrpc: "2.0", method: "tick", params: { n: 7 } });
    expect(a).toHaveBeenCalledWith({ n: 7 });
    expect(b).toHaveBeenCalledWith({ n: 7 });
    transport.close();
  });

  it("does not dispatch when no handlers are registered for the method", async () => {
    const { transport, socket } = await makeOpenTransport();
    const handler = vi.fn();
    transport.onNotification({ method: "other", handler });
    socket.simulateMessage({ jsonrpc: "2.0", method: "tick", params: {} });
    expect(handler).not.toHaveBeenCalled();
    transport.close();
  });

  it("CancelFn for a notification is idempotent and removes only that handler", async () => {
    const { transport, socket } = await makeOpenTransport();
    const a = vi.fn();
    const b = vi.fn();
    const cancelA = transport.onNotification({ method: "tick", handler: a });
    transport.onNotification({ method: "tick", handler: b });
    cancelA();
    cancelA(); // idempotent, no-op
    socket.simulateMessage({ jsonrpc: "2.0", method: "tick", params: {} });
    expect(a).not.toHaveBeenCalled();
    expect(b).toHaveBeenCalled();
    transport.close();
  });

  it("a throwing handler does not break the dispatch loop for siblings", async () => {
    const { transport, socket } = await makeOpenTransport();
    const thrower = vi.fn(() => {
      throw new Error("kaboom");
    });
    const survivor = vi.fn();
    transport.onNotification({ method: "tick", handler: thrower });
    transport.onNotification({ method: "tick", handler: survivor });
    socket.simulateMessage({ jsonrpc: "2.0", method: "tick", params: {} });
    expect(thrower).toHaveBeenCalled();
    expect(survivor).toHaveBeenCalled();
    transport.close();
  });
});

describe("Transport timeout + abort", () => {
  it("rejects with TimeoutError after the per-call timeout", async () => {
    const { transport } = await makeOpenTransport();
    const pending = transport.call({ method: "slow", timeoutMs: 20 });
    await expect(pending).rejects.toBeInstanceOf(TimeoutError);
    await expect(pending).rejects.toMatchObject({ timeoutMs: 20 });
    transport.close();
  });

  it("uses defaultTimeoutMs when no per-call timeout is given", async () => {
    const { transport } = await makeOpenTransport({ defaultTimeoutMs: 15 });
    await expect(transport.call({ method: "slow" })).rejects.toBeInstanceOf(TimeoutError);
    transport.close();
  });

  it("AbortSignal abort rejects the call with TransportError", async () => {
    const { transport } = await makeOpenTransport();
    const ctrl = new AbortController();
    const pending = transport.call({ method: "slow", signal: ctrl.signal });
    ctrl.abort();
    await expect(pending).rejects.toBeInstanceOf(TransportError);
    transport.close();
  });

  it("an already-aborted signal rejects synchronously without sending a frame", async () => {
    const { transport, socket } = await makeOpenTransport();
    const ctrl = new AbortController();
    ctrl.abort();
    await expect(
      transport.call({ method: "noop", signal: ctrl.signal }),
    ).rejects.toBeInstanceOf(TransportError);
    expect(socket.sentFrames).toHaveLength(0);
    transport.close();
  });
});

describe("Transport malformed input", () => {
  it("ignores binary frames and keeps the wire alive", async () => {
    const { transport, socket } = await makeOpenTransport();
    socket.simulateBinaryMessage();
    // still alive -- can still call/get a response
    const p = transport.call({ method: "ping" });
    const id = JSON.parse(socket.sentFrames[0]!).id;
    socket.simulateMessage({ jsonrpc: "2.0", id, result: "ok" });
    await expect(p).resolves.toBe("ok");
    transport.close();
  });

  it("ignores invalid JSON and keeps the wire alive", async () => {
    const { transport, socket } = await makeOpenTransport();
    socket.simulateMessage("{not valid json");
    const p = transport.call({ method: "ping" });
    const id = JSON.parse(socket.sentFrames[0]!).id;
    socket.simulateMessage({ jsonrpc: "2.0", id, result: "ok" });
    await expect(p).resolves.toBe("ok");
    transport.close();
  });

  it("rejects with TransportError when a response has neither result nor error", async () => {
    const { transport, socket } = await makeOpenTransport();
    const p = transport.call({ method: "ping" });
    const id = JSON.parse(socket.sentFrames[0]!).id;
    socket.simulateMessage({ jsonrpc: "2.0", id });
    await expect(p).rejects.toBeInstanceOf(TransportError);
    transport.close();
  });
});

describe("Transport reconnection", () => {
  it("disconnect fires onDisconnect and triggers reconnect; onReconnect fires after success", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1 },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const firstSocket = MockWebSocket.lastInstance!;
    firstSocket.simulateOpen();

    const disconnected = vi.fn();
    const reconnected = vi.fn();
    transport.onDisconnect(disconnected);
    transport.onReconnect(reconnected);

    firstSocket.simulateClose(1006, "gone");
    expect(transport.connectionState).toBe("reconnecting");
    expect(disconnected).toHaveBeenCalledTimes(1);

    // wait past the reconnect delay
    await tick(20);
    const secondSocket = MockWebSocket.lastInstance!;
    expect(secondSocket).not.toBe(firstSocket);
    secondSocket.simulateOpen();

    expect(transport.connectionState).toBe("open");
    expect(reconnected).toHaveBeenCalledTimes(1);
    transport.close();
  });

  it("disconnect rejects all in-flight calls with TransportError", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const socket = MockWebSocket.lastInstance!;
    socket.simulateOpen();

    const a = transport.call({ method: "first" });
    const b = transport.call({ method: "second" });
    socket.simulateClose(1006);
    await expect(a).rejects.toBeInstanceOf(TransportError);
    await expect(b).rejects.toBeInstanceOf(TransportError);
    transport.close();
  });

  it("close() cancels a pending reconnect", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 50 },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const firstSocket = MockWebSocket.lastInstance!;
    firstSocket.simulateOpen();
    firstSocket.simulateClose(1006);
    expect(transport.connectionState).toBe("reconnecting");
    transport.close();
    expect(transport.connectionState).toBe("closed");
    // wait past where the reconnect would have fired; no new socket
    await tick(80);
    expect(MockWebSocket.lastInstance).toBe(firstSocket);
  });

  it("onReconnect does NOT fire on the initial connect", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: false },
      reportError: () => undefined,
      webSocketFactory: factory,
    });
    const reconnected = vi.fn();
    transport.onReconnect(reconnected);
    MockWebSocket.lastInstance!.simulateOpen();
    expect(reconnected).not.toHaveBeenCalled();
    transport.close();
  });

  it("onDisconnect does NOT fire on explicit close()", async () => {
    const { transport } = await makeOpenTransport();
    const disconnected = vi.fn();
    transport.onDisconnect(disconnected);
    transport.close();
    await tick();
    expect(disconnected).not.toHaveBeenCalled();
  });
});

describe("Transport.onConnectionStateChange", () => {
  it("fires on every transition with the new state", async () => {
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const transport = new Transport({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1 },
      reportError: () => undefined,
      webSocketFactory: factory,
    });

    const states: string[] = [];
    transport.onConnectionStateChange((s) => states.push(s));

    // connecting -> open
    MockWebSocket.lastInstance!.simulateOpen();
    expect(states).toEqual(["open"]);

    // open -> reconnecting
    MockWebSocket.lastInstance!.simulateClose(1006, "gone");
    expect(states).toEqual(["open", "reconnecting"]);

    // reconnecting -> open
    await tick(20);
    MockWebSocket.lastInstance!.simulateOpen();
    expect(states).toEqual(["open", "reconnecting", "open"]);

    // -> closed (explicit)
    transport.close();
    expect(states).toEqual(["open", "reconnecting", "open", "closed"]);
  });

  it("does not fire on register and does not re-fire when the state stays the same", async () => {
    const { transport } = await makeOpenTransport();
    const fn = vi.fn();
    transport.onConnectionStateChange(fn);
    await tick();
    expect(fn).not.toHaveBeenCalled();
    transport.close();
    expect(fn).toHaveBeenCalledTimes(1);
    transport.close();  // idempotent
    expect(fn).toHaveBeenCalledTimes(1);
  });

  it("cancel function removes the handler", async () => {
    const { transport } = await makeOpenTransport();
    const fn = vi.fn();
    const cancel = transport.onConnectionStateChange(fn);
    cancel();
    transport.close();
    expect(fn).not.toHaveBeenCalled();
  });
});


describe("Transport reconnect handshake deadline", () => {
  it("abandons a hung handshake attempt and retries", async () => {
    vi.useFakeTimers();
    try {
      const factory: WebSocketFactory = (url) => new MockWebSocket(url);
      const transport = new Transport({
        url: "ws://test",
        reconnect: { enabled: true, initialDelayMs: 10, maxDelayMs: 10, factor: 1 },
        openTimeoutMs: 50,
        reportError: () => undefined,
        webSocketFactory: factory,
      });
      const first = MockWebSocket.lastInstance!;
      first.simulateOpen();
      expect(transport.connectionState).toBe("open");

      let reconnects = 0;
      transport.onReconnect(() => {
        reconnects += 1;
      });

      // Drop the connection; the first retry produces a socket that never opens.
      first.simulateClose();
      await vi.advanceTimersByTimeAsync(10);
      const hung = MockWebSocket.lastInstance!;
      expect(hung).not.toBe(first);

      // The hung attempt is abandoned after openTimeoutMs and a new attempt is made.
      await vi.advanceTimersByTimeAsync(50 + 10);
      const next = MockWebSocket.lastInstance!;
      expect(next).not.toBe(hung);
      expect(transport.connectionState).toBe("reconnecting");

      // The next attempt can still succeed, and the late events of the abandoned
      // socket must not disturb the fresh connection.
      next.simulateOpen();
      hung.simulateOpen();
      hung.simulateClose();
      expect(reconnects).toBe(1);
      expect(transport.connectionState).toBe("open");
      transport.close();
    } finally {
      vi.useRealTimers();
    }
  });
});
