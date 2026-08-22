// === kernel_registry.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Every dial is parked until the test settles it, so the connect/disconnect interleavings below
// are exact rather than timing-dependent.

import { beforeEach, describe, expect, it, vi } from "vitest";
import type { Client } from "@sen/client";
import { KernelRegistry } from "../../src/kernel_registry.js";
import { Kernel } from "../../src/kernel.js";
import { KernelDisconnectedError } from "../../src/errors.js";

interface FakeClient {
  connectionState: string;
  closed: boolean;
  onConnectionStateChange: () => void;
  onReconnect: () => void;
  onDisconnect: () => void;
  close: () => void;
}

const shared = vi.hoisted(() => ({
  dials: [] as Array<{ resolve: (client: unknown) => void; reject: (err: unknown) => void }>,
}));

vi.mock("@sen/client", () => ({
  connect: () =>
    new Promise((resolve, reject) => {
      shared.dials.push({ resolve, reject });
    }),
}));

function fakeClient(): FakeClient {
  const client: FakeClient = {
    connectionState: "open",
    closed: false,
    onConnectionStateChange: () => undefined,
    onReconnect: () => undefined,
    onDisconnect: () => undefined,
    close: () => {
      client.closed = true;
    },
  };
  return client;
}

function settleDial(index: number, client?: FakeClient): FakeClient | undefined {
  const dial = shared.dials[index];
  if (dial === undefined) throw new Error(`no dial at index ${index} (have ${shared.dials.length})`);
  if (client === undefined) {
    dial.reject(new Error("dial failed"));
    return undefined;
  }
  dial.resolve(client as unknown as Client);
  return client;
}

describe("KernelRegistry name ownership", () => {
  beforeEach(() => {
    shared.dials.length = 0;
  });

  it("a failed dial does not evict the kernel that took the name after it", async () => {
    const registry = new KernelRegistry(() => undefined);

    const slow = registry.connect("a", "ws://k", 100);
    const disconnected = registry.disconnect("a");
    const second = registry.connect("a", "ws://k", 100);

    settleDial(0);
    await expect(slow).rejects.toThrow("dial failed");
    await disconnected;

    const client = settleDial(1, fakeClient())!;
    const survivor = await second;

    expect(registry.list()).toEqual([survivor]);
    expect(registry.resolve("a")).toBe(survivor);
    expect(client.closed).toBe(false);

    // Reachable means shuttable-down: the eviction bug left the survivor's socket alive with
    // nothing able to close it.
    await registry.shutdown();
    expect(client.closed).toBe(true);
  });

  it("frees the name on a failed dial when it still owns it", async () => {
    const registry = new KernelRegistry(() => undefined);
    const failing = registry.connect("a", "ws://k", 100);
    settleDial(0);
    await expect(failing).rejects.toThrow("dial failed");
    expect(registry.list()).toEqual([]);

    const retry = registry.connect("a", "ws://k", 100);
    settleDial(1, fakeClient());
    expect((await retry).name).toBe("a");
  });

  it("a reconnect during a slow shutdown keeps the name", async () => {
    const registry = new KernelRegistry(() => undefined);
    const first = registry.connect("a", "ws://k", 100);
    const firstClient = settleDial(0, fakeClient())!;
    const original = await first;

    // disconnect frees the name before awaiting shutdown; the reconnect below lands inside that
    // window and must not be undone when the teardown finishes.
    const disconnected = registry.disconnect("a");
    const second = registry.connect("a", "ws://k", 100);
    settleDial(1, fakeClient());
    const replacement = await second;
    await disconnected;

    expect(replacement).not.toBe(original);
    expect(registry.resolve("a")).toBe(replacement);
    expect(firstClient.closed).toBe(true);
  });

  it("rejects a duplicate name while the first dial is still in flight", async () => {
    const registry = new KernelRegistry(() => undefined);
    const first = registry.connect("a", "ws://k", 100);
    await expect(registry.connect("a", "ws://k", 100)).rejects.toThrow("already in use");
    settleDial(0, fakeClient());
    await first;
    expect(registry.list()).toHaveLength(1);
  });

  it("a shut-down kernel does not open another socket", async () => {
    const kernel = new Kernel("a", "ws://k", 100, () => undefined);
    const first = kernel.openWs();
    // A failed dial clears the cached promise, so the next client() would dial afresh.
    settleDial(0);
    await expect(first).rejects.toThrow("dial failed");
    await kernel.shutdown();

    const afterShutdown = kernel.client();
    expect(shared.dials).toHaveLength(1);
    await expect(afterShutdown).rejects.toBeInstanceOf(KernelDisconnectedError);
  });

  it("disconnect of an unknown name throws and leaves the map alone", async () => {
    const registry = new KernelRegistry(() => undefined);
    const first = registry.connect("a", "ws://k", 100);
    settleDial(0, fakeClient());
    await first;
    await expect(registry.disconnect("b")).rejects.toThrow("not connected");
    expect(registry.list()).toHaveLength(1);
  });
});
