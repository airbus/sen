// === semaphore.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { Semaphore } from "../../src/util/semaphore.js";

function deferred<T = void>(): { promise: Promise<T>; resolve: (v: T) => void; reject: (e: unknown) => void } {
  let resolve!: (v: T) => void;
  let reject!: (e: unknown) => void;
  const promise = new Promise<T>((res, rej) => {
    resolve = res;
    reject = rej;
  });
  return { promise, resolve, reject };
}

describe("Semaphore", () => {
  it("rejects non-positive max", () => {
    expect(() => new Semaphore(0)).toThrow();
    expect(() => new Semaphore(-1)).toThrow();
  });

  it("never exceeds the cap with synchronous-start admissions", async () => {
    const sem = new Semaphore(3);
    let active = 0;
    let peak = 0;
    const tasks = Array.from({ length: 20 }, () =>
      sem.run(async () => {
        active += 1;
        peak = Math.max(peak, active);
        await Promise.resolve();
        active -= 1;
      }),
    );
    await Promise.all(tasks);
    expect(peak).toBeLessThanOrEqual(3);
  });

  it("resists the race we hit in tools.ts: fast-path + queued resume", async () => {
    // Fill cap, queue N more, then fire one fast-path run() the instant a slot opens.
    const sem = new Semaphore(2);
    const gates = [deferred(), deferred()];
    const inProgress = new Set<number>();

    const slow = (idx: number, gate: { promise: Promise<void> }): Promise<void> =>
      sem.run(async () => {
        inProgress.add(idx);
        await gate.promise;
        inProgress.delete(idx);
      });

    const a = slow(0, gates[0]!);
    const b = slow(1, gates[1]!);

    const queued = Array.from({ length: 5 }, (_, i) =>
      sem.run(async () => {
        inProgress.add(100 + i);
        expect(inProgress.size).toBeLessThanOrEqual(2);
        await Promise.resolve();
        inProgress.delete(100 + i);
      }),
    );

    gates[0]!.resolve();
    gates[1]!.resolve();
    await a;
    await b;
    await Promise.all(queued);
    expect(inProgress.size).toBe(0);
  });

  it("releases the slot if the fn throws", async () => {
    const sem = new Semaphore(1);
    await expect(
      sem.run(async () => {
        throw new Error("boom");
      }),
    ).rejects.toThrow("boom");
    let ran = false;
    await sem.run(async () => {
      ran = true;
    });
    expect(ran).toBe(true);
  });

  it("processes the queue in FIFO order", async () => {
    const sem = new Semaphore(1);
    const order: number[] = [];
    const gate = deferred();
    const first = sem.run(async () => {
      await gate.promise;
      order.push(0);
    });
    const rest = Array.from({ length: 5 }, (_, i) =>
      sem.run(async () => {
        order.push(i + 1);
      }),
    );
    gate.resolve();
    await first;
    await Promise.all(rest);
    expect(order).toEqual([0, 1, 2, 3, 4, 5]);
  });

  it("resumes additional callers as slots free", async () => {
    const sem = new Semaphore(2);
    const completion: number[] = [];
    const gates = [deferred(), deferred(), deferred(), deferred()];
    const promises = gates.map((g, i) =>
      sem.run(async () => {
        await g.promise;
        completion.push(i);
      }),
    );

    gates[0]!.resolve();
    gates[1]!.resolve();
    await Promise.resolve();
    await new Promise((r) => setTimeout(r, 0));

    gates[2]!.resolve();
    gates[3]!.resolve();
    await Promise.all(promises);
    expect(completion).toEqual([0, 1, 2, 3]);
  });

  it("drops a queued waiter when its signal aborts; the slot is not consumed", async () => {
    const sem = new Semaphore(1);
    const holder: { resolve: () => void } = { resolve: () => undefined };
    const held = new Promise<void>((r) => {
      holder.resolve = r;
    });
    const longRun = sem.run(() => held);

    const ac = new AbortController();
    const queued = sem.run(() => Promise.resolve("queued"), ac.signal);
    ac.abort();
    await expect(queued).rejects.toMatchObject({ name: "AbortError" });

    // Aborted waiter must not consume the slot once the holder releases.
    let nextRan = false;
    const next = sem.run(() => {
      nextRan = true;
      return Promise.resolve("next");
    });
    holder.resolve();
    await longRun;
    await next;
    expect(nextRan).toBe(true);
  });

  it("rejects synchronously when the signal is pre-aborted", async () => {
    const sem = new Semaphore(1);
    const ac = new AbortController();
    ac.abort();
    await expect(sem.run(() => Promise.resolve(), ac.signal)).rejects.toMatchObject({
      name: "AbortError",
    });
  });
});
