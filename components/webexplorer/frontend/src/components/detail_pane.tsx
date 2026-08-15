// === DetailPane.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useMemo, useState, type ReactNode } from "react";

import type { Client } from "@sen/client";
import { useObject } from "@sen/client/react";

import { isComposite } from "../core/types.js";
import {
  type ClassMemberGroup,
  totalEvents,
  totalMethods,
  totalProperties,
  useClassMemberGroups,
} from "../state/class_members.js";
import { useInterestByName, useInterestContaining } from "../state/interest_registry.js";
import type { Selection } from "../state/selection.js";
import { EmptyHint, EmptyPane } from "../ui/empty_state.js";
import { FilterInput } from "../ui/filter_input.js";
import { HoverIconButton } from "../ui/buttons.js";
import {
  CollapseAllIcon,
  EventsIcon,
  ExpandAllIcon,
  MethodsIcon,
  PropertiesIcon,
} from "../ui/icons.js";
import { EventsTable } from "./events_table.js";
import { MethodTable } from "./method_table.js";
import { PropertyTable, WatchAllToggle } from "./property_table.js";

export interface DetailPaneProps {
  client: Client | null;
  selection: Selection | null;
}

export function DetailPane({ client, selection }: DetailPaneProps) {
  if (!selection) {
    return (
      <EmptyPane
        heading="Pick an object to inspect."
        help="Open an interest in the left pane, then click any object to see its properties, methods, and events here."
      />
    );
  }
  if (!client) {
    return <EmptyHint>not connected.</EmptyHint>;
  }
  return <DetailFor client={client} selection={selection} />;
}

type Tab = "props" | "events" | "methods";

const FILTER_PLACEHOLDER = "Filter";

function DetailFor({ client, selection }: { client: Client; selection: Selection }) {
  const preferredInterest = useInterestByName(selection.interestName);
  const preferredObj = useObject(preferredInterest, selection.objectName);
  // If the source interest dropped this object but another open interest has it, resolve
  // through that one. Route via useObject so incremental match arrivals re-render.
  const fallback = useInterestContaining(preferredObj ? null : selection.objectName);
  const fallbackObj = useObject(fallback?.interest ?? null, selection.objectName);
  const obj = preferredObj ?? fallbackObj;
  const groups = useClassMemberGroups(client, selection.className);
  const [tab, setTab] = useState<Tab>("props");
  const [filter, setFilter] = useState("");
  // Class-section vs composite-body sets are independent so an expand-all of one survives
  // a user-toggled collapse of the other.
  const [collapsedClasses, setCollapsedClasses] = useState<ReadonlySet<string>>(new Set());
  const [collapsedProperties, setCollapsedProperties] = useState<ReadonlySet<string>>(new Set());
  const toggleClassCollapsed = useCallback((className: string) => {
    setCollapsedClasses((prev) => {
      const next = new Set(prev);
      if (next.has(className)) next.delete(className);
      else next.add(className);
      return next;
    });
  }, []);
  const togglePropertyCollapsed = useCallback((propertyName: string) => {
    setCollapsedProperties((prev) => {
      const next = new Set(prev);
      if (next.has(propertyName)) next.delete(propertyName);
      else next.add(propertyName);
      return next;
    });
  }, []);
  const compositePropertyNames = useMemo(() => {
    if (!groups) return [];
    const names: string[] = [];
    for (const g of groups) {
      for (const p of g.properties) {
        if (isComposite(client, p.type)) names.push(p.name);
      }
    }
    return names;
  }, [groups, client]);
  const collapseAll = useCallback(() => {
    if (!groups) return;
    setCollapsedClasses(new Set(groups.map((g) => g.className)));
    setCollapsedProperties(new Set(compositePropertyNames));
  }, [groups, compositePropertyNames]);
  const expandAll = useCallback(() => {
    setCollapsedClasses(new Set());
    setCollapsedProperties(new Set());
  }, []);

  const counts: TabCounts | null = groups
    ? {
        props: totalProperties(groups),
        events: totalEvents(groups),
        methods: totalMethods(groups),
      }
    : null;
  const propertyCount = counts?.props ?? 0;
  const hasGroups = !!groups;
  let tabActions: ReactNode = null;
  if (tab === "props" && hasGroups) {
    tabActions = (
      <>
        <HoverIconButton
          onClick={collapseAll}
          ariaLabel="collapse all class sections and composite property bodies"
          tooltip="Collapse all"
          icon={<CollapseAllIcon />}
          size={20}
        />
        <HoverIconButton
          onClick={expandAll}
          ariaLabel="expand all class sections and composite property bodies"
          tooltip="Expand all"
          icon={<ExpandAllIcon />}
          size={20}
        />
        {propertyCount > 0 && <WatchAllToggle groups={groups!} selection={selection} />}
      </>
    );
  }

  return (
    <div
      style={{
        display: "flex",
        flexDirection: "column",
        height: "100%",
        minHeight: 0,
        fontSize: "var(--fs-lg)",
      }}
    >
      <div
        style={{
          flex: "none",
          padding: "12px 12px 6px 12px",
        }}
      >
        <DetailHeader
          tab={tab}
          onTabChange={setTab}
          counts={counts}
          filter={filter}
          onFilterChange={setFilter}
          tabActions={tabActions}
        />
      </div>
      <div
        style={{
          flex: 1,
          minHeight: 0,
          overflow: "auto",
          // Right padding is 0 so the Properties action column sits flush against the edge.
          padding: "12px 0 12px 12px",
          // Always reserve the scrollbar gutter; otherwise the right edge jumps ~17px when
          // a scrollbar appears, inconsistently insetting the action column.
          scrollbarGutter: "stable",
        }}
      >
        {!obj ? (
          <div style={{ color: "var(--fg-muted)", fontSize: "var(--fs-md)" }}>
            <code>{selection.objectName}</code> isn't in the match set of any currently-open
            query. Open a query that contains it from the Queries pane, or wait for it
            to come back into <code>{selection.interestName}</code>.
          </div>
        ) : !groups ? (
          <div style={{ color: "var(--fg-muted)", fontSize: "var(--fs-md)" }}>
            Class spec for <code>{selection.className}</code> not yet cached. (Should arrive on the
            next interestUpdate.)
          </div>
        ) : (
          <>
            {tab === "props" && (
              <PropertyTable
                client={client}
                obj={obj}
                groups={groups}
                selection={selection}
                filter={filter}
                collapsedClasses={collapsedClasses}
                collapsedProperties={collapsedProperties}
                onToggleClassCollapsed={toggleClassCollapsed}
                onTogglePropertyCollapsed={togglePropertyCollapsed}
              />
            )}
            {tab === "events" && (
              <EventsTable
                client={client}
                groups={groups}
                selection={selection}
                filter={filter}
              />
            )}
            {tab === "methods" && (
              <MethodTable client={client} obj={obj} groups={groups} filter={filter} />
            )}
          </>
        )}
      </div>
    </div>
  );
}

interface TabCounts {
  props: number;
  events: number;
  methods: number;
}

// Always renders so the user can switch tabs even before obj/class spec resolve.
function DetailHeader({
  tab,
  onTabChange,
  counts,
  filter,
  onFilterChange,
  tabActions,
}: {
  tab: Tab;
  onTabChange: (t: Tab) => void;
  counts: TabCounts | null;
  filter: string;
  onFilterChange: (v: string) => void;
  tabActions: ReactNode;
}) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 6, paddingBottom: 8 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
        <Tabs current={tab} onChange={onTabChange} counts={counts} />
        <span style={{ flex: 1 }} />
        {tabActions && (
          <span
            style={{ display: "inline-flex", alignItems: "center", gap: 4, flex: "none" }}
          >
            {tabActions}
          </span>
        )}
      </div>
      <FilterInput
        value={filter}
        onChange={onFilterChange}
        placeholder={FILTER_PLACEHOLDER}
        ariaLabel="filter rows"
        fill
      />
    </div>
  );
}

function Tabs({
  current,
  onChange,
  counts,
}: {
  current: Tab;
  onChange: (t: Tab) => void;
  counts: TabCounts | null;
}) {
  const tabs: { id: Tab; label: string; icon: ReactNode }[] = [
    { id: "props", label: "Properties", icon: <PropertiesIcon /> },
    { id: "events", label: "Events", icon: <EventsIcon /> },
    { id: "methods", label: "Methods", icon: <MethodsIcon /> },
  ];
  return (
    <div
      style={{
        display: "inline-flex",
        gap: 4,
        flex: "none",
        borderBottom: "1px solid var(--border-default)",
      }}
    >
      {tabs.map((t) => {
        const active = t.id === current;
        const count = counts ? counts[t.id] : null;
        return (
          <button
            key={t.id}
            type="button"
            onClick={() => onChange(t.id)}
            style={{
              display: "inline-flex",
              alignItems: "center",
              gap: 6,
              padding: "5px 10px 6px",
              fontSize: "var(--fs-lg)",
              border: "none",
              borderRadius: 0,
              cursor: "pointer",
              background: "transparent",
              color: active ? "var(--fg-base)" : "var(--fg-muted)",
              fontWeight: active ? 500 : 400,
              fontFamily: "var(--font-ui)",
              boxShadow: active ? "inset 0 -2px 0 0 var(--accent)" : "none",
              transition: "color 120ms ease, box-shadow 120ms ease",
            }}
          >
            <span style={{ display: "inline-flex", color: "currentColor" }}>{t.icon}</span>
            {t.label}
            {count !== null && (
              <span style={{ marginLeft: 2, color: "var(--fg-subtle)", fontSize: "var(--fs-sm)" }}>
                {count}
              </span>
            )}
          </button>
        );
      })}
    </div>
  );
}
