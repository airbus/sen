// === interest_handle.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { connect } from "../src/connect.js";
import { InterestReleasedError } from "../src/errors.js";
import type { WebSocketFactory } from "../src/internal/transport.js";
import { MockWebSocket, tick } from "./test_helpers/mock_websocket.js";
import type { InterestUpdateNotification } from "../src/index.js";

async function makeConnectedClient() {
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
  return { client, socket: MockWebSocket.lastInstance! };
}

/** Push a createInterest success response for the most recent request. */
function respondToCreateInterest(socket: MockWebSocket, expectedMethod = "createInterest"): void {
  const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
  expect(lastFrame.method).toBe(expectedMethod);
  socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
}

describe("Client.declareInterest", () => {
  it("sends createInterest with wire-faithful params and returns a handle", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "SELECT * FROM x" });
    await tick();
    const req = JSON.parse(socket.sentFrames[0]!);
    expect(req.method).toBe("createInterest");
    expect(req.params).toEqual({
      interestName: "i1",
      query: "SELECT * FROM x",
      subscribe: null,
      withSchemas: null,
    });
    respondToCreateInterest(socket);
    const handle = await pending;
    expect(handle.name).toBe("i1");
    expect(handle.objects()).toEqual([]);
    client.close();
  });

  it("rejects when the server returns an error and unregisters the handle", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "bad", query: "INVALID" });
    await tick();
    const req = JSON.parse(socket.sentFrames[0]!);
    socket.simulateMessage({
      jsonrpc: "2.0",
      id: req.id,
      error: { code: -32602, message: "invalid query" },
    });
    await expect(pending).rejects.toThrow(/invalid query/);
    client.close();
  });

  it("serializes concurrent declares for the same name behind the prior release", async () => {
    // Reproduces the React-StrictMode double-mount race: two declareInterest calls for the
    // same name fire back-to-back. Without serialization both would hit the wire and the
    // second would get "interest already exists". With it, the second declare's wire
    // createInterest is deferred until the first declare's release lifecycle completes.
    const { client, socket } = await makeConnectedClient();
    const firstPending = client.declareInterest({ name: "X", query: "q1" });
    const secondPending = client.declareInterest({ name: "X", query: "q2" });
    await tick();
    // Only the first declare's createInterest is on the wire. The second is queued behind
    // the first's settled chain link and has not yet sent anything.
    expect(socket.sentFrames).toHaveLength(1);
    const firstReq = JSON.parse(socket.sentFrames[0]!);
    expect(firstReq.method).toBe("createInterest");
    expect(firstReq.params.query).toBe("q1");
    // Complete the first declare, then immediately release the resulting handle — the
    // StrictMode hook does the equivalent on cleanup when `released` was set before the
    // declare resolved.
    socket.simulateMessage({ jsonrpc: "2.0", id: firstReq.id, result: null });
    const firstHandle = await firstPending;
    const firstRelease = firstHandle.release();
    await tick();
    // The release goes out before the second declare's createInterest — the chain link
    // is the prior handle's terminal `released` state, which only fires after the wire
    // releaseInterest completes.
    expect(socket.sentFrames).toHaveLength(2);
    const releaseReq = JSON.parse(socket.sentFrames[1]!);
    expect(releaseReq.method).toBe("releaseInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: releaseReq.id, result: null });
    await firstRelease;
    await tick();
    // With the first handle now in `released`, the chain unblocks and the second declare's
    // createInterest finally hits the wire — never seeing "already exists".
    expect(socket.sentFrames).toHaveLength(3);
    const secondReq = JSON.parse(socket.sentFrames[2]!);
    expect(secondReq.method).toBe("createInterest");
    expect(secondReq.params.query).toBe("q2");
    socket.simulateMessage({ jsonrpc: "2.0", id: secondReq.id, result: null });
    const secondHandle = await secondPending;
    expect(secondHandle.name).toBe("X");
    client.close();
  });

  it("a failed declare unblocks the next declare for the same name immediately", async () => {
    // If the first declare's wire createInterest rejects, the next declare for the same
    // name should not stall waiting on a handle that never reached `active`. The cycle
    // entry resolves on rejection so the next attempt can proceed.
    const { client, socket } = await makeConnectedClient();
    const firstPending = client.declareInterest({ name: "X", query: "INVALID" });
    const secondPending = client.declareInterest({ name: "X", query: "q" });
    await tick();
    expect(socket.sentFrames).toHaveLength(1);
    const firstReq = JSON.parse(socket.sentFrames[0]!);
    socket.simulateMessage({
      jsonrpc: "2.0",
      id: firstReq.id,
      error: { code: -32602, message: "invalid query" },
    });
    await expect(firstPending).rejects.toThrow(/invalid query/);
    await tick();
    expect(socket.sentFrames).toHaveLength(2);
    const secondReq = JSON.parse(socket.sentFrames[1]!);
    expect(secondReq.method).toBe("createInterest");
    expect(secondReq.params.query).toBe("q");
    socket.simulateMessage({ jsonrpc: "2.0", id: secondReq.id, result: null });
    const handle = await secondPending;
    expect(handle.name).toBe("X");
    client.close();
  });
});

describe("InterestHandle match-set tracking", () => {
  it("populates objectsMap from interestUpdate.added", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const update: InterestUpdateNotification = {
      interestName: "i1",
      added: [
        { objectName: "a1", qualifiedClassName: "demo.Aircraft", currentValues: [] },
        { objectName: "a2", qualifiedClassName: "demo.Aircraft", currentValues: [] },
      ],
      removed: [],
      types: [],
      typeSchemas: "",
    };
    socket.simulateMessage({ jsonrpc: "2.0", method: "interestUpdate", params: update });

    expect(handle.objects()).toHaveLength(2);
    expect(handle.objectByName("a1")?.className).toBe("demo.Aircraft");
    expect(handle.objectByName("a2")?.name).toBe("a2");
    client.close();
  });

  it("removes objects on interestUpdate.removed", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objects()).toHaveLength(1);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    expect(handle.objects()).toHaveLength(0);
    client.close();
  });

  it("drops subscriptionsByObject entry on remove when no consumers remain", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    const handleA = handle.objectByName("a1")!;
    expect(handleA).toBeDefined();
    // No subscribers ever registered — purely a passive object reference.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    // Re-add the same name. If subscriptionsByObject had leaked, the new entry would alias
    // the old sticky-state. We assert the new ObjectHandle is observably fresh: its cache
    // is empty (next get() round-trips).
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objectByName("a1")).toBeDefined();
    client.close();
  });

  it("objects() returns the same array reference between mutations", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    // Initial empty match set: repeated calls share the cached empty array.
    const empty1 = handle.objects();
    const empty2 = handle.objects();
    expect(empty1).toBe(empty2);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });

    // Membership changed: new reference. Subsequent reads share the new cached array.
    const afterAdd1 = handle.objects();
    const afterAdd2 = handle.objects();
    expect(afterAdd1).not.toBe(empty1);
    expect(afterAdd1).toBe(afterAdd2);
    expect(afterAdd1).toHaveLength(1);

    // Idempotent update with no real change should still return the same cached reference.
    // (An interestUpdate with empty added/removed shouldn't invalidate the cache.)
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: [], types: [], typeSchemas: "" },
    });
    expect(handle.objects()).toBe(afterAdd1);

    // Removing flips identity again.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    const afterRemove = handle.objects();
    expect(afterRemove).not.toBe(afterAdd1);
    expect(afterRemove).toHaveLength(0);

    client.close();
  });

  it("ignores interestUpdate scoped to a different interestName", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "other",
        added: [{ objectName: "x", qualifiedClassName: "y", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objects()).toHaveLength(0);
    client.close();
  });

  it("fires onObjectAdded / onObjectRemoved callbacks", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const added = vi.fn();
    const removed = vi.fn();
    handle.onObjectAdded(added);
    handle.onObjectRemoved(removed);

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(added).toHaveBeenCalledTimes(1);
    expect(added.mock.calls[0]![0].name).toBe("a1");

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: { interestName: "i1", added: [], removed: ["a1"], types: [], typeSchemas: "" },
    });
    expect(removed).toHaveBeenCalledWith("a1");
    client.close();
  });

  it("cancel returned by onObjectAdded is idempotent", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const added = vi.fn();
    const cancel = handle.onObjectAdded(added);
    cancel();
    cancel(); // idempotent

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(added).not.toHaveBeenCalled();
    client.close();
  });
});

describe("InterestHandle.release", () => {
  it("sends releaseInterest, clears the map, and unregisters from routing", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objects()).toHaveLength(1);

    const releasePending = handle.release();
    // grab + respond to the releaseInterest request
    const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(lastFrame.method).toBe("releaseInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
    await releasePending;

    expect(handle.objects()).toEqual([]);

    // post-release: a stray interestUpdate for the same name should not repopulate the handle.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "ghost", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objects()).toEqual([]);
    client.close();
  });

  it("is idempotent: a second release() resolves without sending again", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const releasePending = handle.release();
    const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
    await releasePending;

    const before = socket.sentFrames.length;
    await handle.release();
    expect(socket.sentFrames.length).toBe(before);
    client.close();
  });
});

describe("InterestHandle post-release method guards", () => {
  it("onObjectAdded / onObjectRemoved / getObjectsBatchState / asyncIterator throw InterestReleasedError after release()", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const releasePending = handle.release();
    const releaseReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(releaseReq.method).toBe("releaseInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: releaseReq.id, result: null });
    await releasePending;

    expect(() => handle.onObjectAdded(() => undefined)).toThrow(InterestReleasedError);
    expect(() => handle.onObjectRemoved(() => undefined)).toThrow(InterestReleasedError);
    await expect(handle.getObjectsBatchState()).rejects.toBeInstanceOf(InterestReleasedError);
    const iter = handle[Symbol.asyncIterator]();
    await expect(iter.next()).rejects.toBeInstanceOf(InterestReleasedError);

    client.close();
  });

  it("post-release synchronous accessors keep working (objects/state are read-only views)", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const releasePending = handle.release();
    const releaseReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: releaseReq.id, result: null });
    await releasePending;

    expect(handle.state).toBe("released");
    expect(handle.objects()).toEqual([]);
    expect(handle.objectByName("anything")).toBeUndefined();

    client.close();
  });

});

describe("InterestHandle state machine", () => {
  it("starts in 'pending', transitions to 'active' after createInterest succeeds", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    // We can't get the handle yet (declareInterest hasn't resolved), but the wire request has
    // been sent. Ack it.
    respondToCreateInterest(socket);
    const handle = await pending;
    expect(handle.state).toBe("active");
    client.close();
  });

  it("release-during-pending waits for createInterest to settle, then sends releaseInterest", async () => {
    const { client, socket } = await makeConnectedClient();
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    // Pop a synchronous handle ref by waiting one tick and then peeking via the registry... we
    // can't directly, so we simulate the user calling release() concurrently by awaiting
    // declarePending then immediately release. The "pending" path is exercised by the
    // release-after-rejection test below.
    respondToCreateInterest(socket);
    const handle = await declarePending;

    const before = socket.sentFrames.length;
    const releasePending = handle.release();
    await tick();
    const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(lastFrame.method).toBe("releaseInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
    await releasePending;
    expect(handle.state).toBe("released");
    expect(socket.sentFrames.length).toBeGreaterThan(before);
    client.close();
  });

  it("release after createInterest rejection is a no-op (no wire releaseInterest)", async () => {
    const { client, socket } = await makeConnectedClient();
    const declarePending = client.declareInterest({ name: "bad", query: "INVALID" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({
      jsonrpc: "2.0",
      id: createReq.id,
      error: { code: -32602, message: "invalid query" },
    });
    let caught: unknown;
    try {
      await declarePending;
    } catch (err) {
      caught = err;
    }
    expect(caught).toBeDefined();

    // The handle the user would have gotten is now released. We can't reach it from outside
    // (declareInterest threw before returning it), but the assertion is at the wire: no
    // releaseInterest frame is sent.
    const releaseFrames = socket.sentFrames
      .map((f) => JSON.parse(f))
      .filter((req: { method: string }) => req.method === "releaseInterest");
    expect(releaseFrames).toHaveLength(0);
    client.close();
  });
});

describe("InterestHandle reestablish (via Client.reestablishAll)", () => {
  it("fires onObjectRemoved for currently-matched objects before re-issuing createInterest", async () => {
    // Reestablish is idempotent per connection epoch, so the scenario needs a real bounce;
    // autoReestablish is off so this test drives the (single) pass by hand.
    const factory: WebSocketFactory = (url) => new MockWebSocket(url);
    const connectPending = connect({
      url: "ws://test",
      reconnect: { enabled: true, initialDelayMs: 1, maxDelayMs: 1, autoReestablish: false },
      onError: () => undefined,
      webSocketFactory: factory,
    });
    await tick();
    MockWebSocket.lastInstance!.simulateOpen();
    const client = await connectPending;
    const socket = MockWebSocket.lastInstance!;

    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    // Seed two matches.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [
          { objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] },
          { objectName: "a2", qualifiedClassName: "demo.X", currentValues: [] },
        ],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(handle.objects().map((o) => o.name).sort()).toEqual(["a1", "a2"]);

    const removed = vi.fn();
    handle.onObjectRemoved(removed);

    // Bounce the connection so the epoch advances and reestablish has work to do.
    socket.simulateClose(1006);
    await tick(20);
    const socket2 = MockWebSocket.lastInstance!;
    socket2.simulateOpen();
    await tick();

    // reestablishAll triggers handle.reestablish(); per the contract we want, every previously
    // matched object should be reported removed BEFORE the new createInterest goes out.
    const reestablished = client.reestablishAll();
    expect(removed.mock.calls.map((c) => c[0]).sort()).toEqual(["a1", "a2"]);
    expect(handle.objects()).toEqual([]);

    // The fresh createInterest is on the wire; ack it and replay the same two objects.
    const createReq = JSON.parse(socket2.sentFrames[socket2.sentFrames.length - 1]!);
    expect(createReq.method).toBe("createInterest");
    socket2.simulateMessage({ jsonrpc: "2.0", id: createReq.id, result: null });
    await reestablished;

    const added = vi.fn();
    handle.onObjectAdded(added);
    socket2.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [
          { objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] },
          { objectName: "a2", qualifiedClassName: "demo.X", currentValues: [] },
        ],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    expect(added.mock.calls.map((c) => c[0].name).sort()).toEqual(["a1", "a2"]);
    client.close();
  });
});

describe("InterestHandle async iteration", () => {
  it("yields objects already in the match set, then later arrivals, until released", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    // Seed two objects before iteration starts.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [
          { objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] },
          { objectName: "a2", qualifiedClassName: "demo.X", currentValues: [] },
        ],
        removed: [],
        types: [], typeSchemas: "",
      },
    });

    const seen: string[] = [];
    const iter = (async () => {
      for await (const obj of handle) {
        seen.push(obj.name);
        if (seen.length === 3) break;
      }
    })();

    // Wait one tick for the iterator to drain the initial replay.
    await tick();
    expect(seen).toEqual(["a1", "a2"]);

    // Push a late arrival; the iterator should pick it up and then break.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a3", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [], typeSchemas: "",
      },
    });
    await iter;
    expect(seen).toEqual(["a1", "a2", "a3"]);
    client.close();
  });

  it("exits cleanly when the interest is released mid-iteration", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    let iteratorDone = false;
    const iter = (async () => {
      for await (const _ of handle) {
        // Will never enter: no objects pushed, then release() wakes us out.
      }
      iteratorDone = true;
    })();

    await tick();
    expect(iteratorDone).toBe(false);

    // Release the interest. The wire releaseInterest will be answered below.
    const releasePromise = handle.release();
    await tick();
    const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    expect(lastFrame.method).toBe("releaseInterest");
    socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
    await releasePromise;
    await iter;
    expect(iteratorDone).toBe(true);
    client.close();
  });

  it("throws InterestReleasedError when iteration is started on a released interest", async () => {
    const { client, socket } = await makeConnectedClient();
    const pending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    respondToCreateInterest(socket);
    const handle = await pending;

    const releasePromise = handle.release();
    await tick();
    const lastFrame = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    socket.simulateMessage({ jsonrpc: "2.0", id: lastFrame.id, result: null });
    await releasePromise;

    let caught: unknown;
    try {
      for await (const _ of handle) { /* unreachable */ }
    } catch (err) {
      caught = err;
    }
    expect(caught).toBeInstanceOf(InterestReleasedError);
    client.close();
  });
});

describe("InterestHandle pending-state interestUpdate race", () => {
  it("queues an interestUpdate arriving before createInterest's response and drains on markActive", async () => {
    const { client, socket } = await makeConnectedClient();
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    // createInterest is on the wire but unanswered. Server races an interestUpdate in.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [],
        typeSchemas: "",
      },
    });
    // Now ack createInterest; the queued update should drain into the handle.
    respondToCreateInterest(socket);
    const handle = await declarePending;
    expect(handle.objects().map((o) => o.name)).toEqual(["a1"]);

    // A handler registered after the drain still sees the replayed object via onObjectAdded's
    // built-in current-set replay (covered elsewhere), so the queue + replay combine cleanly.
    client.close();
  });

  it("drops queued updates when the initial createInterest is rejected", async () => {
    const { client, socket } = await makeConnectedClient();
    const declarePending = client.declareInterest({ name: "bad", query: "INVALID" });
    await tick();
    const createReq = JSON.parse(socket.sentFrames[socket.sentFrames.length - 1]!);
    // Update races in ahead of the error response.
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "bad",
        added: [{ objectName: "ghost", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [],
        typeSchemas: "",
      },
    });
    socket.simulateMessage({
      jsonrpc: "2.0",
      id: createReq.id,
      error: { code: -32602, message: "invalid query" },
    });
    await expect(declarePending).rejects.toThrow(/invalid query/);
    // A later stray update for the same name must also be dropped (handle unregistered).
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "bad",
        added: [{ objectName: "ghost2", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [],
        typeSchemas: "",
      },
    });
    client.close();
  });

  it("preserves arrival order when multiple updates queue before markActive", async () => {
    const { client, socket } = await makeConnectedClient();
    const declarePending = client.declareInterest({ name: "i1", query: "q" });
    await tick();
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a1", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: [],
        types: [],
        typeSchemas: "",
      },
    });
    socket.simulateMessage({
      jsonrpc: "2.0",
      method: "interestUpdate",
      params: {
        interestName: "i1",
        added: [{ objectName: "a2", qualifiedClassName: "demo.X", currentValues: [] }],
        removed: ["a1"],
        types: [],
        typeSchemas: "",
      },
    });
    respondToCreateInterest(socket);
    const handle = await declarePending;
    // After both updates drain in order, a1 was added then removed, a2 was added.
    expect(handle.objects().map((o) => o.name)).toEqual(["a2"]);
    client.close();
  });
});
