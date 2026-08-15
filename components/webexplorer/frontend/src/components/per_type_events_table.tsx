// === per_type_events_table.tsx =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useCallback, useContext, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";
import { createPortal } from "react-dom";
import { Virtuoso, type VirtuosoHandle } from "react-virtuoso";

import type { ArgSpec, Client, EventSpec } from "@sen/client";
import { useObjects } from "@sen/client/react";

import { formatTimestamp } from "../core/format.js";
import { isInClassChain, useClassEvents, useTypeCacheTick } from "../state/class_members.js";
import { useAllEvents, type EventDelivery } from "../state/event_store.js";
import { eventsDrawerTabsActions, type EventTypeTab } from "../state/events_drawer_tabs.js";
import { useInterestByName } from "../state/interest_registry.js";
import { getLastResumeAt } from "../state/visibility.js";
import { EmptyHint, EmptyPane } from "../ui/empty_state.js";
import { ObjectLink } from "../ui/explorer_links.js";
import { EventsIcon, FilterIcon } from "../ui/icons.js";
import { LiveToggleButton } from "../ui/buttons.js";
import { InlineValue } from "../widgets/value_render/inline_value.js";
import { BottomPaneHeaderSlot } from "./bottom_pane.js";

const TS_COL = "timestamp" as const;
const SRC_COL = "source" as const;
const EMPTY_STRINGS: readonly string[] = Object.freeze([]);
const DEFAULT_TS_WIDTH = 94;
const DEFAULT_OBJ_WIDTH = 140;
const MIN_COL_WIDTH = 40;

// Per-typeKey key so concurrent tabs can't race on a shared object's read-modify-write.
const COL_WIDTHS_KEY_PREFIX = "webex.events.colWidths.";
const LEGACY_COL_WIDTHS_KEY = "webex.events.colWidths";

function readLegacyColWidths(typeKey: string): Record<string, number> | null {
  try {
    const raw = localStorage.getItem(LEGACY_COL_WIDTHS_KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw) as Record<string, Record<string, number>>;
    const forTab = parsed[typeKey];
    if (!forTab || typeof forTab !== "object") return null;
    return forTab;
  } catch {
    return null;
  }
}

function readColWidths(typeKey: string): Record<string, number> {
  try {
    let parsed: unknown;
    const raw = localStorage.getItem(COL_WIDTHS_KEY_PREFIX + typeKey);
    if (raw) {
      parsed = JSON.parse(raw);
    } else {
      // Fall back to the legacy shared-object key so saved widths survive the schema change.
      parsed = readLegacyColWidths(typeKey);
      if (!parsed) return {};
    }
    if (!parsed || typeof parsed !== "object") return {};
    const out: Record<string, number> = {};
    for (const [k, v] of Object.entries(parsed as Record<string, unknown>)) {
      if (typeof v === "number" && Number.isFinite(v) && v >= MIN_COL_WIDTH) out[k] = v;
    }
    return out;
  } catch {
    return {};
  }
}

function writeColWidths(typeKey: string, widths: Record<string, number>): void {
  try {
    localStorage.setItem(COL_WIDTHS_KEY_PREFIX + typeKey, JSON.stringify(widths));
  } catch {
  }
}

const bodyCellBase: React.CSSProperties = {
  display: "flex",
  alignItems: "center",
  padding: "5px 10px",
  fontFamily: "var(--font-mono)",
  color: "var(--fg-base)",
  overflow: "hidden",
  textOverflow: "ellipsis",
  whiteSpace: "nowrap",
  borderRight: "1px solid var(--border-default)",
  minWidth: 0,
};

const pillButton: React.CSSProperties = {
  padding: "3px 10px",
  fontSize: "var(--fs-md)",
  fontFamily: "var(--font-ui)",
  background: "transparent",
  color: "var(--fg-muted)",
  border: "1px solid var(--border-default)",
  borderRadius: "var(--radius-sm)",
  cursor: "pointer",
};


export function PerTypeEventsTable({
  client,
  tab,
}: {
  client: Client | null;
  tab: EventTypeTab;
}) {
  // useClassEvents reads the client's type cache and crashes on null.
  if (!client) {
    return (
      <EmptyPane
        heading="Not connected."
        help="Reconnect to see deliveries of this event type."
      />
    );
  }
  // key={tab.key} forces full remount on tab swap so per-tab refs don't leak.
  return <ConnectedTable key={tab.key} client={client} tab={tab} />;
}

function ConnectedTable({ client, tab }: { client: Client; tab: EventTypeTab }) {
  const allEvents = useClassEvents(client, tab.className);
  const spec: EventSpec | undefined = useMemo(
    () => allEvents.find((e) => e.name === tab.eventName),
    [allEvents, tab.eventName],
  );

  const liveLog = useAllEvents();

  const [live, setLive] = useState(true);
  const [clearedBeforeSeq, setClearedBeforeSeq] = useState(0);

  // Snapshot at pause edge so later appends don't shift rows under the user.
  const pausedSnapshotRef = useRef<readonly EventDelivery[] | null>(null);
  if (!live && pausedSnapshotRef.current === null) {
    pausedSnapshotRef.current = liveLog.events.slice();
  }
  if (live && pausedSnapshotRef.current !== null) pausedSnapshotRef.current = null;

  // All sources ever seen emit this triple; the picker reads this for scope alternatives.
  const sourceTrackingRef = useRef<Set<string>>(new Set());
  // seenSources keys on this so its Array.from+sort only runs when the set actually grew.
  const [seenVersion, setSeenVersion] = useState(0);

  // tab.className may be a parent where the event was declared; subclasses actually emit.
  // Per-Client cache so N open tables share one walk; cacheTick invalidates on onTypeAdded.
  const cacheTick = useTypeCacheTick(client);
  const isInClass = useMemo(() => {
    const base = tab.className;
    return (className: string): boolean => isInClassChain(client, base, className);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [client, tab.className, cacheTick]);

  // Tri-state: null = no filter; empty Set = pass none; non-empty = explicit selection.
  const filterSet = useMemo<ReadonlySet<string> | null>(
    () => (tab.sourceFilters === null ? null : new Set(tab.sourceFilters)),
    [tab.sourceFilters],
  );

  // Depend on liveLog wrapper (flips per batch), NOT .events (stable inner reference).
  const visible = useMemo<readonly EventDelivery[]>(() => {
    const src = !live && pausedSnapshotRef.current !== null ? pausedSnapshotRef.current : liveLog.events;
    const out: EventDelivery[] = [];
    const seen = sourceTrackingRef.current;
    for (const d of src) {
      if (
        d.interestName !== tab.interestName ||
        d.eventName !== tab.eventName ||
        !isInClass(d.className)
      ) {
        continue;
      }
      // Tracking is filter-unaware so the picker can offer addable siblings.
      if (!seen.has(d.objectName)) seen.add(d.objectName);
      if (filterSet && !filterSet.has(d.objectName)) continue;
      if (d.seq <= clearedBeforeSeq) continue;
      out.push(d);
    }
    return out;
  }, [liveLog, live, clearedBeforeSeq, tab.interestName, tab.eventName, isInClass, filterSet]);

  // Bump seenVersion only when the set actually grew; steady-state cost is one Set.size read.
  const seenSizeAtLastBumpRef = useRef(0);
  useEffect(() => {
    const size = sourceTrackingRef.current.size;
    if (size !== seenSizeAtLastBumpRef.current) {
      seenSizeAtLastBumpRef.current = size;
      setSeenVersion((v) => v + 1);
    }
  });

  const seenSources = useMemo(
    () => Array.from(sourceTrackingRef.current).sort(),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [seenVersion],
  );

  const totalMatching = useMemo(() => {
    const src = !live && pausedSnapshotRef.current !== null ? pausedSnapshotRef.current : liveLog.events;
    let n = 0;
    for (const d of src) {
      if (
        d.interestName === tab.interestName &&
        d.eventName === tab.eventName &&
        isInClass(d.className)
      ) {
        if (filterSet && !filterSet.has(d.objectName)) continue;
        n++;
      }
    }
    return n;
  }, [liveLog, live, tab.interestName, tab.eventName, isInClass, filterSet]);

  // scrollToIndex fires atBottomStateChange(false) mid-flight; guard suppresses it.
  const virtuosoRef = useRef<VirtuosoHandle>(null);
  const scrollGuardUntilRef = useRef(0);
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
  const handleToggleLive = (): void => setLive((prev) => !prev);

  const [widths, setWidths] = useState<Record<string, number>>(() => readColWidths(tab.key));
  useEffect(() => {
    setWidths(readColWidths(tab.key));
  }, [tab.key]);

  const updateWidth = useCallback(
    (col: string, next: number) => {
      setWidths((prev) => {
        const clamped = Math.max(MIN_COL_WIDTH, Math.round(next));
        if (prev[col] === clamped) return prev;
        const merged = { ...prev, [col]: clamped };
        writeColWidths(tab.key, merged);
        return merged;
      });
    },
    [tab.key],
  );

  // Source column always rendered: its header carries the filter icon.
  const argCount = spec?.args.length ?? 0;
  const gridTemplate = useMemo(() => {
    const parts: string[] = [];
    parts.push(`${widths[TS_COL] ?? DEFAULT_TS_WIDTH}px`);
    parts.push(`${widths[SRC_COL] ?? DEFAULT_OBJ_WIDTH}px`);
    if (spec) {
      for (const arg of spec.args) {
        const w = widths[arg.name];
        parts.push(w !== undefined ? `${w}px` : "minmax(120px, 1fr)");
      }
    }
    return parts.join(" ");
  }, [widths, spec, argCount]);

  const candidates = useSourceCandidates(
    tab.interestName,
    isInClass,
    seenSources,
    tab.sourceFilters ?? EMPTY_STRINGS,
  );

  const onFiltersChange = useCallback(
    (next: readonly string[] | null) => {
      eventsDrawerTabsActions.setSourceFilters(tab.key, next);
    },
    [tab.key],
  );

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
      <button
        type="button"
        onClick={handleClear}
        title="Clear the visible rows (other surfaces keep their copy)"
        style={pillButton}
      >
        Clear
      </button>
      <span
        style={{
          fontSize: "var(--fs-sm)",
          color: "var(--fg-subtle)",
          fontFamily: "var(--font-mono)",
          minWidth: 48,
          textAlign: "right",
        }}
      >
        {visible.length}
        {visible.length !== totalMatching ? ` / ${totalMatching}` : ""}
      </span>
      <LiveToggleButton on={live} onToggle={handleToggleLive} />
    </span>
  );

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      {headerSlot && createPortal(toolbar, headerSlot)}
      {/* Relative wrapper so ResizeOverlay can absolute-cover header+body with full-height handles. */}
      <div style={{ position: "relative", flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
        <HeaderRow
          gridTemplate={gridTemplate}
          spec={spec}
          candidates={candidates}
          selectedFilters={tab.sourceFilters}
          onFiltersChange={onFiltersChange}
        />
        <div style={{ flex: 1, minHeight: 0 }}>
          {visible.length === 0 ? (
            totalMatching === 0 ? (
              <EmptyPane
                icon={<EventsIcon />}
                heading={
                  tab.sourceFilters === null
                    ? `No ${tab.className}.${tab.eventName} deliveries yet.`
                    : tab.sourceFilters.length === 0
                      ? "Source filter is empty."
                      : `No ${tab.className}.${tab.eventName} deliveries from the selected instances yet.`
                }
                help={
                  tab.sourceFilters === null
                    ? "Waiting for instances of this class in the bound interest to fire this event. Each delivery will land here as a new row."
                    : tab.sourceFilters.length === 0
                      ? "Every source is currently excluded. Open the filter icon in the source column and click 'Select all' to widen the scope."
                      : "Waiting for the selected instances to fire this event. Click the filter icon in the source column to change the selection."
                }
              />
            ) : (
              <EmptyHint>no rows past the last clear.</EmptyHint>
            )
          ) : (
            <Virtuoso
              ref={virtuosoRef}
              style={{ height: "100%" }}
              data={visible}
              computeItemKey={(_, d) => d.seq}
              itemContent={(_, d) => (
                <Row delivery={d} client={client} spec={spec} gridTemplate={gridTemplate} />
              )}
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
        <ResizeOverlay gridTemplate={gridTemplate} spec={spec} widths={widths} onResize={updateWidth} />
      </div>
    </div>
  );
}

function HeaderRow({
  gridTemplate,
  spec,
  candidates,
  selectedFilters,
  onFiltersChange,
}: {
  gridTemplate: string;
  spec: EventSpec | undefined;
  candidates: readonly string[];
  selectedFilters: readonly string[] | null;
  onFiltersChange: (next: readonly string[] | null) => void;
}) {
  return (
    <div
      style={{
        display: "grid",
        gridTemplateColumns: gridTemplate,
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
      <HeaderCell label="timestamp" />
      <HeaderCell
        label="source"
        trailing={
          <SourceFilterButton
            candidates={candidates}
            selected={selectedFilters}
            onChange={onFiltersChange}
          />
        }
      />
      {spec?.args.map((arg: ArgSpec) => (
        <HeaderCell key={arg.name} label={arg.name} title={arg.type} />
      ))}
    </div>
  );
}

function HeaderCell({
  label,
  title,
  trailing,
}: {
  label: string;
  title?: string;
  trailing?: React.ReactNode;
}) {
  return (
    <span
      title={title ?? label}
      style={{
        ...cellPadding,
        display: "flex",
        alignItems: "center",
        gap: 4,
        overflow: "hidden",
        whiteSpace: "nowrap",
        borderRight: "1px solid var(--border-default)",
      }}
    >
      <span style={{ overflow: "hidden", textOverflow: "ellipsis" }}>{label}</span>
      {trailing && <span style={{ display: "inline-flex", marginLeft: "auto" }}>{trailing}</span>}
    </span>
  );
}

// Outer is pointer-events:none so wheel/clicks pass through; each handle flips it back on
// for its 10px drag strip.
function ResizeOverlay({
  gridTemplate,
  spec,
  widths,
  onResize,
}: {
  gridTemplate: string;
  spec: EventSpec | undefined;
  widths: Record<string, number>;
  onResize: (col: string, next: number) => void;
}) {
  const trackRefs = useRef<Record<string, HTMLDivElement | null>>({});
  const cols: { col: string; startWidth: number | undefined }[] = [
    { col: TS_COL, startWidth: widths[TS_COL] ?? DEFAULT_TS_WIDTH },
    { col: SRC_COL, startWidth: widths[SRC_COL] ?? DEFAULT_OBJ_WIDTH },
  ];
  if (spec) {
    for (const arg of spec.args) {
      cols.push({ col: arg.name, startWidth: widths[arg.name] });
    }
  }

  return (
    <div
      aria-hidden="true"
      style={{
        position: "absolute",
        inset: 0,
        display: "grid",
        gridTemplateColumns: gridTemplate,
        pointerEvents: "none",
        zIndex: 1,
      }}
    >
      {cols.map(({ col, startWidth }) => (
        <div
          key={col}
          ref={(node) => {
            trackRefs.current[col] = node;
          }}
          style={{ position: "relative" }}
        >
          <ColumnResizer
            onDragStart={() =>
              startWidth ?? trackRefs.current[col]?.getBoundingClientRect().width ?? 0
            }
            onDrag={(w) => onResize(col, w)}
          />
        </div>
      ))}
    </div>
  );
}

function ColumnResizer({
  onDragStart,
  onDrag,
}: {
  /** Width-in-px at drag start; reads DOM rect for 1fr columns. */
  onDragStart: () => number;
  onDrag: (nextWidthPx: number) => void;
}) {
  const [hover, setHover] = useState(false);
  const [active, setActive] = useState(false);
  const startXRef = useRef(0);
  const startWidthRef = useRef(0);

  const handleDown = (e: React.PointerEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();
    startXRef.current = e.clientX;
    startWidthRef.current = onDragStart();
    setActive(true);
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const handleMove = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    const dx = e.clientX - startXRef.current;
    onDrag(startWidthRef.current + dx);
  };
  const handleUp = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    setActive(false);
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
    onDrag(onDragStart() + dx);
  };

  return (
    <div
      role="separator"
      aria-orientation="vertical"
      aria-label="resize column"
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
        right: -5,
        top: 0,
        bottom: 0,
        width: 10,
        cursor: "col-resize",
        userSelect: "none",
        touchAction: "none",
        pointerEvents: "auto",
      }}
    >
      {(hover || active) && (
        <div
          style={{
            position: "absolute",
            left: "50%",
            top: 0,
            bottom: 0,
            width: 2,
            marginLeft: -1,
            background: "var(--accent)",
            opacity: active ? 0.9 : 0.75,
            transition: active ? "none" : "opacity 120ms",
          }}
        />
      )}
    </div>
  );
}

const cellPadding: React.CSSProperties = {
  padding: "5px 10px",
};

const Row = memo(RowImpl, (prev, next) =>
  prev.delivery === next.delivery &&
  prev.client === next.client &&
  prev.spec === next.spec &&
  prev.gridTemplate === next.gridTemplate,
);

function RowImpl({
  delivery,
  client,
  spec,
  gridTemplate,
}: {
  delivery: EventDelivery;
  client: Client | null;
  spec: EventSpec | undefined;
  gridTemplate: string;
}) {
  // Rows mounted within 1.5s of arrival run the arrival animation once; backlog renders static.
  const [isFresh] = useState(() => {
    const now = Date.now();
    return now - delivery.at < 1500 && delivery.at > getLastResumeAt();
  });
  return (
    <div
      className={isFresh ? "events-row--arrived" : undefined}
      style={{
        display: "grid",
        gridTemplateColumns: gridTemplate,
        borderBottom: "1px solid var(--border-default)",
        fontSize: "var(--fs-md)",
        alignItems: "stretch",
      }}
    >
      <span
        style={{ ...bodyCellBase, color: "var(--fg-subtle)", fontSize: "var(--fs-sm)" }}
        title={delivery.timestamp + " UTC (server)"}
      >
        {formatTimestamp(delivery.timestamp)}
      </span>
      <span style={bodyCellBase} title={delivery.objectName}>
        <ObjectLink
          selection={{
            interestName: delivery.interestName,
            objectName: delivery.objectName,
            className: delivery.className,
            sessionName: delivery.sessionName,
            busName: delivery.busName,
          }}
        />
      </span>
      {spec?.args.map((arg: ArgSpec, i: number) => (
        <span key={arg.name} style={bodyCellBase}>
          <InlineValue client={client} declaredType={arg.type} value={delivery.args[i]} />
        </span>
      ))}
    </div>
  );
}

// Candidates = (current interest instances passing isInClass) ∪ seenSources ∪ currentFilters
// so departed instances stay selectable while their buffered deliveries linger.
function useSourceCandidates(
  interestName: string,
  isInClass: (className: string) => boolean,
  seenSources: readonly string[],
  currentFilters: readonly string[],
): readonly string[] {
  const handle = useInterestByName(interestName);
  const objects = useObjects(handle);
  return useMemo(() => {
    const set = new Set<string>();
    for (const o of objects) if (isInClass(o.className)) set.add(o.name);
    for (const s of seenSources) set.add(s);
    for (const s of currentFilters) set.add(s);
    return Array.from(set).sort();
  }, [objects, isInClass, seenSources, currentFilters]);
}

function SourceFilterButton({
  candidates,
  selected,
  onChange,
}: {
  candidates: readonly string[];
  selected: readonly string[] | null;
  onChange: (next: readonly string[] | null) => void;
}) {
  const [open, setOpen] = useState(false);
  const [hover, setHover] = useState(false);
  const anchorRef = useRef<HTMLSpanElement | null>(null);
  // null = no filter; everything else (including []) is "filtered".
  const active = selected !== null;

  useEffect(() => {
    if (!open) return;
    const onDocClick = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (target && anchorRef.current?.contains(target)) return;
      const popoverEl = document.getElementById("webex-source-picker");
      if (target && popoverEl?.contains(target)) return;
      setOpen(false);
    };
    const onEsc = (e: KeyboardEvent) => {
      if (e.key === "Escape") setOpen(false);
    };
    document.addEventListener("mousedown", onDocClick);
    document.addEventListener("keydown", onEsc);
    return () => {
      document.removeEventListener("mousedown", onDocClick);
      document.removeEventListener("keydown", onEsc);
    };
  }, [open]);

  return (
    <span ref={anchorRef} style={{ display: "inline-flex" }}>
      <button
        type="button"
        onClick={() => setOpen((v) => !v)}
        onMouseEnter={() => setHover(true)}
        onMouseLeave={() => setHover(false)}
        aria-label="Filter sources"
        aria-pressed={active}
        title={
          active
            ? selected.length === 0
              ? "Filter is empty - no sources visible"
              : `Showing ${selected.length} of ${candidates.length} sources`
            : "Filter by source"
        }
        style={{
          display: "inline-flex",
          alignItems: "center",
          justifyContent: "center",
          width: 18,
          height: 18,
          padding: 0,
          background: "transparent",
          border: "none",
          color: active
            ? "var(--accent)"
            : hover
              ? "var(--fg-base)"
              : "var(--fg-subtle)",
          cursor: "pointer",
        }}
      >
        <FilterIcon active={active} />
      </button>
      {open && (
        <SourcePickerPopover
          anchorRef={anchorRef}
          candidates={candidates}
          selected={selected}
          onChange={onChange}
          onClose={() => setOpen(false)}
        />
      )}
    </span>
  );
}

function SourcePickerPopover({
  anchorRef,
  candidates,
  selected,
  onChange,
  onClose,
}: {
  anchorRef: React.RefObject<HTMLSpanElement | null>;
  candidates: readonly string[];
  /** null = Select all; [] = Clear all; else explicit selection. */
  selected: readonly string[] | null;
  onChange: (next: readonly string[] | null) => void;
  onClose: () => void;
}) {
  const [query, setQuery] = useState("");
  // Portal + position:fixed escapes the header's overflow:hidden; capture-phase scroll
  // listener catches nested scrollable ancestors.
  const [pos, setPos] = useState<{ top: number; right: number } | null>(null);
  useLayoutEffect(() => {
    const update = () => {
      const rect = anchorRef.current?.getBoundingClientRect();
      if (!rect) return;
      setPos({ top: rect.bottom + 4, right: window.innerWidth - rect.right });
    };
    update();
    window.addEventListener("resize", update);
    window.addEventListener("scroll", update, true);
    return () => {
      window.removeEventListener("resize", update);
      window.removeEventListener("scroll", update, true);
    };
  }, [anchorRef]);
  const filteredCandidates = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) return candidates;
    return candidates.filter((c) => c.toLowerCase().includes(q));
  }, [candidates, query]);

  const allActive = selected === null;
  const selectedSet = useMemo(() => new Set(selected ?? []), [selected]);

  const toggle = (name: string) => {
    if (allActive) {
      onChange(candidates.filter((c) => c !== name));
      return;
    }
    const arr = selected ?? [];
    const next = selectedSet.has(name)
      ? arr.filter((s) => s !== name)
      : [...arr, name];
    // Collapse "all selected" back to null so the filter icon reads as inactive.
    onChange(next.length === candidates.length ? null : next);
  };

  const selectAll = () => onChange(null);
  const clearAll = () => onChange([]);

  if (!pos) return null;
  return createPortal(
    <div
      id="webex-source-picker"
      role="dialog"
      aria-label="Source filter"
      style={{
        position: "fixed",
        top: pos.top,
        right: pos.right,
        width: 240,
        maxHeight: 320,
        display: "flex",
        flexDirection: "column",
        background: "var(--bg-elevated)",
        border: "1px solid var(--border-strong)",
        borderRadius: "var(--radius-lg)",
        boxShadow: "0 12px 28px rgba(0, 0, 0, 0.45)",
        zIndex: 60,
        // Reset so header uppercase/letter-spacing tokens can't leak in.
        textTransform: "none",
        letterSpacing: "normal",
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-md)",
        color: "var(--fg-base)",
      }}
    >
      <div
        style={{
          padding: "8px 10px",
          borderBottom: "1px solid var(--border-default)",
          display: "flex",
          alignItems: "center",
          gap: 6,
        }}
      >
        <input
          type="text"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder={`Filter ${candidates.length} sources`}
          spellCheck={false}
          autoComplete="off"
          autoFocus
          style={{
            flex: 1,
            minWidth: 0,
            padding: "3px 8px",
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-sm)",
            background: "var(--bg-input)",
            border: "1px solid var(--border-default)",
            borderRadius: "var(--radius-sm)",
            color: "var(--fg-base)",
            boxSizing: "border-box",
          }}
        />
      </div>
      <div
        style={{
          padding: "4px 10px",
          borderBottom: "1px solid var(--border-default)",
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-muted)",
        }}
      >
        <span>
          {allActive
            ? `${candidates.length} instances (all)`
            : `${(selected as readonly string[]).length} of ${candidates.length} selected`}
        </span>
        <span style={{ display: "inline-flex", gap: 8 }}>
          <button
            type="button"
            onClick={selectAll}
            style={linkButton}
            disabled={allActive}
            title={allActive ? "Already showing all instances" : "Tick every checkbox"}
          >
            Select all
          </button>
          <span style={{ color: "var(--fg-subtle)" }}>·</span>
          <button
            type="button"
            onClick={clearAll}
            style={linkButton}
            disabled={!allActive && (selected as readonly string[]).length === 0}
            title="Untick every checkbox - no sources visible"
          >
            Clear all
          </button>
        </span>
      </div>
      <div style={{ flex: 1, minHeight: 0, overflow: "auto" }}>
        {filteredCandidates.length === 0 ? (
          <div
            style={{
              padding: "10px 12px",
              color: "var(--fg-subtle)",
              fontSize: "var(--fs-sm)",
            }}
          >
            no candidates match the filter.
          </div>
        ) : (
          filteredCandidates.map((name) => {
            const checked = allActive || selectedSet.has(name);
            return (
              <label
                key={name}
                style={{
                  display: "flex",
                  alignItems: "center",
                  gap: 8,
                  padding: "4px 10px",
                  cursor: "pointer",
                  fontFamily: "var(--font-mono)",
                  fontSize: "var(--fs-sm)",
                }}
                onMouseEnter={(e) => (e.currentTarget.style.background = "var(--accent-wash)")}
                onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
              >
                <input
                  type="checkbox"
                  checked={checked}
                  onChange={() => toggle(name)}
                  style={{ accentColor: "var(--accent)", margin: 0 }}
                />
                <span
                  style={{
                    overflow: "hidden",
                    textOverflow: "ellipsis",
                    whiteSpace: "nowrap",
                  }}
                  title={name}
                >
                  {name}
                </span>
              </label>
            );
          })
        )}
      </div>
      <div
        style={{
          padding: "6px 10px",
          borderTop: "1px solid var(--border-default)",
          display: "flex",
          justifyContent: "flex-end",
        }}
      >
        <button type="button" onClick={onClose} style={pillButton}>
          Done
        </button>
      </div>
    </div>,
    document.body,
  );
}

const linkButton: React.CSSProperties = {
  background: "transparent",
  border: "none",
  color: "var(--accent)",
  cursor: "pointer",
  padding: 0,
  fontFamily: "var(--font-ui)",
  fontSize: "var(--fs-sm)",
};
