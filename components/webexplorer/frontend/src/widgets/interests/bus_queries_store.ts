// === bus_queries_store.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Module-scope so the app-root InterestOwner can subscribe to it regardless of
// which view is mounted; pane-local state would destroy backend declarations on unmount.

import { makeStore, useSelector } from "@sen/client/react";

import { loadBusQueries, saveBusQueries, type NamedQuery } from "./bus_queries.js";

interface BusQueriesState {
  readonly entries: Record<string, NamedQuery[]>;
}

const busQueriesStore = makeStore<BusQueriesState>({ entries: loadBusQueries() });

// Identity comparison suffices: actions return a fresh entries object only on real change.
let lastPersisted = busQueriesStore.getState().entries;
busQueriesStore.subscribe(() => {
  const current = busQueriesStore.getState().entries;
  if (current === lastPersisted) return;
  lastPersisted = current;
  saveBusQueries(current);
});

export const busQueriesActions = Object.freeze({
  add: (bKey: string, query: NamedQuery) => {
    busQueriesStore.setState((prev) => {
      const existing = prev.entries[bKey] ?? [];
      if (existing.some((e) => e.name === query.name)) return prev;
      return { entries: { ...prev.entries, [bKey]: [...existing, query] } };
    });
  },
  remove: (bKey: string, name: string) => {
    busQueriesStore.setState((prev) => {
      const existing = prev.entries[bKey] ?? [];
      const next = existing.filter((q) => q.name !== name);
      if (next.length === existing.length) return prev;
      return { entries: { ...prev.entries, [bKey]: next } };
    });
  },
  clearAll: () => {
    busQueriesStore.setState((prev) => {
      let changed = false;
      const next: Record<string, NamedQuery[]> = {};
      for (const [bKey, list] of Object.entries(prev.entries)) {
        if (list.length > 0) changed = true;
        next[bKey] = [];
      }
      return changed ? { entries: next } : prev;
    });
  },
});

export function useBusQueries(): Record<string, NamedQuery[]> {
  return useSelector(busQueriesStore, (s) => s.entries);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    busQueriesStore.dispose();
  });
}
