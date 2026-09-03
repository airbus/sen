// === semaphore.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Admit-and-increment so concurrent fast-path callers can't race a queued waiter past the
// cap. FIFO queue. Pass `signal` to run() so a cancelled waiter drops out of the queue
// without holding a slot.
export class Semaphore {
  private active = 0;
  private readonly queue: Array<() => void> = [];
  private readonly idleWaiters: Array<() => void> = [];

  constructor(private readonly max: number) {
    if (max <= 0) throw new Error(`Semaphore max must be positive (got ${max})`);
  }

  async run<T>(fn: () => Promise<T>, signal?: AbortSignal): Promise<T> {
    if (signal?.aborted) throw makeAbortError();
    if (this.active < this.max) {
      this.active += 1;
    } else {
      await new Promise<void>((resolve, reject) => {
        const entry = (): void => {
          if (signal !== undefined) signal.removeEventListener("abort", onAbort);
          this.active += 1;
          resolve();
        };
        const onAbort = (): void => {
          const idx = this.queue.indexOf(entry);
          if (idx >= 0) this.queue.splice(idx, 1);
          reject(makeAbortError());
        };
        signal?.addEventListener("abort", onAbort, { once: true });
        this.queue.push(entry);
      });
    }
    try {
      return await fn();
    } finally {
      this.active -= 1;
      const next = this.queue.shift();
      if (next !== undefined) {
        next();
      } else if (this.active === 0) {
        const waiters = this.idleWaiters.splice(0);
        for (const w of waiters) w();
      }
    }
  }

  // Idle at call time resolves synchronously; otherwise the next transition to all-done
  // wakes every waiter.
  awaitIdle(): Promise<void> {
    if (this.active === 0 && this.queue.length === 0) return Promise.resolve();
    return new Promise<void>((resolve) => this.idleWaiters.push(resolve));
  }
}

function makeAbortError(): Error {
  const err = new Error("aborted");
  err.name = "AbortError";
  return err;
}
