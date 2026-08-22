// === kernel_events.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// The interest handle is a stand-in whose acks the test settles by hand, so each subscribe /
// unsubscribe / shutdown interleaving below is exact rather than timing-dependent.

import { getEventListeners, getMaxListeners } from "node:events";
import { describe, expect, it } from "vitest";
import type { CancelFn, EventTriggeredHandler, InterestHandle, Var } from "@sen/client";
import { EVENT_BUFFER_CAP, INTEREST_CAP_PER_KERNEL, Kernel } from "../../src/kernel.js";
import { KernelDisconnectedError } from "../../src/errors.js";

const INTEREST = "i";
const OBJECT = "obj";
const EVENT = "evt";

interface Subscription {
  eventName: string;
  handler: EventTriggeredHandler;
  cancelled: number;
}

interface Ack {
  resolve: () => void;
  reject: (err: unknown) => void;
}

class FakeObject {
  readonly subscriptions: Subscription[] = [];
  readonly acks: Ack[] = [];

  onEventTriggered(eventName: string, handler: EventTriggeredHandler): CancelFn {
    const entry: Subscription = { eventName, handler, cancelled: 0 };
    this.subscriptions.push(entry);
    return () => {
      entry.cancelled += 1;
    };
  }

  awaitEventSubscribed(): Promise<void> {
    let resolve!: () => void;
    let reject!: (err: unknown) => void;
    const promise = new Promise<void>((res, rej) => {
      resolve = res;
      reject = rej;
    });
    this.acks.push({ resolve, reject });
    return promise;
  }

  get live(): Subscription[] {
    return this.subscriptions.filter((s) => s.cancelled === 0);
  }
}

function makeKernel(): { kernel: Kernel; obj: FakeObject } {
  const obj = new FakeObject();
  const handle = { objectByName: (name: string) => (name === OBJECT ? obj : undefined) };
  const kernel = new Kernel("k", "ws://k", 100, () => undefined);
  kernel.interests.set(INTEREST, handle as unknown as InterestHandle);
  return { kernel, obj };
}

// One macrotask turn drains every microtask, so all continuations queued by the settles above
// it have run by the time this resolves.
function flush(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 0));
}

// Settling by hand means a rejection can outrun the assertion; capture the outcome up front.
function outcome<T>(promise: Promise<T>): Promise<{ value: T } | { error: unknown }> {
  return promise.then(
    (value) => ({ value }),
    (error: unknown) => ({ error }),
  );
}

function abortListenerCount(kernel: Kernel): number {
  return getEventListeners(kernel.signal, "abort").length;
}

// Collects process warnings, displacing Node's own handler so an expected warning doesn't print
// into the suite output and read as a failure.
async function captureWarnings(run: () => void): Promise<string[]> {
  const displaced = process.listeners("warning");
  const seen: string[] = [];
  const record = (warning: Error): void => {
    seen.push(warning.name);
  };
  process.removeAllListeners("warning");
  process.on("warning", record);
  try {
    run();
    await flush();
  } finally {
    process.off("warning", record);
    for (const listener of displaced) process.on("warning", listener);
  }
  return seen;
}

describe("Kernel.subscribeToEvent", () => {
  it("reports a duplicate subscribe on a live subscription as not new", async () => {
    const { kernel, obj } = makeKernel();
    const first = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    obj.acks[0]!.resolve();
    expect(await first).toBe(true);

    expect(await kernel.subscribeToEvent(INTEREST, OBJECT, EVENT)).toBe(false);
    expect(obj.subscriptions).toHaveLength(1);
  });

  it("subscribes afresh when the subscription it waited on was torn down", async () => {
    const { kernel, obj } = makeKernel();
    const first = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    const rider = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);

    expect(kernel.unsubscribeFromEvent(INTEREST, OBJECT, EVENT)).toBe(true);
    obj.acks[0]!.resolve();
    expect(await first).toBe(true);
    await flush();

    // Answering false here would leave the caller polling a buffer nothing writes to.
    expect(obj.subscriptions).toHaveLength(2);
    obj.acks[1]!.resolve();
    expect(await rider).toBe(true);
    expect(obj.live).toHaveLength(1);
  });

  it("does not park a new subscriber on the ack of a subscription already unsubscribed", async () => {
    const { kernel, obj } = makeKernel();
    const first = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    expect(kernel.unsubscribeFromEvent(INTEREST, OBJECT, EVENT)).toBe(true);

    // The stale ack is never settled: a subscriber that waited on it would hang.
    const second = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    expect(obj.subscriptions).toHaveLength(2);
    obj.acks[1]!.resolve();
    expect(await second).toBe(true);

    obj.acks[0]!.resolve();
    expect(await first).toBe(true);
  });

  it("fails the waiter too when shutdown wins the race", async () => {
    const { kernel, obj } = makeKernel();
    const first = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));
    const rider = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));

    await kernel.shutdown();
    await flush();

    expect(((await first) as { error?: unknown }).error).toBeInstanceOf(KernelDisconnectedError);
    // The rider must hear about the shutdown, not be told it is already subscribed.
    expect(((await rider) as { error?: unknown }).error).toBeInstanceOf(KernelDisconnectedError);
    expect(obj.live).toHaveLength(0);
  });

  it("refuses to resubscribe through an interest released while it waited", async () => {
    const { kernel, obj } = makeKernel();
    const first = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    const rider = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));

    // What releaseInterest does before it awaits the handle.
    kernel.tearDownInterestEvents(INTEREST);
    kernel.interests.delete(INTEREST);

    obj.acks[0]!.resolve();
    await first;
    expect(((await rider) as { error?: unknown }).error).toBeInstanceOf(Error);
    expect(obj.subscriptions).toHaveLength(1);
  });

  it("cleans up after a rejected ack and lets a later subscribe succeed", async () => {
    const { kernel, obj } = makeKernel();
    const first = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));
    obj.acks[0]!.reject(new Error("subscribe refused"));
    expect((await first) as { error?: unknown }).toHaveProperty("error");
    expect(obj.live).toHaveLength(0);

    const second = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    obj.acks[1]!.resolve();
    expect(await second).toBe(true);
    expect(obj.live).toHaveLength(1);
  });

  it("keeps the entries of a resubscribe that replaced a cancelled one", async () => {
    const { kernel, obj } = makeKernel();
    const first = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));
    kernel.unsubscribeFromEvent(INTEREST, OBJECT, EVENT);
    const second = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    obj.acks[1]!.resolve();
    expect(await second).toBe(true);

    // The first subscribe failing must not retract the replacement's cancel handle.
    obj.acks[0]!.reject(new Error("subscribe refused"));
    await first;
    expect(kernel.unsubscribeFromEvent(INTEREST, OBJECT, EVENT)).toBe(true);
    expect(obj.live).toHaveLength(0);
  });
});

describe("Kernel abort listeners", () => {
  it("detaches the abort listener when the ack wins", async () => {
    const { kernel, obj } = makeKernel();
    for (let i = 0; i < 20; i++) {
      const pending = kernel.subscribeToEvent(INTEREST, OBJECT, `evt${i}`);
      obj.acks[i]!.resolve();
      expect(await pending).toBe(true);
    }
    // The signal lives as long as the kernel, so whatever stays attached is retained for that
    // whole lifetime - one rejection closure per subscribe, on a path clients hit repeatedly.
    expect(abortListenerCount(kernel)).toBe(0);
  });

  it("detaches across subscribe/unsubscribe churn", async () => {
    const { kernel, obj } = makeKernel();
    for (let i = 0; i < 20; i++) {
      const pending = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
      obj.acks[i]!.resolve();
      expect(await pending).toBe(true);
      expect(kernel.unsubscribeFromEvent(INTEREST, OBJECT, EVENT)).toBe(true);
    }
    expect(abortListenerCount(kernel)).toBe(0);
  });

  it("sets a finite ceiling: quiet under healthy traffic, still loud on a leak", async () => {
    const ceiling = getMaxListeners(makeKernel().kernel.signal);
    expect(Number.isFinite(ceiling) && ceiling > 0).toBe(true);
    // Subscriptions are per interest per event name, so clearing the interest cap is not
    // enough: a heavy deployment at five event names per interest must stay quiet too, or the
    // first spurious warning teaches whoever reads it to ignore the real one.
    const heavyDeployment = INTEREST_CAP_PER_KERNEL * 5;
    expect(ceiling).toBeGreaterThan(heavyDeployment);

    const healthy = makeKernel().kernel;
    const quiet = await captureWarnings(() => {
      for (let i = 0; i < heavyDeployment; i++) {
        healthy.signal.addEventListener("abort", () => undefined);
      }
    });
    expect(quiet).not.toContain("MaxListenersExceededWarning");

    // Unlimited would pass the line above just as well. This is the half that says a leak
    // reintroduced later still announces itself.
    const leaking = makeKernel().kernel;
    const loud = await captureWarnings(() => {
      for (let i = 0; i <= ceiling; i++) leaking.signal.addEventListener("abort", () => undefined);
    });
    expect(loud).toContain("MaxListenersExceededWarning");
  });

  it("detaches when the ack rejects", async () => {
    const { kernel, obj } = makeKernel();
    for (let i = 0; i < 5; i++) {
      const pending = outcome(kernel.subscribeToEvent(INTEREST, OBJECT, EVENT));
      obj.acks[i]!.reject(new Error("subscribe refused"));
      await pending;
    }
    expect(abortListenerCount(kernel)).toBe(0);
  });
});

describe("Kernel event buffering", () => {
  it("bounds the buffer by payload size, not just entry count", async () => {
    const { kernel, obj } = makeKernel();
    const pending = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    obj.acks[0]!.resolve();
    await pending;

    const handler = obj.subscriptions[0]!.handler;
    const pushes = 40;
    for (let i = 0; i < pushes; i++) {
      const args: Var[] = [{ blob: "x".repeat(512 * 1024) }];
      handler(args, { timestamp: `t${i}` });
    }

    const drained = kernel.drainEvents(INTEREST);
    expect(drained.length).toBeGreaterThan(0);
    expect(drained.length).toBeLessThan(pushes);
    expect(drained[drained.length - 1]!.timestamp).toBe(`t${pushes - 1}`);
  });

  it("still bounds by entry count for small payloads", async () => {
    const { kernel, obj } = makeKernel();
    const pending = kernel.subscribeToEvent(INTEREST, OBJECT, EVENT);
    obj.acks[0]!.resolve();
    await pending;

    const handler = obj.subscriptions[0]!.handler;
    for (let i = 0; i < EVENT_BUFFER_CAP + 10; i++) handler([i], { timestamp: `t${i}` });

    const drained = kernel.drainEvents(INTEREST);
    expect(drained).toHaveLength(EVENT_BUFFER_CAP);
    expect(drained[0]!.timestamp).toBe("t10");
  });
});
