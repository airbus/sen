// === make_buffer_store.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { CancelFn } from "../../values.js";

/**
 * Per-key cached view store over mutable backing data, with rAF-coalesced notifications.
 *
 * Designed for high-frequency wire deliveries (property changes, event triggers, sample
 * pushes) where the underlying buffers are mutated in place and consumers need a snapshot-
 * style view that satisfies React 18's `useSyncExternalStore` stability requirements.
 *
 * The store does not own the backing data; the consumer manages its own storage and calls
 * {@link BufferStore.invalidate} when a key's data has changed. The store caches the result
 * of `produce(key)` until invalidation; the cached reference is what every consumer reads.
 *
 * `subscribeKey(key, fn)` fires when `key` is invalidated. `subscribeAll(fn)` fires once
 * per flush (one rAF) regardless of how many keys were dirtied within that frame. Both are
 * used by the React hooks ({@link useView}, {@link useViews}).
 *
 * @example
 *   const buffers = new Map<string, number[]>();
 *   const store = makeBufferStore<string, readonly number[]>({
 *     produce: (key) => buffers.get(key) ?? [],
 *   });
 *   // High-frequency append from wire delivery:
 *   buffers.get("x")!.push(value);
 *   store.invalidate("x"); // coalesced into next rAF flush
 */
export interface BufferStoreOptions<K, V> {
  /**
   * Compute a view of `key`'s current backing data. Called on cache miss (first read after
   * creation or after {@link BufferStore.invalidate}). Should be cheap: typically wraps the
   * live mutable backing without copying. Consumers treat the returned `V` as immutable by
   * convention.
   */
  produce: (key: K) => V;
  /**
   * Coalesce {@link BufferStore.invalidate} notifications into a single per-frame flush.
   * Default: true. Set false for tests, SSR, or any non-browser environment without
   * `requestAnimationFrame`; in that mode every `invalidate` fires listeners synchronously.
   */
  rafBatch?: boolean;
}

/** Per-key cached view store. See {@link makeBufferStore}. */
export interface BufferStore<K, V> {
  /**
   * Return the cached view for `key`, computing it via the producer on cache miss. Same
   * reference is returned for repeated calls until {@link invalidate} or {@link dropKey}
   * is called for that key.
   */
  getView(key: K): V;
  /**
   * Mark `key`'s cached view as invalid. The cache entry is cleared on the next flush; on
   * that same flush, per-key and global listeners fire. Default mode (`rafBatch: true`)
   * batches all invalidations within a frame into one flush; set false at construction to
   * flush synchronously.
   */
  invalidate(key: K): void;
  /** Subscribe to invalidations of a specific key. */
  subscribeKey(key: K, listener: () => void): CancelFn;
  /**
   * Subscribe to any invalidation. Fires once per flush regardless of how many keys were
   * dirtied. Used for aggregate views (e.g. multi-key snapshots).
   */
  subscribeAll(listener: () => void): CancelFn;
  /**
   * Drop the cache entry for `key` without firing listeners. Used when the underlying
   * backing for `key` has been released (eviction, unsubscribe). After dropKey, the next
   * `getView(key)` re-runs the producer.
   */
  dropKey(key: K): void;
  /**
   * Current store version. Bumps once per flush (once per rAF batch regardless of how many
   * keys were dirtied). Hooks that want to short-circuit per-render work without
   * re-walking key sets (see {@link useViews}) cache this and skip when the version is
   * unchanged.
   */
  getVersion(): number;
  /** Cancel any pending rAF flush, clear all caches and listeners. */
  dispose(): void;
}

/**
 * Create a {@link BufferStore}. The producer is the only mandatory option; everything else
 * has working defaults aimed at browser high-frequency consumers.
 */
export function makeBufferStore<K, V>(opts: BufferStoreOptions<K, V>): BufferStore<K, V> {
  const cache = new Map<K, V>();
  const keyListeners = new Map<K, Set<() => void>>();
  const allListeners = new Set<() => void>();
  const dirty = new Set<K>();
  let rafId: number | null = null;
  let timeoutId: ReturnType<typeof setTimeout> | null = null;
  let disposed = false;
  let version = 0;
  const rafBatch = opts.rafBatch ?? true;

  const fireSafely = (l: () => void): void => {
    try {
      l();
    } catch (err) {
      // One subscriber's exception must not break the rest of the flush fan-out. Logged
      // (not swallowed) so the bug stays visible during development.
      console.error("makeBufferStore: listener threw during flush", err);
    }
  };

  const flush = (): void => {
    rafId = null;
    timeoutId = null;
    if (dirty.size === 0) return;
    // Bump version BEFORE notifications fire so subscribers that read `getVersion()` from
    // inside their listener see the post-flush value.
    version++;
    const drained = Array.from(dirty);
    dirty.clear();
    for (const key of drained) {
      cache.delete(key);
      const set = keyListeners.get(key);
      if (set) for (const l of set) fireSafely(l);
    }
    // Global listeners fire once after all per-key fan-out so a consumer of subscribeAll
    // sees one notification per flush, not one per dirtied key.
    for (const l of allListeners) fireSafely(l);
  };

  const schedule = (): void => {
    if (!rafBatch) {
      flush();
      return;
    }
    if (rafId !== null || timeoutId !== null) return;
    if (typeof requestAnimationFrame === "function") {
      rafId = requestAnimationFrame(flush);
    } else {
      // Non-browser environment (Node test runner, SSR). Match rAF's 60-Hz cadence so timer
      // tests behave like the browser path.
      timeoutId = setTimeout(flush, 16);
    }
  };

  return {
    getView(key) {
      if (cache.has(key)) return cache.get(key) as V;
      const view = opts.produce(key);
      cache.set(key, view);
      return view;
    },
    invalidate(key) {
      if (disposed) return;
      dirty.add(key);
      schedule();
    },
    subscribeKey(key, listener) {
      let set = keyListeners.get(key);
      if (!set) {
        set = new Set();
        keyListeners.set(key, set);
      }
      set.add(listener);
      return () => {
        const s = keyListeners.get(key);
        if (!s) return;
        s.delete(listener);
        if (s.size === 0) keyListeners.delete(key);
      };
    },
    subscribeAll(listener) {
      allListeners.add(listener);
      return () => {
        allListeners.delete(listener);
      };
    },
    dropKey(key) {
      cache.delete(key);
    },
    getVersion() {
      return version;
    },
    dispose() {
      disposed = true;
      if (rafId !== null && typeof cancelAnimationFrame === "function") {
        cancelAnimationFrame(rafId);
        rafId = null;
      }
      if (timeoutId !== null) {
        clearTimeout(timeoutId);
        timeoutId = null;
      }
      cache.clear();
      keyListeners.clear();
      allListeners.clear();
      dirty.clear();
    },
  };
}
