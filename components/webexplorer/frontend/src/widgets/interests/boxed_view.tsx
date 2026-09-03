// === BoxedView.tsx ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client } from "@sen/client";

import { queryKey } from "../../core/keys.js";
import { type NamedQuery } from "./bus_queries.js";
import { SessionBox, type BusEntry } from "./session_box.js";

export function BoxedInterestsView({
  client,
  groupedInterests,
  collapsedSessions,
  toggleSession,
  collapsedBuses,
  toggleBusCollapsed,
  removeBusQuery,
  filterText,
  focusedQueryKey,
  clearFocus,
  popoverBus,
  setPopoverBus,
  addBusQuery,
  filteredCounts,
  reportFilteredCount,
  clearFilteredCount,
}: {
  client: Client;
  groupedInterests: Array<{ session: string; buses: BusEntry[] }>;
  collapsedSessions: Record<string, boolean>;
  toggleSession: (name: string) => void;
  collapsedBuses: Record<string, boolean>;
  toggleBusCollapsed: (bKey: string) => void;
  removeBusQuery: (bKey: string, name: string) => void;
  filterText: string;
  focusedQueryKey: string | null;
  clearFocus: () => void;
  popoverBus: string | null;
  setPopoverBus: (key: string | null) => void;
  addBusQuery: (bKey: string, sessionName: string, busName: string, q: NamedQuery) => void;
  filteredCounts: Record<string, number>;
  reportFilteredCount: (key: string, count: number) => void;
  clearFilteredCount: (key: string) => void;
}) {
  const filtering = filterText.length > 0;
  const busVisibleCount = (session: string, bus: string, queries: NamedQuery[]) =>
    queries.reduce((sum, q) => sum + (filteredCounts[queryKey(session, bus, q.name)] ?? 0), 0);
  return (
    <div style={{ padding: "6px 8px 16px", display: "flex", flexDirection: "column", gap: 10 }}>
      {groupedInterests.map(({ session, buses }) => {
        const sessionCollapsed = !!collapsedSessions[session];
        // CSS-hide (not unmount) so inner QueryRows keep reporting counts; unmounting
        // freezes the empty state and the filter can't recover from it.
        const sessionHidden =
          filtering &&
          buses.reduce((sum, b) => sum + busVisibleCount(session, b.bus, b.queries), 0) === 0;
        return (
          <SessionBox
            key={session}
            client={client}
            session={session}
            buses={buses}
            collapsed={sessionCollapsed}
            onToggleCollapsed={() => toggleSession(session)}
            hidden={sessionHidden}
            collapsedBuses={collapsedBuses}
            toggleBusCollapsed={toggleBusCollapsed}
            popoverBus={popoverBus}
            setPopoverBus={setPopoverBus}
            addBusQuery={addBusQuery}
            removeBusQuery={removeBusQuery}
            filterText={filterText}
            focusedQueryKey={focusedQueryKey}
            clearFocus={clearFocus}
            reportFilteredCount={reportFilteredCount}
            clearFilteredCount={clearFilteredCount}
            busVisibleCount={busVisibleCount}
            filtering={filtering}
          />
        );
      })}
    </div>
  );
}
