// === InterestsPane.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useMemo, useRef, useState } from "react";

import type { Client } from "@sen/client";
import { useTopology } from "@sen/client/react";

import { pinnedObjectsActions, type PinnedObject } from "../../state/ui_prefs.js";
import { DangerPillButton, HoverIconButton } from "../../ui/buttons.js";

import { CollapseAllIcon, ExpandAllIcon, QueryIcon } from "../../ui/icons.js";
import { BoxedInterestsView } from "./boxed_view.js";
import { busKey, pinKeyBusPrefix, queryKey } from "../../core/keys.js";
import { Hint } from "./icons.js";
import { type NamedQuery } from "./bus_queries.js";
import { busQueriesActions, useBusQueries } from "./bus_queries_store.js";

// Pane is the query-management UI; wire-level declarations live in App-root InterestOwner.

export interface InterestsPaneProps {
  client: Client | null;
  // Held here so the topology-driven GC can prune stale pins when sessions/buses vanish.
  pinnedObjects: ReadonlyMap<string, PinnedObject>;
}

export function InterestsPane({ client, pinnedObjects }: InterestsPaneProps) {
  const { sessions, loading } = useTopology(client);
  const [collapsedSessions, setCollapsedSessions] = useState<Record<string, boolean>>({});
  const busQueries = useBusQueries();
  const [collapsedBuses, setCollapsedBuses] = useState<Record<string, boolean>>({});
  // One popover open across the panel at a time.
  const [popoverBus, setPopoverBus] = useState<string | null>(null);
  // Set on interest creation so the view can scroll-into-view and flash; row clears it.
  const [focusedQueryKey, setFocusedQueryKey] = useState<string | null>(null);
  // Ref + forceRender (vs setState spread) keeps writes O(1) under bursty filter churn.
  const filteredCountsRef = useRef<Record<string, number>>({});
  const [, setFilteredCountsTick] = useState(0);
  // Coalesce bursty unmount-driven re-renders into one per microtask.
  const pendingTickRef = useRef(false);
  const scheduleTick = useCallback(() => {
    if (pendingTickRef.current) return;
    pendingTickRef.current = true;
    queueMicrotask(() => {
      pendingTickRef.current = false;
      setFilteredCountsTick((n) => n + 1);
    });
  }, []);
  const reportFilteredCount = useCallback(
    (key: string, count: number) => {
      if (filteredCountsRef.current[key] === count) return;
      filteredCountsRef.current[key] = count;
      scheduleTick();
    },
    [scheduleTick],
  );
  // Prune pinnedObjects + filteredCountsRef to the bus keys current topology presents:
  // QueryRows for vanished sessions never unmount cleanly, so we GC by topology instead.
  useEffect(() => {
    if (loading || sessions.length === 0) return;
    const valid = new Set<string>();
    for (const s of sessions) {
      for (const b of s.buses) valid.add(busKey(s.name, b));
    }
    pinnedObjectsActions.pruneByBusKeys(valid);
    let dropped = 0;
    for (const key of Object.keys(filteredCountsRef.current)) {
      const prefix = pinKeyBusPrefix(key);
      if (prefix !== null && !valid.has(prefix)) {
        delete filteredCountsRef.current[key];
        dropped++;
      }
    }
    if (dropped > 0) scheduleTick();
  }, [sessions, loading, scheduleTick]);
  const clearFilteredCount = useCallback(
    (key: string) => {
      if (!(key in filteredCountsRef.current)) return;
      delete filteredCountsRef.current[key];
      scheduleTick();
    },
    [scheduleTick],
  );
  const filteredCounts = filteredCountsRef.current;

  const toggleSession = useCallback((name: string) => {
    setCollapsedSessions((prev) => ({ ...prev, [name]: !prev[name] }));
  }, []);
  const toggleBusCollapsed = useCallback((bKey: string) => {
    setCollapsedBuses((prev) => ({ ...prev, [bKey]: !prev[bKey] }));
  }, []);
  const addBusQuery = useCallback(
    (bKey: string, sessionName: string, busName: string, q: NamedQuery) => {
      busQueriesActions.add(bKey, q);
      const key = queryKey(sessionName, busName, q.name);
      // Uncollapse parents so the focus flash lands on a visible row.
      setCollapsedSessions((prev) => ({ ...prev, [sessionName]: false }));
      setCollapsedBuses((prev) => ({ ...prev, [bKey]: false }));
      setFocusedQueryKey(key);
    },
    [],
  );
  const removeBusQuery = useCallback((bKey: string, name: string) => {
    busQueriesActions.remove(bKey, name);
  }, []);

  const groupedInterests = useMemo(() => {
    const groups: Array<{
      session: string;
      buses: Array<{ bus: string; queries: NamedQuery[] }>;
    }> = [];
    const orderedSessions = [...sessions].sort((a, b) => a.name.localeCompare(b.name));
    for (const session of orderedSessions) {
      const busGroups: Array<{ bus: string; queries: NamedQuery[] }> = [];
      const orderedBuses = [...session.buses].sort((a, b) => a.localeCompare(b));
      for (const bus of orderedBuses) {
        const qs = busQueries[busKey(session.name, bus)] ?? [];
        // Keep empty buses; the AddInterestButton in the header is the entry point.
        const queries = [...qs].sort((a, b) => a.name.localeCompare(b.name));
        busGroups.push({ bus, queries });
      }
      if (busGroups.length === 0) continue;
      groups.push({ session: session.name, buses: busGroups });
    }
    return groups;
  }, [busQueries, sessions]);

  const totalInterestCount = useMemo(
    () =>
      groupedInterests.reduce(
        (sum, g) => sum + g.buses.reduce((s, b) => s + b.queries.length, 0),
        0,
      ),
    [groupedInterests],
  );

  const clearFocus = useCallback(() => setFocusedQueryKey(null), []);

  const collapseAll = useCallback(() => {
    setCollapsedSessions(() => {
      const next: Record<string, boolean> = {};
      for (const g of groupedInterests) next[g.session] = true;
      return next;
    });
    setCollapsedBuses(() => {
      const next: Record<string, boolean> = {};
      for (const g of groupedInterests) {
        for (const b of g.buses) next[busKey(g.session, b.bus)] = true;
      }
      return next;
    });
  }, [groupedInterests]);
  const expandAll = useCallback(() => {
    setCollapsedSessions({});
    setCollapsedBuses({});
  }, []);

  if (!client) {
    return <Hint>connecting...</Hint>;
  }

  const sessionList = [...sessions].sort((a, b) => a.name.localeCompare(b.name));

  if (loading && sessionList.length === 0) {
    return <Hint>loading topology...</Hint>;
  }
  if (sessionList.length === 0) {
    return <Hint>no sessions on this network</Hint>;
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", fontSize: "var(--fs-lg)" }}>
      <div
        className="carved-bottom"
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          // Matches Object Explorer header height so the layout doesn't shift on swap.
          minHeight: 55,
          padding: "0 10px",
          flex: "none",
        }}
      >
        <span
          style={{
            display: "inline-flex",
            alignItems: "center",
            gap: 6,
            fontSize: "var(--fs-lg)",
            color: "var(--fg-base)",
            fontFamily: "var(--font-ui)",
            fontWeight: 500,
          }}
        >
          <span style={{ color: "var(--fg-muted)", display: "inline-flex" }}>
            <QueryIcon color="currentColor" />
          </span>
          Queries
          {totalInterestCount > 0 && (
            <span
              style={{
                fontSize: "var(--fs-sm)",
                color: "var(--fg-subtle)",
                fontFamily: "var(--font-mono)",
              }}
            >
              {totalInterestCount}
            </span>
          )}
        </span>
        <span style={{ marginLeft: "auto", display: "inline-flex", gap: 6, alignItems: "center" }}>
          <ClearAllQueriesButton disabled={totalInterestCount === 0} />
          <HoverIconButton
            onClick={collapseAll}
            ariaLabel="collapse all sessions and buses"
            tooltip="Collapse all"
            icon={<CollapseAllIcon />}
            size={20}
          />
          <HoverIconButton
            onClick={expandAll}
            ariaLabel="expand all sessions and buses"
            tooltip="Expand all"
            icon={<ExpandAllIcon />}
            size={20}
          />
        </span>
      </div>
      <div style={{ flex: 1, overflow: "auto", paddingBottom: 16 }}>
        <BoxedInterestsView
          client={client}
          groupedInterests={groupedInterests}
          collapsedSessions={collapsedSessions}
          toggleSession={toggleSession}
          collapsedBuses={collapsedBuses}
          toggleBusCollapsed={toggleBusCollapsed}
          removeBusQuery={removeBusQuery}
          filterText=""
          focusedQueryKey={focusedQueryKey}
          clearFocus={clearFocus}
          popoverBus={popoverBus}
          setPopoverBus={setPopoverBus}
          addBusQuery={addBusQuery}
          filteredCounts={filteredCounts}
          reportFilteredCount={reportFilteredCount}
          clearFilteredCount={clearFilteredCount}
        />
      </div>
    </div>
  );
}

function ClearAllQueriesButton({ disabled }: { disabled: boolean }) {
  return (
    <DangerPillButton
      onClick={busQueriesActions.clearAll}
      disabled={disabled}
      title="Remove every query on every bus"
    />
  );
}
