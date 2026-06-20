// === make_store.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";

import { makeStore } from "../../src/react/store/make_store.js";

describe("makeStore", () => {
  it("seeds with the initial value and returns it from getState", () => {
    const store = makeStore({ count: 7 });
    expect(store.getState()).toEqual({ count: 7 });
  });

  it("setState with a value swaps state and notifies subscribers", () => {
    const store = makeStore({ count: 0 });
    const listener = vi.fn();
    store.subscribe(listener);

    store.setState({ count: 1 });
    expect(store.getState()).toEqual({ count: 1 });
    expect(listener).toHaveBeenCalledTimes(1);
  });

  it("setState with an updater function receives prev and applies its return", () => {
    const store = makeStore({ count: 5 });
    store.setState((prev) => ({ count: prev.count + 1 }));
    expect(store.getState()).toEqual({ count: 6 });
  });

  it("no-ops when the updater returns an Object.is-equal value", () => {
    const initial = { count: 0 };
    const store = makeStore(initial);
    const listener = vi.fn();
    store.subscribe(listener);

    store.setState(initial);
    expect(listener).not.toHaveBeenCalled();

    store.setState((prev) => prev);
    expect(listener).not.toHaveBeenCalled();

    expect(store.getState()).toBe(initial);
  });

  it("fires every subscribed listener on each non-no-op setState", () => {
    const store = makeStore(0);
    const a = vi.fn();
    const b = vi.fn();
    store.subscribe(a);
    store.subscribe(b);

    store.setState(1);
    store.setState(2);
    expect(a).toHaveBeenCalledTimes(2);
    expect(b).toHaveBeenCalledTimes(2);
  });

  it("subscribe returns a cancel that removes only that listener", () => {
    const store = makeStore(0);
    const kept = vi.fn();
    const dropped = vi.fn();
    store.subscribe(kept);
    const cancel = store.subscribe(dropped);

    cancel();
    store.setState(1);
    expect(kept).toHaveBeenCalledTimes(1);
    expect(dropped).not.toHaveBeenCalled();
  });

  it("dispose clears all listeners and silences subsequent setState", () => {
    const store = makeStore(0);
    const listener = vi.fn();
    store.subscribe(listener);

    store.dispose();
    store.setState(99);
    expect(listener).not.toHaveBeenCalled();
    // State stays at the last value pre-dispose; setState is a no-op after.
    expect(store.getState()).toBe(0);
  });

  it("handles primitive state values (numbers, strings)", () => {
    const numStore = makeStore(0);
    numStore.setState((n) => n + 10);
    expect(numStore.getState()).toBe(10);

    const strStore = makeStore("hello");
    const listener = vi.fn();
    strStore.subscribe(listener);
    strStore.setState("world");
    expect(strStore.getState()).toBe("world");
    expect(listener).toHaveBeenCalledTimes(1);
    strStore.setState("world"); // Object.is-equal string, no-op
    expect(listener).toHaveBeenCalledTimes(1);
  });
});
