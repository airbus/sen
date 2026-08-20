// === events_table.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useRef, useState } from "react";

import type { Client, EventSpec } from "@sen/client";

import { formatTimestamp } from "../core/format.js";
import { makeEventWatchKey } from "../core/watch_keys.js";
import { type ClassMemberGroup } from "../state/class_members.js";
import { eventWatchActions, useIsEventWatched } from "../state/event_watches.js";
import { useObjectEvents, type EventDelivery } from "../state/event_store.js";
import { eventsDrawerTabsActions, useIsEventVisibleInTable } from "../state/events_drawer_tabs.js";
import type { Selection } from "../state/selection.js";
import { bottomPaneActions, bottomTabActions } from "../state/ui_prefs.js";
import { Collapsible } from "../ui/layout.js";
import { EmptyHint } from "../ui/empty_state.js";
import { HelpButton, IconToggle } from "../ui/buttons.js";

import { EventsIcon, TableIcon } from "../ui/icons.js";
import { Tooltip } from "../ui/tooltip.js";
import { WatchToggleButton } from "../widgets/value_render/index.js";
import { FlashEnabledContext, useFlash } from "../widgets/value_render/value_row.js";
import { GroupHeader } from "./group_header.js";

export function EventsTable({
  client,
  groups,
  selection,
  filter,
}: {
  client: Client;
  groups: ClassMemberGroup[];
  selection: Selection;
  filter: string;
}) {
  const filterLower = filter.trim().toLowerCase();
  const surfaceGroups = useMemo(
    () =>
      [...groups]
        .reverse()
        .map((g) => ({
          ...g,
          events: g.events.filter((e) => {
            if (!filterLower) return true;
            if (e.name.toLowerCase().includes(filterLower)) return true;
            for (const a of e.args) {
              if (a.name.toLowerCase().includes(filterLower)) return true;
              if (a.type.toLowerCase().includes(filterLower)) return true;
            }
            return false;
          }),
        }))
        .filter((g) => g.events.length > 0),
    [groups, filterLower],
  );
  const totalDeclared = useMemo(
    () => groups.reduce((sum, g) => sum + g.events.length, 0),
    [groups],
  );

  // Wrapper identity flips per invalidate; `.events` is the stable mutable backing array.
  const logView = useObjectEvents(selection.interestName, selection.objectName);
  const log = logView.events;
  const lastByEvent = useMemo(() => {
    const m = new Map<string, { seq: number; timestamp: string }>();
    for (let i = log.length - 1; i >= 0; i--) {
      const d = log[i]!;
      if (!m.has(d.eventName)) m.set(d.eventName, { seq: d.seq, timestamp: d.timestamp });
    }
    return m;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [logView]);

  const [collapsedClasses, setCollapsedClasses] = useState<ReadonlySet<string>>(new Set());
  const toggleClassCollapsed = (className: string) =>
    setCollapsedClasses((prev) => {
      const next = new Set(prev);
      if (next.has(className)) next.delete(className);
      else next.add(className);
      return next;
    });

  if (totalDeclared === 0) {
    return (
      <div>
        <EmptyHint>no events declared</EmptyHint>
      </div>
    );
  }
  if (surfaceGroups.length === 0) {
    return (
      <div>
        <EmptyHint>no events match the filter.</EmptyHint>
      </div>
    );
  }
  const showGroupHeaders = surfaceGroups.length > 1;
  return (
    <div
      style={{
        height: "100%",
        display: "flex",
        flexDirection: "column",
        minHeight: 0,
        gap: 8,
      }}
    >
      <FlashEnabledContext.Provider value={true}>
        <div
          style={{
            // Size-to-content; shrink + scroll when over, don't grow into empty space.
            flex: "0 1 auto",
            minHeight: 0,
            overflow: "auto",
          }}
        >
          {surfaceGroups.map((g) => {
            const classCollapsed = collapsedClasses.has(g.className);
            return (
              <div
                key={g.className}
                style={
                  showGroupHeaders
                    ? {
                        marginLeft: 4,
                        borderLeft: "2px solid rgba(180, 180, 200, 0.4)",
                        paddingLeft: 8,
                        marginBottom: 10,
                      }
                    : undefined
                }
              >
                {showGroupHeaders && (
                  <GroupHeader
                    className={g.className}
                    collapsed={classCollapsed}
                    onToggle={() => toggleClassCollapsed(g.className)}
                  />
                )}
                <Collapsible open={!classCollapsed}>
                  {g.events.map((e) => {
                    const last = lastByEvent.get(e.name);
                    return (
                      <EventRow
                        key={e.name}
                        client={client}
                        spec={e}
                        className={g.className}
                        selection={selection}
                        lastSeq={last?.seq}
                        lastTimestamp={last?.timestamp}
                      />
                    );
                  })}
                </Collapsible>
              </div>
            );
          })}
        </div>
      </FlashEnabledContext.Provider>
    </div>
  );
}

function EventRow({
  client,
  spec,
  className,
  selection,
  lastSeq,
  lastTimestamp,
}: {
  client: Client;
  spec: EventSpec;
  className: string;
  selection: Selection;
  /** Identity change drives the row flash. */
  lastSeq: number | undefined;
  lastTimestamp: string | undefined;
}) {
  const watchKey = makeEventWatchKey({
    interestName: selection.interestName,
    objectName: selection.objectName,
    eventName: spec.name,
  });
  const watched = useIsEventWatched(watchKey);
  // useFlash writes data-flash on rowRef; `.property-row[data-flash]` paints the tint.
  const rowRef = useRef<HTMLDivElement | null>(null);
  useFlash(lastSeq, undefined, rowRef);
  const rowClass = "property-row";
  const onToggleWatch = () => {
    eventWatchActions.toggleWatch({
      interestName: selection.interestName,
      objectName: selection.objectName,
      eventName: spec.name,
      className,
      sessionName: selection.sessionName,
      busName: selection.busName,
    });
  };
  const tableParts = {
    interestName: selection.interestName,
    className,
    eventName: spec.name,
  };
  const inTable = useIsEventVisibleInTable(tableParts, selection.objectName);
  const onToggleTable = () => {
    if (inTable) {
      // No-op when filter is `null` (= all instances); the picker is the right place to narrow.
      eventsDrawerTabsActions.removeSourceFromTab(tableParts, selection.objectName);
      return;
    }
    eventsDrawerTabsActions.openType(tableParts, {
      ensureSourceVisible: selection.objectName,
    });
    bottomPaneActions.setFolded(false);
    bottomTabActions.set("events");
  };
  return (
    <div
      ref={rowRef}
      className={`${rowClass} carved-bottom`}
      style={{
        display: "flex",
        alignItems: "center",
        gap: 6,
        padding: "3px 8px",
        minWidth: 0,
      }}
    >
      <span
        style={{ display: "inline-flex", color: "var(--fg-subtle)", flex: "none" }}
        title="event"
      >
        <EventsIcon />
      </span>
      <Tooltip content={spec.description}>
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-base)",
            minWidth: 0,
            flex: "0 1 auto",
            overflow: "hidden",
            textOverflow: "ellipsis",
            whiteSpace: "nowrap",
          }}
        >
          {spec.name}
        </span>
      </Tooltip>
      {lastTimestamp && (
        <span
          style={{
            marginLeft: "auto",
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
            flex: "none",
            whiteSpace: "nowrap",
          }}
          title={`Last fired ${lastTimestamp} UTC (server)`}
        >
          {formatTimestamp(lastTimestamp)}
        </span>
      )}
      <span
        style={{
          display: "inline-flex",
          alignItems: "center",
          gap: 4,
          flex: "none",
          marginLeft: lastTimestamp ? 0 : "auto",
        }}
      >
        <HelpButton
          description={spec.description}
          ariaLabel={`${spec.name} documentation`}
        />
        <WatchToggleButton
          watched={watched}
          onClick={onToggleWatch}
          tooltip={watched ? "Remove from Watches" : "Add to Watches"}
        />
        <IconToggle
          pressed={inTable}
          onClick={onToggleTable}
          tooltip={inTable ? "Hide from Events table" : "Show in Events table"}
          icon={<TableIcon />}
        />
      </span>
    </div>
  );
}
