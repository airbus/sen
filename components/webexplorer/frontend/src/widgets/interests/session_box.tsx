// === SessionBox.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client } from "@sen/client";

import { Collapsible } from "../../ui/layout.js";
import { SessionIcon } from "../../ui/icons.js";
import { BusBox } from "./bus_box.js";
import { Caret, SessionName, TypeTag } from "./icons.js";
import { busKey } from "../../core/keys.js";
import { type NamedQuery } from "./bus_queries.js";

export interface BusEntry {
  bus: string;
  queries: NamedQuery[];
}

export function SessionBox({
  client,
  session,
  buses,
  collapsed,
  onToggleCollapsed,
  hidden,
  collapsedBuses,
  toggleBusCollapsed,
  popoverBus,
  setPopoverBus,
  addBusQuery,
  removeBusQuery,
  filterText,
  focusedQueryKey,
  clearFocus,
  reportFilteredCount,
  clearFilteredCount,
  busVisibleCount,
  filtering,
}: {
  client: Client;
  session: string;
  buses: BusEntry[];
  collapsed: boolean;
  onToggleCollapsed: () => void;
  hidden: boolean;
  collapsedBuses: Record<string, boolean>;
  toggleBusCollapsed: (bKey: string) => void;
  popoverBus: string | null;
  setPopoverBus: (key: string | null) => void;
  addBusQuery: (bKey: string, sessionName: string, busName: string, q: NamedQuery) => void;
  removeBusQuery: (bKey: string, name: string) => void;
  filterText: string;
  focusedQueryKey: string | null;
  clearFocus: () => void;
  reportFilteredCount: (key: string, count: number) => void;
  clearFilteredCount: (key: string) => void;
  busVisibleCount: (session: string, bus: string, queries: NamedQuery[]) => number;
  filtering: boolean;
}) {
  return (
    <div style={{ display: hidden ? "none" : "block" }}>
      <div
        className="property-row"
        onClick={onToggleCollapsed}
        style={{
          display: "flex",
          alignItems: "center",
          gap: 6,
          padding: "4px 4px 4px 0",
          cursor: "pointer",
          userSelect: "none",
        }}
      >
        <Caret open={!collapsed} />
        <SessionIcon />
        <SessionName>{session}</SessionName>
        <TypeTag>Session</TypeTag>
      </div>
      <Collapsible open={!collapsed}>
        <div
          style={{
            paddingLeft: 20,
            display: "flex",
            flexDirection: "column",
          }}
        >
          {buses.map(({ bus, queries }) => {
            const bKey = busKey(session, bus);
            const busCollapsed = !!collapsedBuses[bKey];
            const popoverOpen = popoverBus === bKey;
            // CSS-hide the bus row when a filter is active and no interest under it
            // has matches; QueryRows stay mounted so they keep reporting counts.
            const busHidden = filtering && busVisibleCount(session, bus, queries) === 0;
            return (
              <BusBox
                key={bus}
                client={client}
                sessionName={session}
                busName={bus}
                queries={queries}
                collapsed={busCollapsed}
                onToggleCollapsed={() => toggleBusCollapsed(bKey)}
                hidden={busHidden}
                popoverOpen={popoverOpen}
                onTogglePopover={() => setPopoverBus(popoverOpen ? null : bKey)}
                onAddQuery={(q) => {
                  addBusQuery(bKey, session, bus, q);
                  setPopoverBus(null);
                }}
                onRemoveQuery={(name) => removeBusQuery(bKey, name)}
                filterText={filterText}
                focusedQueryKey={focusedQueryKey}
                clearFocus={clearFocus}
                reportFilteredCount={reportFilteredCount}
                clearFilteredCount={clearFilteredCount}
              />
            );
          })}
        </div>
      </Collapsible>
    </div>
  );
}
