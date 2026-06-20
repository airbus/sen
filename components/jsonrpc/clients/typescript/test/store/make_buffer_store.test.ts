// === make_buffer_store.test.ts =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";

import { makeBufferStore } from "../../src/react/store/make_buffer_store.js";

/**
 * Tests use `rafBatch: false` for deterministic synchronous flush. The rAF path is verified
 * separately in its own test using vitest fake timers — we don't want every test to depend on
 * timer mocking.
 */
function makeStore<K, V>(produce: (key: K) => V) {
  return makeBufferStore<K, V>({ produce, rafBatch: false });
}

describe("makeBufferStore", () => {
  describe("view caching", () => {
    it("caches the producer's return on first getView, returns same ref on repeat", () => {
      let produceCalls = 0;
      const store = makeStore<string, { items: number[] }>(() => {
        produceCalls++;
        return { items: [1, 2, 3] };
      });

      const a = store.getView("k");
      const b = store.getView("k");
      expect(a).toBe(b);
      expect(produceCalls).toBe(1);
    });

    it("produces a fresh view after invalidate(key)", () => {
      let produceCalls = 0;
      const store = makeStore<string, number>(() => ++produceCalls);

      const a = store.getView("k");
      store.invalidate("k");
      const b = store.getView("k");
      expect(a).not.toBe(b);
      expect(produceCalls).toBe(2);
    });

    it("dropKey clears the cache without firing listeners", () => {
      let produceCalls = 0;
      const store = makeStore<string, number>(() => ++produceCalls);
      const listener = vi.fn();
      store.subscribeKey("k", listener);

      store.getView("k");
      store.dropKey("k");
      expect(listener).not.toHaveBeenCalled();

      store.getView("k");
      expect(produceCalls).toBe(2);
    });

    it("per-key cache is independent across keys", () => {
      let calls = 0;
      const store = makeStore<string, number>(() => ++calls);
      store.getView("a");
      store.getView("b");
      store.getView("a");
      store.getView("b");
      expect(calls).toBe(2);
    });
  });

  describe("listeners", () => {
    it("invalidate(key) fires only that key's listeners + subscribeAll", () => {
      const store = makeStore<string, number>(() => 0);
      const onA = vi.fn();
      const onB = vi.fn();
      const onAll = vi.fn();
      store.subscribeKey("a", onA);
      store.subscribeKey("b", onB);
      store.subscribeAll(onAll);

      store.invalidate("a");
      expect(onA).toHaveBeenCalledTimes(1);
      expect(onB).not.toHaveBeenCalled();
      expect(onAll).toHaveBeenCalledTimes(1);
    });

    it("subscribeKey cancel removes only that listener", () => {
      const store = makeStore<string, number>(() => 0);
      const kept = vi.fn();
      const dropped = vi.fn();
      store.subscribeKey("k", kept);
      const cancel = store.subscribeKey("k", dropped);

      cancel();
      store.invalidate("k");
      expect(kept).toHaveBeenCalledTimes(1);
      expect(dropped).not.toHaveBeenCalled();
    });

    it("subscribeAll fires once per flush, not once per dirtied key", () => {
      const store = makeStore<string, number>(() => 0);
      const onAll = vi.fn();
      store.subscribeAll(onAll);

      // rafBatch is false, so each invalidate flushes synchronously.
      store.invalidate("a");
      store.invalidate("b");
      store.invalidate("c");
      // Each synchronous flush emits one global notification.
      expect(onAll).toHaveBeenCalledTimes(3);
    });

    it("version bumps once per flush (rAF-batched), not once per dirtied key", () => {
      const store = makeStore<string, number>(() => 0);
      expect(store.getVersion()).toBe(0);
      // rafBatch=false → each invalidate is its own synchronous flush.
      store.invalidate("a");
      store.invalidate("b");
      expect(store.getVersion()).toBe(2);
    });

    it("subscribeAll cancel removes only that listener", () => {
      const store = makeStore<string, number>(() => 0);
      const kept = vi.fn();
      const dropped = vi.fn();
      store.subscribeAll(kept);
      const cancel = store.subscribeAll(dropped);
      cancel();
      store.invalidate("anything");
      expect(kept).toHaveBeenCalledTimes(1);
      expect(dropped).not.toHaveBeenCalled();
    });

    it("throwing listener does not abort sibling per-key or subscribeAll fan-out", () => {
      const store = makeStore<string, number>(() => 0);
      const errSpy = vi.spyOn(console, "error").mockImplementation(() => {});
      const throwingA = vi.fn(() => {
        throw new Error("boom");
      });
      const siblingA = vi.fn();
      const onB = vi.fn();
      const onAll = vi.fn();
      store.subscribeKey("a", throwingA);
      store.subscribeKey("a", siblingA);
      store.subscribeKey("b", onB);
      store.subscribeAll(onAll);

      store.invalidate("a");
      store.invalidate("b");

      expect(throwingA).toHaveBeenCalledTimes(1);
      expect(siblingA).toHaveBeenCalledTimes(1);
      expect(onB).toHaveBeenCalledTimes(1);
      expect(onAll).toHaveBeenCalledTimes(2);
      expect(errSpy).toHaveBeenCalled();
      errSpy.mockRestore();
    });
  });

  describe("rAF batching", () => {
    it("coalesces multiple invalidates within one frame into a single flush", () => {
      // The node test env has no global rAF; install a controllable one so we can observe
      // batching. We define + delete on globalThis (rather than spyOn) because spyOn
      // requires the property to already exist.
      let pending: (() => void) | null = null;
      const raf = vi.fn((cb: FrameRequestCallback) => {
        pending = () => cb(0);
        return 1;
      });
      const cancelRaf = vi.fn(() => {
        pending = null;
      });
      (globalThis as unknown as { requestAnimationFrame: typeof raf }).requestAnimationFrame = raf;
      (globalThis as unknown as { cancelAnimationFrame: typeof cancelRaf }).cancelAnimationFrame = cancelRaf;

      const store = makeBufferStore<string, number>({
        produce: () => 0,
        rafBatch: true,
      });
      const onA = vi.fn();
      const onB = vi.fn();
      const onAll = vi.fn();
      store.subscribeKey("a", onA);
      store.subscribeKey("b", onB);
      store.subscribeAll(onAll);

      // Three invalidates in the same frame: one rAF scheduled.
      store.invalidate("a");
      store.invalidate("b");
      store.invalidate("a"); // dup
      expect(raf).toHaveBeenCalledTimes(1);
      expect(onA).not.toHaveBeenCalled();
      expect(onB).not.toHaveBeenCalled();
      expect(onAll).not.toHaveBeenCalled();

      // Drain the pending rAF: per-key listeners fire once each, global fires once.
      pending!();
      expect(onA).toHaveBeenCalledTimes(1);
      expect(onB).toHaveBeenCalledTimes(1);
      expect(onAll).toHaveBeenCalledTimes(1);

      delete (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
      delete (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
    });

    it("falls back to setTimeout when requestAnimationFrame is unavailable", () => {
      // Vitest's node env has no global rAF; setTimeout is the documented fallback.
      vi.useFakeTimers();
      const store = makeBufferStore<string, number>({
        produce: () => 0,
        rafBatch: true,
      });
      const onAll = vi.fn();
      store.subscribeAll(onAll);

      store.invalidate("k");
      expect(onAll).not.toHaveBeenCalled();

      vi.advanceTimersByTime(20);
      expect(onAll).toHaveBeenCalledTimes(1);
      vi.useRealTimers();
    });

    it("dispose cancels any pending rAF and silences subsequent invalidates", () => {
      let pending: (() => void) | null = null;
      const raf = vi.fn((cb: FrameRequestCallback) => {
        pending = () => cb(0);
        return 42;
      });
      const cancelRaf = vi.fn(() => {
        pending = null;
      });
      (globalThis as unknown as { requestAnimationFrame: typeof raf }).requestAnimationFrame = raf;
      (globalThis as unknown as { cancelAnimationFrame: typeof cancelRaf }).cancelAnimationFrame = cancelRaf;

      const store = makeBufferStore<string, number>({
        produce: () => 0,
        rafBatch: true,
      });
      const onAll = vi.fn();
      store.subscribeAll(onAll);

      store.invalidate("k");
      expect(pending).not.toBeNull();
      store.dispose();
      expect(cancelRaf).toHaveBeenCalledWith(42);

      // Post-dispose invalidate is a no-op: it schedules nothing and fires no listeners.
      store.invalidate("k");
      expect(onAll).not.toHaveBeenCalled();
      expect(raf).toHaveBeenCalledTimes(1);

      delete (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
      delete (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
    });
  });
});
