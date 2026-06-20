// === watch_group_order.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Manual order for the Watch tab's object-group cards. Saved order wins; groups not
// in the list fall back to natural (insertion) order. Group keys are
// `${objectName}\0${sessionName}\0${busName}` (matches watch_groups.tsx).

import { makeStore, useSelector } from "@sen/client/react";

const STORAGE_KEY = "webex.watch.groupOrder";

function loadFromStorage(): readonly string[] {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw) as unknown;
    if (!Array.isArray(parsed)) return [];
    return parsed.filter((s): s is string => typeof s === "string");
  } catch {
    return [];
  }
}

const orderStore = makeStore<{ order: readonly string[] }>({ order: loadFromStorage() });

let lastPersisted = orderStore.getState().order;
orderStore.subscribe(() => {
  const current = orderStore.getState().order;
  if (current === lastPersisted) return;
  lastPersisted = current;
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(current));
  } catch {
  }
});

function setOrder(nextOrder: readonly string[]): void {
  orderStore.setState((prev) =>
    prev.order.length === nextOrder.length && prev.order.every((k, i) => k === nextOrder[i])
      ? prev
      : { order: nextOrder.slice() },
  );
}

export const watchGroupOrderActions = Object.freeze({ setOrder });

/** Empty until the user reorders; grid then falls back to natural order. */
export function useWatchGroupOrder(): readonly string[] {
  return useSelector(orderStore, (s) => s.order);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    orderStore.dispose();
  });
}
