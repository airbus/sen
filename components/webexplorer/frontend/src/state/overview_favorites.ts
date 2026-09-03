// === overview_favorites.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Class-scoped favorite marks; favoriting `Controller.power` surfaces it on every
// Controller card. Keys are `${className}.${propertyName}` or
// `${className}.${propertyName}#${leafPath}` for a specific leaf.

import { makeStore, useSelector } from "@sen/client/react";

const STORAGE_KEY = "webex.overview.favoriteProperties";
const ORDER_STORAGE_KEY = "webex.overview.favoriteOrder";

/** Empty `leafPath` marks the whole property; non-empty marks a specific leaf. */
export function favoriteKey(className: string, propertyName: string, leafPath = ""): string {
  return leafPath === "" ? `${className}.${propertyName}` : `${className}.${propertyName}#${leafPath}`;
}

// Class names are dotted (`sen.kernel.Controller`); property names are not. Split on
// the last dot before any `#`.
function splitFavoriteKey(k: string): { className: string; suffix: string } | null {
  const hash = k.indexOf("#");
  const beforeHash = hash >= 0 ? k.slice(0, hash) : k;
  const dot = beforeHash.lastIndexOf(".");
  if (dot <= 0) return null;
  return { className: k.slice(0, dot), suffix: k.slice(dot + 1) };
}

function loadFromStorage(): ReadonlySet<string> {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return new Set();
    const parsed = JSON.parse(raw) as unknown;
    if (!Array.isArray(parsed)) return new Set();
    const out = new Set<string>();
    for (const v of parsed) if (typeof v === "string") out.add(v);
    return out;
  } catch {
    return new Set();
  }
}

// Bucket by className so per-class reads stay O(1).
function buildByClass(keys: ReadonlySet<string>): ReadonlyMap<string, ReadonlySet<string>> {
  const out = new Map<string, Set<string>>();
  for (const k of keys) {
    const split = splitFavoriteKey(k);
    if (split === null) continue;
    let bucket = out.get(split.className);
    if (!bucket) {
      bucket = new Set();
      out.set(split.className, bucket);
    }
    bucket.add(split.suffix);
  }
  return out;
}

const EMPTY_CLASS_FAVORITES: ReadonlySet<string> = new Set();

// Newly favorited entries not in the order list fall to declared order at the tail.
function loadOrderFromStorage(): ReadonlyMap<string, readonly string[]> {
  try {
    const raw = localStorage.getItem(ORDER_STORAGE_KEY);
    if (!raw) return new Map();
    const parsed = JSON.parse(raw) as unknown;
    if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) return new Map();
    const out = new Map<string, readonly string[]>();
    for (const [k, v] of Object.entries(parsed as Record<string, unknown>)) {
      if (!Array.isArray(v)) continue;
      const items: string[] = [];
      for (const s of v) if (typeof s === "string") items.push(s);
      out.set(k, items);
    }
    return out;
  } catch {
    return new Map();
  }
}

const initialKeys = loadFromStorage();
const favoritesStore = makeStore<{
  keys: ReadonlySet<string>;
  /** className -> set of suffixes; per-class index of `keys`. */
  byClass: ReadonlyMap<string, ReadonlySet<string>>;
  /** className -> ordered suffix list; manual reorder from the card strip. */
  order: ReadonlyMap<string, readonly string[]>;
}>({
  keys: initialKeys,
  byClass: buildByClass(initialKeys),
  order: loadOrderFromStorage(),
});

let lastPersistedKeys = favoritesStore.getState().keys;
let lastPersistedOrder = favoritesStore.getState().order;
favoritesStore.subscribe(() => {
  const state = favoritesStore.getState();
  if (state.keys !== lastPersistedKeys) {
    lastPersistedKeys = state.keys;
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(Array.from(state.keys)));
    } catch {
    }
  }
  if (state.order !== lastPersistedOrder) {
    lastPersistedOrder = state.order;
    try {
      const obj: Record<string, readonly string[]> = {};
      for (const [k, v] of state.order) obj[k] = v;
      localStorage.setItem(ORDER_STORAGE_KEY, JSON.stringify(obj));
    } catch {
    }
  }
});

function toggle(className: string, propertyName: string, leafPath = ""): void {
  const key = favoriteKey(className, propertyName, leafPath);
  const suffix = leafPath === "" ? propertyName : `${propertyName}#${leafPath}`;
  favoritesStore.setState((prev) => {
    const nextKeys = new Set(prev.keys);
    let nextOrder = prev.order;
    // Touch only this class's bucket so per-class selectors elsewhere don't re-fire.
    const nextByClass = new Map(prev.byClass);
    const prevBucket = nextByClass.get(className);
    if (nextKeys.has(key)) {
      nextKeys.delete(key);
      if (prevBucket && prevBucket.has(suffix)) {
        if (prevBucket.size === 1) {
          nextByClass.delete(className);
        } else {
          const trimmedBucket = new Set(prevBucket);
          trimmedBucket.delete(suffix);
          nextByClass.set(className, trimmedBucket);
        }
      }
      // Drop from saved order too, or toggling off-then-on resurrects the slot.
      const existing = prev.order.get(className);
      if (existing && existing.includes(suffix)) {
        const trimmed = existing.filter((s) => s !== suffix);
        const orderMap = new Map(prev.order);
        if (trimmed.length === 0) orderMap.delete(className);
        else orderMap.set(className, trimmed);
        nextOrder = orderMap;
      }
    } else {
      nextKeys.add(key);
      const bumpedBucket = new Set(prevBucket ?? []);
      bumpedBucket.add(suffix);
      nextByClass.set(className, bumpedBucket);
    }
    return { keys: nextKeys, byClass: nextByClass, order: nextOrder };
  });
}

/** Empty `nextOrder` reverts to declared order. */
function setOrder(className: string, nextOrder: readonly string[]): void {
  favoritesStore.setState((prev) => {
    const orderMap = new Map(prev.order);
    if (nextOrder.length === 0) orderMap.delete(className);
    else orderMap.set(className, nextOrder.slice());
    return { keys: prev.keys, byClass: prev.byClass, order: orderMap };
  });
}

export const overviewFavoritesActions = Object.freeze({ toggle, setOrder });

/** Prefer `useIsOverviewFavorite` for single-key checks. */
export function useOverviewFavorites(): ReadonlySet<string> {
  return useSelector(favoritesStore, (s) => s.keys);
}

/** Empty `leafPath` = whole property. */
export function useIsOverviewFavorite(
  className: string,
  propertyName: string,
  leafPath = "",
): boolean {
  const key = favoriteKey(className, propertyName, leafPath);
  return useSelector(favoritesStore, (s) => s.keys.has(key));
}

export function useOverviewFavoriteOrder(className: string): readonly string[] | undefined {
  return useSelector(favoritesStore, (s) => s.order.get(className));
}

export function useOverviewFavoritesForClass(className: string): ReadonlySet<string> {
  return useSelector(favoritesStore, (s) => s.byClass.get(className) ?? EMPTY_CLASS_FAVORITES);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    favoritesStore.dispose();
  });
}
