// === use_view.ts =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useRef, useSyncExternalStore } from "react";

import type { BufferStore } from "./make_buffer_store.js";

/**
 * Subscribe to one key's view from a {@link BufferStore}. Re-renders when the store
 * invalidates `key`. Passing `null` short-circuits to undefined and a no-op subscription,
 * useful for conditional reads (e.g. `useView(store, name ? name : null)`).
 *
 * @example
 *   const samples = useView(sampleStore, propertyKey);
 *   // samples is the BufferView for that key, or undefined when propertyKey is null.
 */
export function useView<K, V>(store: BufferStore<K, V>, key: K | null): V | undefined {
  const subscribe = useCallback(
    (listener: () => void) => {
      if (key === null) return () => undefined;
      return store.subscribeKey(key, listener);
    },
    [store, key],
  );
  const getSnapshot = useCallback((): V | undefined => {
    if (key === null) return undefined;
    return store.getView(key);
  }, [store, key]);
  return useSyncExternalStore(subscribe, getSnapshot);
}

/**
 * Subscribe to a set of keys from a {@link BufferStore}. Returns a Map keyed by `K` whose
 * values are the per-key views; per-key view identities follow the store's cache, so
 * unchanged keys keep stable references across renders.
 *
 * The returned Map identity stays stable across notifications that don't change any of the
 * requested keys' values. The Map is rebuilt only when at least one key's view actually
 * changed (or when the keys list contents change).
 *
 * Callers should memoize the `keys` array if it's computed from another collection;
 * passing a fresh array per render still works but skips the Map-identity-stability
 * optimization (the hook walks the keys to detect changes regardless).
 *
 * @example
 *   const keys = useMemo(() => series.map(seriesKey), [series]);
 *   const views = useViews(sampleStore, keys);
 *   for (const k of keys) {
 *     const v = views.get(k);
 *     // ...
 *   }
 */
export function useViews<K, V>(store: BufferStore<K, V>, keys: readonly K[]): ReadonlyMap<K, V> {
  // Track the latest keys via ref so an inline-array caller doesn't re-subscribe to
  // subscribeAll on every render. subscribe identity only flips on store change.
  const keysRef = useRef(keys);
  keysRef.current = keys;

  const subscribe = useCallback(
    (listener: () => void) => store.subscribeAll(listener),
    [store],
  );

  // Map cache: returned Map is reused as long as every requested key's view is identical
  // to the previous render's.
  //
  // Two-tier short-circuit:
  //  - **Version + keys-ref fast path**: when the store version is unchanged AND the caller
  //    passed the same keys array as last render, return the cached Map without walking
  //    the keys list. This is the dominant case (parent re-renders without any store
  //    notification, e.g. unrelated state change in a sibling).
  //  - **Walk-detection path**: when version OR keys-ref changed, walk the keys to compare
  //    each cached value against the store's current view. Allocate a fresh Map only when
  //    something actually changed.
  const memoRef = useRef<{
    map: ReadonlyMap<K, V>;
    version: number;
    keys: readonly K[];
  } | null>(null);

  const getSnapshot = useCallback((): ReadonlyMap<K, V> => {
    const ks = keysRef.current;
    const currentVersion = store.getVersion();
    const cached = memoRef.current;
    // Fast path: nothing has changed since the last render.
    if (cached !== null && cached.version === currentVersion && cached.keys === ks) {
      return cached.map;
    }
    // Walk-detection: either the store changed or the keys array did.
    let needsRebuild = cached === null || cached.map.size !== ks.length;
    if (!needsRebuild) {
      for (const k of ks) {
        if ((cached as { map: ReadonlyMap<K, V> }).map.get(k) !== store.getView(k)) {
          needsRebuild = true;
          break;
        }
      }
    }
    if (!needsRebuild) {
      // Walk confirmed no change. Refresh the cached version + keys ref so the fast path
      // hits next time, but reuse the Map.
      const cachedMap = (cached as { map: ReadonlyMap<K, V> }).map;
      memoRef.current = { map: cachedMap, version: currentVersion, keys: ks };
      return cachedMap;
    }
    const next = new Map<K, V>();
    for (const k of ks) next.set(k, store.getView(k));
    memoRef.current = { map: next, version: currentVersion, keys: ks };
    return next;
  }, [store]);

  return useSyncExternalStore(subscribe, getSnapshot);
}
