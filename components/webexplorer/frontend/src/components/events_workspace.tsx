// === EventsWorkspace.tsx =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Fragment, memo, useContext, useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";
import { createPortal } from "react-dom";
import { Virtuoso, type VirtuosoHandle } from "react-virtuoso";

import type { Client, InterestHandle, ObjectHandle } from "@sen/client";
import { useObjects } from "@sen/client/react";

import { useClassEvents } from "../state/class_members.js";
import { useAllEvents, type EventDelivery } from "../state/event_store.js";
import {
  STREAM_TAB,
  eventsDrawerTabsActions,
  useActiveEventsTab,
  useOpenEventTypeTabs,
  useOpenEventTypeTabsByKey,
  type EventTypeTab,
} from "../state/events_drawer_tabs.js";
import { useAllInterests } from "../state/interest_registry.js";
import { parseInterestName } from "../widgets/interests/bus_queries.js";
import { DangerPillButton, LiveToggleButton } from "../ui/buttons.js";
import { EmptyHint, EmptyPane, Mono } from "../ui/empty_state.js";
import { FilterInput } from "../ui/filter_input.js";
import {
  ChevronDownIcon,
  ChevronRightIcon,
  ClassBracesIcon,
  CloseIcon,
  EventsIcon,
  ListIcon,
} from "../ui/icons.js";
import { BottomPaneHeaderSlot } from "./bottom_pane.js";
import { EventRow } from "./event_row.js";
import { ObjectEventCollector } from "./object_event_collector.js";
import { PerTypeEventsTable } from "./per_type_events_table.js";

// ObjectEventCollectors mount here so deliveries flow regardless of which view is active.
export interface EventsWorkspaceProps {
  client: Client | null;
}

export function EventsWorkspace({ client }: EventsWorkspaceProps) {
  const interests = useAllInterests();
  return (
    <div style={{ display: "flex", flexDirection: "row", height: "100%", minHeight: 0 }}>
      <ViewSidebar />
      <div style={{ flex: 1, minWidth: 0, display: "flex", flexDirection: "column" }}>
        <EventsBody client={client} />
      </div>
      {client &&
        interests.map(([name, handle]) => (
          <InterestCollectors key={name} client={client} interestName={name} handle={handle} />
        ))}
    </div>
  );
}

function EventsBody({ client }: { client: Client | null }) {
  const active = useActiveEventsTab();
  const tabByKey = useOpenEventTypeTabsByKey();
  // Stream view when STREAM_TAB OR when a stale key no longer maps to an open tab.
  const tab = active === STREAM_TAB ? null : tabByKey.get(active) ?? null;
  if (tab === null) return <StreamView client={client} />;
  return <PerTypeEventsTable client={client} tab={tab} />;
}

const SIDEBAR_WIDTH_DEFAULT = 180;
const SIDEBAR_WIDTH_MIN = 120;
const SIDEBAR_WIDTH_MAX = 400;
const SIDEBAR_WIDTH_KEY = "webex.events.sidebarWidth";

function readSidebarWidth(): number {
  try {
    const raw = localStorage.getItem(SIDEBAR_WIDTH_KEY);
    const n = raw ? parseInt(raw, 10) : NaN;
    if (Number.isFinite(n) && n >= SIDEBAR_WIDTH_MIN && n <= SIDEBAR_WIDTH_MAX) return n;
  } catch {
  }
  return SIDEBAR_WIDTH_DEFAULT;
}

function writeSidebarWidth(w: number): void {
  try {
    localStorage.setItem(SIDEBAR_WIDTH_KEY, String(w));
  } catch {
  }
}

// One tail-first walk per tick feeds every sidebar entry's activity glow.
const ACTIVITY_THRESHOLD_MS = 2000;
const ACTIVITY_TICK_MS = 200;
const EMPTY_ACTIVITY = {
  byEvent: new Set<string>() as ReadonlySet<string>,
  byClassName: new Set<string>() as ReadonlySet<string>,
  anyActivity: false,
};
function useRecentActivity(): {
  byEvent: ReadonlySet<string>;
  byClassName: ReadonlySet<string>;
  anyActivity: boolean;
} {
  // Decoupled from wire rate: 5 Hz tail-walk regardless of delivery throughput.
  const log = useAllEvents();
  const logRef = useRef(log);
  logRef.current = log;
  const [tick, setTick] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), ACTIVITY_TICK_MS);
    return () => clearInterval(id);
  }, []);
  return useMemo(() => {
    const events = logRef.current.events;
    // Steady-state idle short-circuit avoids two Set allocations per tick.
    if (events.length === 0 || events[events.length - 1]!.at < Date.now() - ACTIVITY_THRESHOLD_MS) {
      return EMPTY_ACTIVITY;
    }
    const byEvent = new Set<string>();
    const byClassName = new Set<string>();
    const cutoff = Date.now() - ACTIVITY_THRESHOLD_MS;
    for (let i = events.length - 1; i >= 0; i--) {
      const d = events[i]!;
      if (d.at < cutoff) break;
      byEvent.add(`${d.interestName}\0${d.className}\0${d.eventName}`);
      byClassName.add(d.className);
    }
    return { byEvent, byClassName, anyActivity: true };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tick]);
}

function ViewSidebar() {
  const openTypes = useOpenEventTypeTabs();
  const active = useActiveEventsTab();
  const activity = useRecentActivity();
  // Preserve first-seen class order so adding a second event to an existing class doesn't
  // move the class to the bottom.
  const groups = useMemo<readonly [string, readonly EventTypeTab[]][]>(() => {
    const ordered: string[] = [];
    const byClass = new Map<string, EventTypeTab[]>();
    for (const t of openTypes) {
      let bucket = byClass.get(t.className);
      if (!bucket) {
        bucket = [];
        byClass.set(t.className, bucket);
        ordered.push(t.className);
      }
      bucket.push(t);
    }
    return ordered.map((c) => [c, byClass.get(c)!] as const);
  }, [openTypes]);

  // Auto-expand the active tab's class so the selection is always visible.
  const [collapsed, setCollapsed] = useState<ReadonlySet<string>>(() => new Set());
  const tabByKey = useOpenEventTypeTabsByKey();
  const activeClass = active !== STREAM_TAB ? tabByKey.get(active)?.className : undefined;
  useEffect(() => {
    if (!activeClass) return;
    setCollapsed((prev) => {
      if (!prev.has(activeClass)) return prev;
      const next = new Set(prev);
      next.delete(activeClass);
      return next;
    });
  }, [activeClass]);

  const toggleClass = (cls: string) => {
    setCollapsed((prev) => {
      const next = new Set(prev);
      if (next.has(cls)) next.delete(cls);
      else next.add(cls);
      return next;
    });
  };

  const [width, setWidthState] = useState<number>(() => readSidebarWidth());
  const setWidth = (next: number) => {
    const clamped = Math.max(SIDEBAR_WIDTH_MIN, Math.min(SIDEBAR_WIDTH_MAX, Math.round(next)));
    setWidthState((prev) => (prev === clamped ? prev : clamped));
  };
  const commitWidth = (next: number) => {
    const clamped = Math.max(SIDEBAR_WIDTH_MIN, Math.min(SIDEBAR_WIDTH_MAX, Math.round(next)));
    writeSidebarWidth(clamped);
  };

  return (
    <div
      style={{
        width,
        flex: "none",
        display: "flex",
        flexDirection: "column",
        borderRight: "1px solid var(--border-default)",
        background: "var(--surface-pane-nav)",
        overflowY: "auto",
        position: "relative",
      }}
    >
      <SidebarResizeHandle currentWidth={width} onWidth={setWidth} onCommit={commitWidth} />
      <div
        style={{
          padding: "6px 12px 4px 12px",
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-xs)",
          textTransform: "uppercase",
          letterSpacing: "0.08em",
          color: "var(--fg-subtle)",
        }}
      >
        Views
      </div>
      <SidebarItem
        label="All events"
        leadingIcon={<ListIcon />}
        active={active === STREAM_TAB}
        onClick={() => eventsDrawerTabsActions.setActive(STREAM_TAB)}
        pulsing={activity.anyActivity}
      />
      {groups.length > 0 && <SidebarDivider />}
      {groups.map(([className, tabs]) => {
        const isCollapsed = collapsed.has(className);
        return (
          <Fragment key={className}>
            <SidebarClassHeader
              className={className}
              collapsed={isCollapsed}
              onToggle={() => toggleClass(className)}
            />
            {!isCollapsed &&
              tabs.map((t, i) => (
                <SidebarItem
                  key={t.key}
                  label={t.eventName}
                  leadingIcon={<EventsIcon />}
                  treeBranch={i === tabs.length - 1 ? "last" : "mid"}
                  active={active === t.key}
                  onClick={() => eventsDrawerTabsActions.setActive(t.key)}
                  onClose={() => eventsDrawerTabsActions.closeType(t.key)}
                  pulsing={activity.byEvent.has(
                    `${t.interestName}\0${t.className}\0${t.eventName}`,
                  )}
                />
              ))}
          </Fragment>
        );
      })}
    </div>
  );
}

function SidebarResizeHandle({
  currentWidth,
  onWidth,
  onCommit,
}: {
  currentWidth: number;
  onWidth: (next: number) => void;
  onCommit: (next: number) => void;
}) {
  const [hover, setHover] = useState(false);
  const [active, setActive] = useState(false);
  const startXRef = useRef(0);
  const startWidthRef = useRef(0);
  const latestWidthRef = useRef(currentWidth);

  const handleDown = (e: React.PointerEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();
    startXRef.current = e.clientX;
    startWidthRef.current = currentWidth;
    latestWidthRef.current = currentWidth;
    setActive(true);
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const handleMove = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    const dx = e.clientX - startXRef.current;
    const next = startWidthRef.current + dx;
    latestWidthRef.current = next;
    onWidth(next);
  };
  const handleUp = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    setActive(false);
    onCommit(latestWidthRef.current);
    try {
      e.currentTarget.releasePointerCapture(e.pointerId);
    } catch {
    }
  };
  const handleKey = (e: React.KeyboardEvent<HTMLDivElement>) => {
    const step = e.shiftKey ? 40 : 10;
    let dx = 0;
    if (e.key === "ArrowLeft") dx = -step;
    else if (e.key === "ArrowRight") dx = step;
    else if (e.key === "Home") dx = -10000;
    else if (e.key === "End") dx = 10000;
    else return;
    e.preventDefault();
    const next = currentWidth + dx;
    onWidth(next);
    onCommit(next);
  };

  return (
    <div
      role="separator"
      aria-orientation="vertical"
      aria-label="Resize sidebar"
      tabIndex={0}
      onKeyDown={handleKey}
      onPointerDown={handleDown}
      onPointerMove={handleMove}
      onPointerUp={handleUp}
      onPointerCancel={handleUp}
      onPointerEnter={() => setHover(true)}
      onPointerLeave={() => setHover(false)}
      style={{
        position: "absolute",
        right: -4,
        top: 0,
        bottom: 0,
        width: 8,
        cursor: "col-resize",
        userSelect: "none",
        touchAction: "none",
        zIndex: 2,
      }}
    >
      <div
        style={{
          position: "absolute",
          left: "50%",
          top: 0,
          bottom: 0,
          width: hover || active ? 2 : 0,
          marginLeft: hover || active ? -1 : 0,
          background: "var(--accent)",
          opacity: active ? 0.9 : hover ? 0.7 : 0,
          transition: active ? "none" : "opacity 120ms",
        }}
      />
    </div>
  );
}

function SidebarClassHeader({
  className,
  collapsed,
  onToggle,
}: {
  className: string;
  collapsed: boolean;
  onToggle: () => void;
}) {
  const [hover, setHover] = useState(false);
  return (
    <button
      type="button"
      onClick={onToggle}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={className}
      aria-expanded={!collapsed}
      style={{
        display: "flex",
        alignItems: "center",
        gap: 5,
        padding: "4px 8px 4px 6px",
        border: "none",
        borderLeft: "2px solid transparent",
        background: hover ? "var(--accent-wash)" : "rgba(255, 255, 255, 0.025)",
        color: "var(--fg-base)",
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-md)",
        fontWeight: 600,
        cursor: "pointer",
        textAlign: "left",
        overflow: "hidden",
        textOverflow: "ellipsis",
        whiteSpace: "nowrap",
        flex: "none",
        minHeight: 26,
      }}
    >
      <span
        style={{
          display: "inline-flex",
          alignItems: "center",
          justifyContent: "center",
          width: 12,
          height: 12,
          color: "var(--fg-muted)",
          flex: "none",
        }}
      >
        {collapsed ? <ChevronRightIcon /> : <ChevronDownIcon />}
      </span>
      {/* No flash on the class header; leaf rows glow individually. */}
      <span
        aria-hidden="true"
        style={{ display: "inline-flex", color: "var(--fg-subtle)", flex: "none" }}
      >
        <ClassBracesIcon />
      </span>
      <span style={{ overflow: "hidden", textOverflow: "ellipsis", flex: 1 }}>{className}</span>
    </button>
  );
}

function SidebarDivider() {
  return (
    <div
      style={{
        height: 1,
        margin: "4px 8px",
        background: "var(--border-subtle)",
        flex: "none",
      }}
    />
  );
}

function SidebarItem({
  label,
  active,
  onClick,
  onClose,
  treeBranch,
  leadingIcon,
  pulsing = false,
}: {
  label: string;
  active: boolean;
  onClick: () => void;
  onClose?: () => void;
  /** Child of a tree group; CSS pseudo-elements draw the connector. */
  treeBranch?: "mid" | "last";
  leadingIcon?: React.ReactNode;
  pulsing?: boolean;
}) {
  const indent = treeBranch !== undefined;
  const [hover, setHover] = useState(false);
  const bg = active ? "var(--bg-elevated)" : hover ? "var(--accent-wash)" : "transparent";
  const fg = active ? "var(--fg-base)" : hover ? "var(--accent-text-wash)" : "var(--fg-muted)";
  const cls = treeBranch
    ? `events-sidebar-tree-item${treeBranch === "last" ? " events-sidebar-tree-item--last" : ""}`
    : undefined;
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      aria-pressed={active}
      title={label}
      className={cls}
      style={{
        display: "flex",
        alignItems: "center",
        gap: 4,
        padding: indent ? "4px 8px 4px 28px" : "5px 8px 5px 10px",

        border: "none",
        borderLeft: active ? "2px solid var(--accent)" : "2px solid transparent",
        background: bg,
        color: fg,
        fontFamily: "var(--font-ui)",
        fontSize: indent ? "var(--fs-sm)" : "var(--fs-md)",
        cursor: "pointer",
        textAlign: "left",
        overflowWrap: "anywhere",
        flex: "none",
        minHeight: indent ? 24 : 28,
        transition: "background 120ms ease, color 120ms ease",
      }}
    >
      <span
        className={`value-text${pulsing ? " value-text--flash" : ""}`}
        style={{ display: "inline-flex", alignItems: "center", gap: 4, flex: 1, minWidth: 0 }}
      >
        {leadingIcon && (
          <span
            aria-hidden="true"
            style={{ display: "inline-flex", color: "var(--fg-subtle)", flex: "none" }}
          >
            {leadingIcon}
          </span>
        )}
        <span style={{ flex: 1, minWidth: 0, overflow: "hidden", textOverflow: "ellipsis" }}>
          {label}
        </span>
      </span>
      {onClose && (
        <span
          role="button"
          tabIndex={-1}
          aria-label={`Close ${label}`}
          onClick={(e) => {
            e.stopPropagation();
            onClose();
          }}
          style={{
            display: "inline-flex",
            alignItems: "center",
            justifyContent: "center",
            width: 16,
            height: 16,
            color: hover || active ? "var(--fg-muted)" : "transparent",
            borderRadius: "var(--radius-xs)",
            flex: "none",
            transition: "color 120ms ease",
          }}
        >
          <CloseIcon />
        </span>
      )}
    </button>
  );
}

function StreamView({ client }: { client: Client | null }) {
  // Cell wrapper; the inner `.events` array is stable, so memo deps point at the wrapper.
  const liveLog = useAllEvents();
  // 100ms debounce so search-as-you-type doesn't re-filter a 5000-entry log per keystroke.
  const [filterInput, setFilterInput] = useState("");
  const [filter, setFilter] = useState("");
  useEffect(() => {
    if (filterInput === filter) return;
    const id = setTimeout(() => setFilter(filterInput), 100);
    return () => clearTimeout(id);
  }, [filterInput, filter]);

  const [live, setLive] = useState(true);
  // View-only clear; global store untouched.
  const [clearedBeforeSeq, setClearedBeforeSeq] = useState(0);

  // Snapshot when paused so appends don't shift content under the user.
  const pausedSnapshotRef = useRef<readonly EventDelivery[] | null>(null);
  if (!live && pausedSnapshotRef.current === null) {
    pausedSnapshotRef.current = liveLog.events.slice();
  }
  if (live && pausedSnapshotRef.current !== null) pausedSnapshotRef.current = null;

  // Tail-only filter cache; survives front-trims via seq tracking.
  const filterCacheRef = useRef<{
    filter: string;
    events: readonly EventDelivery[];
    clearedBeforeSeq: number;
    lastSeenSeq: number;
    result: EventDelivery[];
  } | null>(null);

  const visible = useMemo<readonly EventDelivery[]>(() => {
    const events =
      !live && pausedSnapshotRef.current !== null ? pausedSnapshotRef.current : liveLog.events;
    if (!filter.trim()) {
      filterCacheRef.current = null;
      if (clearedBeforeSeq === 0) return events;
      // seq is monotonic; binary search for first entry past clearedBeforeSeq.
      let lo = 0;
      let hi = events.length;
      while (lo < hi) {
        const mid = (lo + hi) >>> 1;
        if (events[mid]!.seq <= clearedBeforeSeq) lo = mid + 1;
        else hi = mid;
      }
      return lo === 0 ? events : events.slice(lo);
    }
    const q = filter.toLowerCase();
    const cache = filterCacheRef.current;
    let result: EventDelivery[];
    let startIdx = 0;

    if (
      cache !== null &&
      cache.filter === filter &&
      cache.events === events &&
      cache.clearedBeforeSeq === clearedBeforeSeq
    ) {
      // Drop cached entries trimmed off the front of the live array.
      if (events.length === 0) {
        result = [];
      } else {
        const frontSeq = events[0]!.seq;
        let dropTo = 0;
        while (dropTo < cache.result.length && cache.result[dropTo]!.seq < frontSeq) dropTo++;
        result = dropTo > 0 ? cache.result.slice(dropTo) : cache.result.slice();
      }
      // Scan from end so a 1-event tail finds its index in one compare.
      for (let i = events.length - 1; i >= 0; i--) {
        if (events[i]!.seq <= cache.lastSeenSeq) {
          startIdx = i + 1;
          break;
        }
        startIdx = i;
      }
    } else {
      result = [];
    }
    for (let i = startIdx; i < events.length; i++) {
      const d = events[i]!;
      if (d.seq <= clearedBeforeSeq) continue;
      if (d.lowerSearch.includes(q)) result.push(d);
    }
    const lastSeenSeq =
      events.length > 0 ? events[events.length - 1]!.seq : cache?.lastSeenSeq ?? 0;
    filterCacheRef.current = { filter, events, clearedBeforeSeq, lastSeenSeq, result };
    return result;
  }, [liveLog, live, filter, clearedBeforeSeq]);

  const virtuosoRef = useRef<VirtuosoHandle>(null);
  // Suppresses the spurious atBottomStateChange(false) between append and tail-scroll.
  const scrollGuardUntilRef = useRef(0);
  // Imperative tail-follow; Virtuoso's followOutput races buffer trims at high rates.
  useEffect(() => {
    if (!live || visible.length === 0) return;
    scrollGuardUntilRef.current = Date.now() + 250;
    virtuosoRef.current?.scrollToIndex({
      index: visible.length - 1,
      align: "end",
      behavior: "auto",
    });
  }, [live, visible.length]);

  const handleClear = (): void => {
    const lastSeq = liveLog.events.length > 0 ? liveLog.events[liveLog.events.length - 1]!.seq : 0;
    setClearedBeforeSeq(lastSeq);
  };
  const handleToggleLive = (): void => {
    setLive((prev) => !prev);
  };

  const headerSlot = useContext(BottomPaneHeaderSlot);
  const toolbar = (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 8,
        fontSize: "var(--fs-md)",
        color: "var(--fg-muted)",
      }}
    >
      <FilterInput
        value={filterInput}
        onChange={setFilterInput}
        placeholder="Filter events..."
        ariaLabel="filter events"
      />
      <DangerPillButton
        onClick={handleClear}
        title="Clear the visible event log (other surfaces keep their copy)"
      />
      <span
        style={{
          fontSize: "var(--fs-sm)",
          color: "var(--fg-subtle)",
          fontFamily: "var(--font-mono)",
          minWidth: 48,
          textAlign: "right",
          whiteSpace: "nowrap",
          flex: "none",
        }}
      >
        {visible.length}
        {visible.length !== liveLog.events.length ? ` / ${liveLog.events.length}` : ""}
      </span>
      <LiveToggleButton on={live} onToggle={handleToggleLive} />
    </span>
  );

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", fontSize: "var(--fs-lg)" }}>
      {headerSlot && createPortal(toolbar, headerSlot)}
      {visible.length > 0 && (
        <div
          style={{
            display: "grid",
            gridTemplateColumns: "94px 1fr",
            gap: 10,
            padding: "5px 12px",
            borderBottom: "1px solid var(--border-default)",
            background: "var(--surface-list-header)",
            fontSize: "var(--fs-xs)",
            textTransform: "uppercase",
            letterSpacing: "0.08em",
            color: "var(--fg-subtle)",
            fontFamily: "var(--font-ui)",
            flex: "none",
          }}
        >
          <span>time</span>
          <span>event</span>
        </div>
      )}

      <div style={{ flex: 1, minHeight: 0 }}>
        {visible.length === 0 ? (
          liveLog.events.length === 0 ? (
            <EmptyPane
              icon={<EventsIcon />}
              heading="Listening for events."
              help={[
                "Every object in the currently-open interests is subscribed. As soon as one fires an event, it lands here in real time.",
                <>Click <Mono>+Table</Mono> on any event row in the Object Explorer to focus a per-type table next to this stream.</>,
              ]}
            />
          ) : (
            <EmptyHint>no matches for filter.</EmptyHint>
          )
        ) : (
          <Virtuoso
            ref={virtuosoRef}
            style={{ height: "100%" }}
            data={visible}
            computeItemKey={(_, d) => d.seq}
            itemContent={(_, d) => <EventRow delivery={d} client={client} showObject={true} />}
            atBottomThreshold={40}
            increaseViewportBy={200}
            atBottomStateChange={(atBottom) => {
              if (!atBottom && live && Date.now() >= scrollGuardUntilRef.current) {
                setLive(false);
              }
            }}
          />
        )}
      </div>
    </div>
  );
}

// memo'd so filter keystrokes don't re-render every collector subtree.
const InterestCollectors = memo(InterestCollectorsImpl);
function InterestCollectorsImpl({
  client,
  interestName,
  handle,
}: {
  client: Client;
  interestName: string;
  handle: InterestHandle;
}) {
  const objects = useObjects(handle);
  const { sessionName, busName } = useMemo(() => {
    const parsed = parseInterestName(interestName);
    return parsed
      ? { sessionName: parsed.sessionName, busName: parsed.busName }
      : { sessionName: "", busName: "" };
  }, [interestName]);
  return (
    <>
      {objects.map((obj) => (
        <ObjectCollector
          key={obj.name}
          client={client}
          interestName={interestName}
          sessionName={sessionName}
          busName={busName}
          obj={obj}
        />
      ))}
    </>
  );
}

const ObjectCollector = memo(ObjectCollectorImpl);
function ObjectCollectorImpl({
  client,
  interestName,
  sessionName,
  busName,
  obj,
}: {
  client: Client;
  interestName: string;
  sessionName: string;
  busName: string;
  obj: ObjectHandle;
}) {
  const events = useClassEvents(client, obj.className);
  const eventNames = useMemo(() => events.map((e) => e.name), [events]);
  return (
    <ObjectEventCollector
      interestName={interestName}
      sessionName={sessionName}
      busName={busName}
      objectName={obj.name}
      className={obj.className}
      obj={obj}
      eventNames={eventNames.length > 0 ? eventNames : null}
    />
  );
}

export type { EventTypeTab };
