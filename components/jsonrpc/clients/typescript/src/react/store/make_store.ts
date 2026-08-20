// === make_store.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { CancelFn } from "../../values.js";

/**
 * Plain reactive store: holds a single value of type `T`, notifies subscribers when
 * setState swaps the value to a non-Object.is-equal one, and exposes the current value
 * through a stable getter.
 *
 * Lives outside React. Subscribe / read via the React hooks ({@link useStore},
 * {@link useSelector}); non-React consumers can use {@link Store.subscribe} directly.
 *
 * `T` must not be a function type (the setState updater overload uses `typeof === "function"`
 * to distinguish a value from an updater). All practical Sen-app state values are objects,
 * primitives, or arrays.
 *
 * @example
 *   const counter = makeStore({ count: 0 });
 *   counter.setState((s) => ({ count: s.count + 1 }));
 *   counter.getState(); // => { count: 1 }
 */
export interface Store<T> {
  /** Current state. Same reference until {@link setState} swaps it for a non-equal value. */
  getState(): T;
  /**
   * Replace the state. Accepts a new value directly or an updater function that receives
   * the previous state. The updater can return the previous reference (or any Object.is-equal
   * value) to no-op; in that case no listeners fire.
   */
  setState(updater: T | ((prev: T) => T)): void;
  /** Subscribe to state changes. Returns a cancel function. */
  subscribe(listener: () => void): CancelFn;
  /**
   * Clear all listeners and mark the store inert (subsequent setState calls are silently
   * ignored). Optional for module-scoped stores (they live until page unload); useful for
   * provider-scoped stores whose lifetime tracks a React component.
   */
  dispose(): void;
}

/**
 * Create a {@link Store} seeded with `initial`. The store is just a value plus a listener
 * set; no React involvement until a hook subscribes.
 */
export function makeStore<T>(initial: T): Store<T> {
  let state = initial;
  const listeners = new Set<() => void>();
  let disposed = false;

  return {
    getState: () => state,
    setState(updater) {
      if (disposed) return;
      const next =
        typeof updater === "function"
          ? (updater as (prev: T) => T)(state)
          : updater;
      if (Object.is(next, state)) return;
      state = next;
      for (const l of listeners) l();
    },
    subscribe(listener) {
      listeners.add(listener);
      return () => {
        listeners.delete(listener);
      };
    },
    dispose() {
      disposed = true;
      listeners.clear();
    },
  };
}
