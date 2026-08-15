// === BusBox.tsx ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client } from "@sen/client";

import { Collapsible } from "../../ui/layout.js";
import { BusIcon } from "../../ui/icons.js";
import { AddInterestPopover } from "./add_interest_popover.js";
import { AddInterestButton, BusName, Caret, TypeTag } from "./icons.js";
import { queryKey } from "../../core/keys.js";
import { type NamedQuery } from "./bus_queries.js";
import { QueryRow } from "./query_row.js";

export function BusBox({
  client,
  sessionName,
  busName,
  queries,
  collapsed,
  onToggleCollapsed,
  hidden,
  popoverOpen,
  onTogglePopover,
  onAddQuery,
  onRemoveQuery,
  filterText,
  focusedQueryKey,
  clearFocus,
  reportFilteredCount,
  clearFilteredCount,
}: {
  client: Client;
  sessionName: string;
  busName: string;
  queries: NamedQuery[];
  collapsed: boolean;
  onToggleCollapsed: () => void;
  hidden: boolean;
  popoverOpen: boolean;
  onTogglePopover: () => void;
  onAddQuery: (q: NamedQuery) => void;
  onRemoveQuery: (name: string) => void;
  filterText: string;
  focusedQueryKey: string | null;
  clearFocus: () => void;
  reportFilteredCount: (key: string, count: number) => void;
  clearFilteredCount: (key: string) => void;
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
          padding: "3px 4px 3px 0",
          cursor: "pointer",
          userSelect: "none",
        }}
      >
        <Caret open={!collapsed} />
        <BusIcon />
        <BusName>{busName}</BusName>
        <TypeTag>Bus</TypeTag>
        <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-subtle)", fontFamily: "var(--font-mono)" }}>
          ({queries.length})
        </span>
        <span style={{ flex: 1 }} />
        <AddInterestButton active={popoverOpen} onClick={onTogglePopover} />
      </div>
      {popoverOpen && (
        <AddInterestPopover
          client={client}
          sessionName={sessionName}
          busName={busName}
          existingNames={queries.map((q) => q.name)}
          onSave={(name, query) => {
            onAddQuery({ name, query });
          }}
          onCancel={onTogglePopover}
        />
      )}
      {queries.length > 0 && (
        <Collapsible open={!collapsed}>
          <div
            style={{
              paddingLeft: 20,
              display: "flex",
              flexDirection: "column",
            }}
          >
            {queries.map((q, idx) => {
              const key = queryKey(sessionName, busName, q.name);
              return (
                <QueryRow
                  key={key}
                  sessionName={sessionName}
                  busName={busName}
                  queryName={q.name}
                  queryString={q.query}
                  onRemove={() => onRemoveQuery(q.name)}
                  filterText={filterText}
                  reportFilteredCount={reportFilteredCount}
                  clearFilteredCount={clearFilteredCount}
                  focused={focusedQueryKey === key}
                  onFocusConsumed={clearFocus}
                  rowIndex={idx}
                />
              );
            })}
          </div>
        </Collapsible>
      )}
    </div>
  );
}
