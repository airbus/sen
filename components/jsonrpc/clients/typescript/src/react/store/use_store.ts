// === use_store.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useRef, useSyncExternalStore } from "react";

import type { Store } from "./make_store.js";

/**
 * Subscribe to the whole state. The component re-renders whenever the store's state
 * reference changes (`setState` calls that pass Object.is do not trigger re-renders).
 *
 * Prefer {@link useSelector} when reading a slice; selectors avoid re-rendering on state
 * changes that don't affect the slice you actually consume.
 */
export function useStore<T>(store: Store<T>): T {
  return useSyncExternalStore(store.subscribe, store.getState);
}

/**
 * Memo-cell holding the last (state, selector, selected) triple the selector produced.
 * Exposed for tests; the hook below threads it through a ref.
 *
 * Invalidation contract:
 *  - If both `state` AND the `selector` reference are unchanged, return the cached
 *    `selected` directly. (StrictMode-safe.)
 *  - Otherwise the selector must run again. Inline-arrow selectors get a fresh closure
 *    every render, and that closure may capture changed values (e.g. a different `key`).
 *    Skipping the run because `state` alone is unchanged returns a stale slice keyed on
 *    a previous capture.
 *  - After re-running, if the new output is value-equal to the prior `selected`, keep the
 *    prior `selected` reference but refresh the cached state + selector.
 *
 * @internal
 */
export function __selectorMemoStep<T, S>(
  state: T,
  selector: (state: T) => S,
  memo: { state: T; selector: (state: T) => S; selected: S } | null,
): { state: T; selector: (state: T) => S; selected: S } {
  if (
    memo !== null &&
    Object.is(memo.state, state) &&
    Object.is(memo.selector, selector)
  ) {
    return memo;
  }
  const next = selector(state);
  if (memo !== null && Object.is(memo.selected, next)) {
    return { state, selector, selected: memo.selected };
  }
  return { state, selector, selected: next };
}

/**
 * Subscribe to a slice of state via a selector. Re-renders only when the selector's output
 * changes by Object.is.
 *
 * The selector is called on every notification AND on every render; keep it cheap and pure
 * (no side effects, no allocations of fresh references for unchanged input). Returning the
 * same reference for value-equal results is what makes this hook efficient: a selector that
 * returns `s.someBoolean` only re-renders consumers when that boolean actually flips, even
 * if other parts of state change every frame.
 *
 * Selectors can change identity across renders (an inline arrow is fine); the hook tracks
 * the latest selector via a ref so a per-render arrow doesn't trigger re-subscription.
 *
 * @example
 *   const userName = useSelector(authStore, (s) => s.user?.name ?? "guest");
 */
export function useSelector<T, S>(store: Store<T>, selector: (state: T) => S): S {
  // Track the latest selector across renders so per-render-arrow closures don't break
  // memoization. We read it through the ref inside getSnapshot.
  const selectorRef = useRef(selector);
  selectorRef.current = selector;

  // Memo cell: see __selectorMemoStep above.
  const memoRef = useRef<{ state: T; selector: (state: T) => S; selected: S } | null>(null);

  const getSnapshot = useCallback((): S => {
    memoRef.current = __selectorMemoStep(
      store.getState(),
      selectorRef.current,
      memoRef.current,
    );
    return memoRef.current.selected;
  }, [store]);

  return useSyncExternalStore(store.subscribe, getSnapshot);
}
