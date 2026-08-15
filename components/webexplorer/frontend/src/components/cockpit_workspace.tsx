// === CockpitWorkspace.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from "react";

import type { Client, ObjectHandle, PropertySpec } from "@sen/client";
import { useObject } from "@sen/client/react";

import { parsePinKey, selectionKey } from "../core/keys.js";
import { isComposite } from "../core/types.js";
import { extractAt } from "../core/value_walk.js";
import { useClassMemberGroups } from "../state/class_members.js";
import { useInPaneExplorer, useOpenExplorers } from "../state/explorer_hosts.js";
import { useInterestContainingOnBus } from "../state/interest_registry.js";
import { useLiveProperty } from "../state/live_value.js";
import {
  overviewFavoritesActions,
  useOverviewFavoriteOrder,
  useOverviewFavoritesForClass,
} from "../state/overview_favorites.js";
import {
  DndContext,
  DragOverlay,
  KeyboardSensor,
  PointerSensor,
  closestCenter,
  useSensor,
  useSensors,
  type DragEndEvent,
  type DragStartEvent,
} from "@dnd-kit/core";
import {
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";
import type { Selection } from "../state/selection.js";
import {
  cockpitCardLayoutsActions,
  overviewDensityActions,
  pinnedObjectsActions,
  useCockpitCardLayouts,
  useOverviewDensityMode,
  usePinnedObjects,
  type OverviewDensityMode,
  type PinnedObject,
} from "../state/ui_prefs.js";
import { BusChip, TypeChip } from "../ui/chips.js";
import { objectCardStyle } from "../ui/object_card_style.js";
import { classSwatch } from "../ui/class_color.js";
import { DangerPillButton, HoverIconButton } from "../ui/buttons.js";

import { LeftTruncated } from "../ui/text_primitives.js";
import { BoolInput } from "../widgets/value_edit/primitive_inputs.js";
import { EmptyPane, Mono } from "../ui/empty_state.js";
import { FilterInput } from "../ui/filter_input.js";
import { GridStackGrid, type GridStackItem } from "../ui/grid_stack_grid.js";
import { InlineValue } from "../widgets/value_render/inline_value.js";
import { ValueRenderer } from "../widgets/value_render/index.js";
import { DEFAULT_QUERY_NAME, interestNameFor } from "../widgets/interests/bus_queries.js";

import "./overview.css";

const DENSITY_PRESETS: Readonly<Record<OverviewDensityMode, { width: number; height: number; gap: number; maxProps: number }>> = {
  cockpit: { width: 130, height: 40, gap: 10, maxProps: 6 },
  tile: { width: 180, height: 64, gap: 6, maxProps: 0 },
};

const COCKPIT_DEFAULT_H_CELLS = 5;
const COCKPIT_DEFAULT_W_CELLS = 2;

const OverviewLayoutContext = createContext<{
  columnCount: number;
  cellHeight: number;
  gap: number;
  maxProps: number;
  density: OverviewDensityMode;
}>({
  columnCount: 1,
  cellHeight: DENSITY_PRESETS.cockpit.height,
  gap: DENSITY_PRESETS.cockpit.gap,
  maxProps: DENSITY_PRESETS.cockpit.maxProps,
  density: "cockpit",
});

// Pin keys open in any Object Explorer; drives the "selected" ring on cockpit cards.
const OverviewExplorerSelectionContext = createContext<ReadonlySet<string>>(new Set());

interface PinnedView {
  pinKey: string;
  sessionName: string;
  busName: string;
  objectName: string;
  className: string;
}

function pinnedViews(pinned: ReadonlyMap<string, PinnedObject>): PinnedView[] {
  const out: PinnedView[] = [];
  for (const [pinKey, payload] of pinned) {
    const parsed = parsePinKey(pinKey);
    if (parsed === null) continue;
    out.push({
      pinKey,
      sessionName: parsed.sessionName,
      busName: parsed.busName,
      objectName: parsed.objectName,
      className: payload.className,
    });
  }
  return out;
}

function matchesFilter(pin: PinnedView, lowerQuery: string): boolean {
  if (!lowerQuery) return true;
  if (pin.objectName.toLowerCase().includes(lowerQuery)) return true;
  if (pin.className.toLowerCase().includes(lowerQuery)) return true;
  if (`${pin.sessionName}.${pin.busName}`.toLowerCase().includes(lowerQuery)) return true;
  return false;
}

function compareViewsByPath(a: PinnedView, b: PinnedView): number {
  const ba = `${a.sessionName}.${a.busName}.${a.objectName}`;
  const bb = `${b.sessionName}.${b.busName}.${b.objectName}`;
  return ba.localeCompare(bb);
}

// Cluster by className first so the class-color accent band carries group identity.
function sortPinnedViews(views: PinnedView[]): PinnedView[] {
  return [...views].sort((a, b) => {
    const byClass = a.className.localeCompare(b.className);
    if (byClass !== 0) return byClass;
    const byName = a.objectName.localeCompare(b.objectName);
    if (byName !== 0) return byName;
    return compareViewsByPath(a, b);
  });
}

export interface CockpitWorkspaceProps {
  client: Client | null;
  onOpenInPaneExplorer: (sel: Selection) => void;
}

export function CockpitWorkspace({ client, onOpenInPaneExplorer }: CockpitWorkspaceProps) {
  const pinned = usePinnedObjects();
  const [filterText, setFilterText] = useState("");
  const views = useMemo(() => pinnedViews(pinned), [pinned]);
  const sortedPins = useMemo(() => sortPinnedViews(views), [views]);
  const lowerQuery = filterText.trim().toLowerCase();
  const filterActive = lowerQuery.length > 0;
  const viewByPinKey = useMemo(() => {
    const out = new Map<string, PinnedView>();
    for (const v of views) out.set(v.pinKey, v);
    return out;
  }, [views]);
  const filterPredicate = useMemo<((pinKey: string) => boolean) | undefined>(() => {
    if (!filterActive) return undefined;
    return (pinKey: string) => {
      const v = viewByPinKey.get(pinKey);
      if (!v) return false;
      return matchesFilter(v, lowerQuery);
    };
  }, [filterActive, lowerQuery, viewByPinKey]);
  const visibleMatchCount = useMemo(() => {
    if (!filterPredicate) return viewByPinKey.size;
    let n = 0;
    for (const v of viewByPinKey.values()) if (filterPredicate(v.pinKey)) n++;
    return n;
  }, [filterPredicate, viewByPinKey]);

  // Hooks unconditional: the empty-pane early return below would otherwise change hook order.
  const density = useOverviewDensityMode();
  const preset = DENSITY_PRESETS[density];
  const tidyRef = useRef<(() => void) | null>(null);
  const [containerWidth, setContainerWidth] = useState(0);
  // Callback ref: useEffect would see null on the cold path (empty pane renders first).
  const roRef = useRef<ResizeObserver | null>(null);
  const containerRef = useCallback((el: HTMLDivElement | null) => {
    if (roRef.current !== null) {
      roRef.current.disconnect();
      roRef.current = null;
    }
    if (el === null) {
      setContainerWidth(0);
      return;
    }
    setContainerWidth(el.clientWidth);
    const ro = new ResizeObserver((entries) => {
      const entry = entries[0];
      if (!entry) return;
      setContainerWidth(entry.contentRect.width);
    });
    ro.observe(el);
    roRef.current = ro;
  }, []);
  const columnCount = Math.max(
    1,
    Math.floor((containerWidth + preset.gap) / (preset.width + preset.gap)),
  );
  const layoutContextValue = useMemo(
    () => ({
      columnCount,
      cellHeight: preset.height,
      gap: preset.gap,
      maxProps: preset.maxProps,
      density,
    }),
    [columnCount, preset.height, preset.gap, preset.maxProps, density],
  );

  const inPaneExplorer = useInPaneExplorer();
  const openExplorers = useOpenExplorers();
  const selectedPinKeys = useMemo(() => {
    const out = new Set<string>();
    if (inPaneExplorer) out.add(selectionKey(inPaneExplorer));
    for (const e of openExplorers.values()) out.add(selectionKey(e.selection));
    return out;
  }, [inPaneExplorer, openExplorers]);

  const hasPins = pinned.size > 0;
  const noMatches = filterActive && visibleMatchCount === 0;

  if (!hasPins) {
    return (
      <EmptyPane
        heading="No pinned objects yet."
        help={[
          <>
            Open the <Mono>Objects</Mono> tab and click the pin icon at the top-right
            of any card to add it here.
          </>,
          <>
            Pins survive reload, so this view is a stable cockpit for the handful of
            objects you actually keep an eye on.
          </>,
        ]}
      />
    );
  }

  return (
    <div
      ref={containerRef}
      style={{
        display: "flex",
        flexDirection: "column",
        gap: "var(--pane-gutter-top)",
        padding: "0 0 16px",
      }}
    >
      <OverviewToolbar
        filterText={filterText}
        onChangeFilterText={setFilterText}
        density={density}
        onChangeDensity={overviewDensityActions.set}
        onTidy={density === "cockpit" ? () => tidyRef.current?.() : undefined}
      />
      {noMatches && (
        <div
          style={{
            padding: "12px 6px",
            fontFamily: "var(--font-ui)",
            fontSize: "var(--fs-sm)",
            color: "var(--fg-subtle)",
          }}
        >
          No cards match <Mono>{filterText}</Mono>.
        </div>
      )}
      <OverviewLayoutContext.Provider value={layoutContextValue}>
        <OverviewExplorerSelectionContext.Provider value={selectedPinKeys}>
          <CompactCardsGrid
            pins={sortedPins}
            client={client}
            onOpenInPaneExplorer={onOpenInPaneExplorer}
            filterPredicate={filterPredicate}
            tidyRef={tidyRef}
          />
        </OverviewExplorerSelectionContext.Provider>
      </OverviewLayoutContext.Provider>
    </div>
  );
}

function OverviewToolbar({
  filterText,
  onChangeFilterText,
  density,
  onChangeDensity,
  onTidy,
}: {
  filterText: string;
  onChangeFilterText: (v: string) => void;
  density: OverviewDensityMode;
  onChangeDensity: (next: OverviewDensityMode) => void;
  /** Renders a "Tidy" pill when set; undefined in tile mode (auto-packed already). */
  onTidy: (() => void) | undefined;
}) {
  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        gap: 10,
        position: "sticky",
        top: 0,
        zIndex: 5,
        height: "var(--topbar-height)",
        padding: "0 12px",
        borderRadius: "var(--radius-xl)",
        border: "1px solid var(--border-glass)",
        background: "var(--surface-pane-nav)",
        boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
        flex: "none",
      }}
    >
      <FilterInput
        value={filterText}
        onChange={onChangeFilterText}
        placeholder="Filter by name or class..."
        ariaLabel="filter pinned cards"
      />
      <span style={{ flex: 1 }} />
      {onTidy && <TidyButton onClick={onTidy} />}
      <ClearAllPinsButton />
      <CompactToggle density={density} onChange={onChangeDensity} />
    </div>
  );
}

function ClearAllPinsButton() {
  return (
    <DangerPillButton
      onClick={pinnedObjectsActions.clearAll}
      title="Unpin every object in the Overview"
    />
  );
}

function TidyButton({ onClick }: { onClick: () => void }) {
  return (
    <button
      type="button"
      className="pressable"
      onClick={onClick}
      title="Pack cards up and left, preserving order"
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 4,
        height: 24,
        padding: "0 10px",
        borderRadius: 999,
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-sm)",
        color: "var(--fg-base)",
        cursor: "pointer",
        flex: "none",
      }}
    >
      Tidy
    </button>
  );
}

function CompactToggle({
  density,
  onChange,
}: {
  density: OverviewDensityMode;
  onChange: (next: OverviewDensityMode) => void;
}) {
  const compact = density === "tile";
  return (
    <label
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-xs)",
        color: "var(--fg-muted)",
        letterSpacing: "0.04em",
        textTransform: "uppercase",
        cursor: "pointer",
        userSelect: "none",
        flex: "none",
      }}
      title={
        compact
          ? "Show live values on each card"
          : "Hide live values for a denser overview"
      }
    >
      Compact
      <BoolInput value={compact} onChange={(v) => onChange(v ? "tile" : "cockpit")} />
    </label>
  );
}

function CompactCardsGrid({
  pins,
  client,
  onOpenInPaneExplorer,
  filterPredicate,
  tidyRef,
}: {
  pins: PinnedView[];
  client: Client | null;
  onOpenInPaneExplorer: (sel: Selection) => void;
  filterPredicate: ((pinKey: string) => boolean) | undefined;
  tidyRef?: React.MutableRefObject<(() => void) | null>;
}) {
  const { columnCount, cellHeight, gap, density } = useContext(OverviewLayoutContext);
  const cockpitLayouts = useCockpitCardLayouts();
  const visiblePins = useMemo(
    () => (filterPredicate ? pins.filter((p) => filterPredicate(p.pinKey)) : pins),
    [pins, filterPredicate],
  );
  const dragAllowed = filterPredicate === undefined;
  const isCockpit = density === "cockpit";
  const items = useMemo<GridStackItem<string>[]>(
    () =>
      visiblePins.map((pin) => {
        if (isCockpit) {
          // Omit x/y for unsaved layouts so gridstack auto-places.
          const saved = cockpitLayouts.get(pin.pinKey);
          return {
            key: pin.pinKey,
            ...(saved !== undefined ? { x: saved.x, y: saved.y } : {}),
            w: saved?.w ?? COCKPIT_DEFAULT_W_CELLS,
            h: saved?.h ?? COCKPIT_DEFAULT_H_CELLS,
            minW: 1,
            maxW: columnCount,
            minH: 1,
            render: () => (
              <ObjectSummaryCard
                client={client}
                pin={pin}
                onOpenInPaneExplorer={onOpenInPaneExplorer}
                dragAllowed={dragAllowed}
              />
            ),
          };
        }
        return {
          key: pin.pinKey,
          w: 1,
          h: 1,
          minW: 1,
          maxW: 1,
          minH: 1,
          maxH: 1,
          render: () => (
            <ObjectSummaryCard
              client={client}
              pin={pin}
              onOpenInPaneExplorer={onOpenInPaneExplorer}
              dragAllowed={dragAllowed}
            />
          ),
        };
      }),
    [visiblePins, client, onOpenInPaneExplorer, cockpitLayouts, columnCount, dragAllowed, isCockpit],
  );
  return (
    <GridStackGrid<string>
      items={items}
      columnCount={columnCount}
      cellHeight={cellHeight}
      gap={gap}
      disableDrag={!dragAllowed}
      {...(isCockpit ? { resizeHandles: "se" } : { disableResize: true })}
      {...(isCockpit ? { float: true } : { compact: true })}
      dragHandleSelector=".overview-card-drag"
      className="overview-card-grid"
      tidyRef={tidyRef}
      onLayoutChange={(next) => {
        if (!isCockpit) return;
        cockpitCardLayoutsActions.setMany(
          next.map((e) => ({
            pinKey: e.key,
            layout: { x: e.x, y: e.y, w: e.w, h: e.h },
          })),
        );
      }}
    />
  );
}

function ObjectSummaryCard({
  client,
  pin,
  onOpenInPaneExplorer,
  dragAllowed,
}: {
  client: Client | null;
  pin: PinnedView;
  onOpenInPaneExplorer: (sel: Selection) => void;
  dragAllowed: boolean;
}) {
  const [hover, setHover] = useState(false);
  const explorerSelection = useContext(OverviewExplorerSelectionContext);
  const isSelected = explorerSelection.has(pin.pinKey);
  const { density } = useContext(OverviewLayoutContext);
  const swatch = useMemo(() => classSwatch(pin.className), [pin.className]);
  // Pins are interest-independent: resolve against any open interest on the bus.
  const resolved = useInterestContainingOnBus(pin.sessionName, pin.busName, pin.objectName);
  const obj = useObject(resolved?.interest ?? null, pin.objectName);
  const online = !!obj;
  const selection = useMemo<Selection>(
    () => ({
      interestName: resolved?.name ?? interestNameFor(pin.sessionName, pin.busName, DEFAULT_QUERY_NAME),
      objectName: pin.objectName,
      className: pin.className,
      sessionName: pin.sessionName,
      busName: pin.busName,
    }),
    [resolved, pin.sessionName, pin.busName, pin.objectName, pin.className],
  );

  return (
    <article
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      onClick={() => onOpenInPaneExplorer(selection)}
      style={objectCardStyle(swatch, hover, isSelected, density)}
    >
      {density === "tile" ? (
        <div style={{ display: "flex", flexDirection: "column", gap: 3, minWidth: 0, flex: 1, justifyContent: "center" }}>
          <span style={{ display: "flex", alignItems: "center", gap: 6, minWidth: 0 }}>
            <span
              aria-hidden="true"
              title={online ? "online" : "offline - no open interest matches"}
              style={{
                width: 5,
                height: 5,
                borderRadius: "50%",
                background: online ? "var(--ok)" : "var(--fg-muted)",
                boxShadow: online ? "0 0 4px var(--ok)" : "none",
                flex: "none",
              }}
            />
            <span
              style={{
                fontFamily: "var(--font-mono)",
                fontSize: "var(--fs-md)",
                fontWeight: 500,
                color: online ? "var(--fg-base)" : "var(--fg-muted)",
                whiteSpace: "nowrap",
                overflow: "hidden",
                textOverflow: "ellipsis",
                flex: 1,
                minWidth: 0,
              }}
              title={pin.objectName}
            >
              {pin.objectName}
            </span>
          </span>
          <span
            style={{
              display: "flex",
              gap: 4,
              minWidth: 0,
              overflow: "hidden",
              alignItems: "center",
            }}
          >
            <TypeChip client={client} type={pin.className} leftTruncate />
            <span style={{ flex: "none" }}>
              <BusChip sessionName={pin.sessionName} busName={pin.busName} />
            </span>
          </span>
        </div>
      ) : (
        // Header doubles as gridstack drag handle via .overview-card-drag; click-without-
        // drag still bubbles to the article's onClick.
        <div
          className={dragAllowed ? "overview-card-drag" : undefined}
          style={{
            display: "flex",
            flexDirection: "column",
            gap: 4,
            minWidth: 0,
            paddingBottom: 6,
            borderBottom: "1px solid var(--border-default)",
            cursor: dragAllowed ? "grab" : undefined,
          }}
        >
          <span style={{ display: "flex", alignItems: "center", gap: 6, minWidth: 0 }}>
            <span
              aria-hidden="true"
              title={online ? "online" : "offline - no open interest matches"}
              style={{
                width: 5,
                height: 5,
                borderRadius: "50%",
                background: online ? "var(--ok)" : "var(--fg-muted)",
                boxShadow: online ? "0 0 4px var(--ok)" : "none",
                flex: "none",
              }}
            />
            <span
              style={{
                fontFamily: "var(--font-mono)",
                fontSize: "var(--fs-lg)",
                fontWeight: 500,
                color: online ? "var(--fg-base)" : "var(--fg-muted)",
                whiteSpace: "nowrap",
                overflow: "hidden",
                textOverflow: "ellipsis",
                flex: 1,
                minWidth: 0,
              }}
              title={pin.objectName}
            >
              {pin.objectName}
            </span>
          </span>
          <span
            style={{
              display: "flex",
              gap: 4,
              minWidth: 0,
              overflow: "hidden",
              alignItems: "center",
            }}
          >
            <TypeChip client={client} type={pin.className} leftTruncate />
            <span style={{ flex: "none" }}>
              <BusChip sessionName={pin.sessionName} busName={pin.busName} />
            </span>
          </span>
        </div>
      )}
      {density === "cockpit" && online && client && obj && (
        <CardPropertyStrip client={client} obj={obj} className={pin.className} />
      )}
      {density === "cockpit" && (
        <div
          style={{
            position: "absolute",
            top: 7,
            right: 9,
            display: "flex",
            alignItems: "center",
            gap: 2,
            opacity: hover ? 1 : 0,
            transition: "opacity 0.12s ease",
          }}
        >
          <HoverIconButton
            size={18}
            danger
            ariaLabel="unpin from overview"
            tooltip="Unpin"
            onClick={(e) => {
              e.stopPropagation();
              pinnedObjectsActions.toggle(pin.pinKey, pin.className);
            }}
            icon={
              <svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true">
                <path
                  d="M1.5 1.5 L8.5 8.5 M8.5 1.5 L1.5 8.5"
                  stroke="currentColor"
                  strokeWidth="1.4"
                  strokeLinecap="round"
                />
              </svg>
            }
          />
        </div>
      )}
      {dragAllowed && <CardDragGrip hover={hover} />}
    </article>
  );
}

// Don't preventDefault on mousedown -- gridstack's drag manager bails on canceled events.
function CardDragGrip({ hover }: { hover: boolean }) {
  return (
    <span
      className="overview-card-drag"
      role="button"
      aria-label="reorder card"
      title="Drag to reorder"
      onClick={(e) => e.stopPropagation()}
      onMouseDown={(e) => e.stopPropagation()}
      style={{
        position: "absolute",
        top: 4,
        left: 6,
        width: 18,
        height: 14,
        display: "grid",
        placeItems: "center",
        cursor: "grab",
        color: "var(--fg-muted)",
        opacity: hover ? 1 : 0,
        transition: "opacity 0.12s ease",
        userSelect: "none",
        WebkitUserSelect: "none",
      }}
    >
      <svg width="10" height="14" viewBox="0 0 10 14" aria-hidden="true">
        <circle cx="3" cy="3" r="1.1" fill="currentColor" />
        <circle cx="7" cy="3" r="1.1" fill="currentColor" />
        <circle cx="3" cy="7" r="1.1" fill="currentColor" />
        <circle cx="7" cy="7" r="1.1" fill="currentColor" />
        <circle cx="3" cy="11" r="1.1" fill="currentColor" />
        <circle cx="7" cy="11" r="1.1" fill="currentColor" />
      </svg>
    </span>
  );
}

function CardPropertyStrip({
  client,
  obj,
  className,
}: {
  client: Client;
  obj: ObjectHandle;
  className: string;
}) {
  const groups = useClassMemberGroups(client, className);
  const { maxProps, density } = useContext(OverviewLayoutContext);
  const classFavoriteSuffixes = useOverviewFavoritesForClass(className);
  const favoriteOrder = useOverviewFavoriteOrder(className);
  // Favorites mode: only favorites in user order. No favorites: first `maxProps` declared.
  const picked = useMemo<PickedCardItem[]>(() => {
    if (!groups) return [];
    // Bucket by propertyName so the walk is O(F + P) instead of O(P*F).
    const leafsByProp = new Map<string, string[]>();
    for (const sfx of classFavoriteSuffixes) {
      const hash = sfx.indexOf("#");
      const propName = hash >= 0 ? sfx.slice(0, hash) : sfx;
      const leafPath = hash >= 0 ? sfx.slice(hash + 1) : "";
      let bucket = leafsByProp.get(propName);
      if (!bucket) {
        bucket = [];
        leafsByProp.set(propName, bucket);
      }
      bucket.push(leafPath);
    }
    for (const bucket of leafsByProp.values()) bucket.sort();
    const byKey = new Map<string, PickedCardItem>();
    const declaredOrderKeys: string[] = [];
    for (const g of groups) {
      for (const p of g.properties) {
        const bucket = leafsByProp.get(p.name);
        if (!bucket) continue;
        for (const lp of bucket) {
          if (lp === "") {
            byKey.set(p.name, { prop: p, leafPath: "" });
            declaredOrderKeys.push(p.name);
          } else {
            const k = `${p.name}#${lp}`;
            byKey.set(k, { prop: p, leafPath: lp });
            declaredOrderKeys.push(k);
          }
        }
      }
    }
    if (byKey.size > 0) {
      const out: PickedCardItem[] = [];
      const seen = new Set<string>();
      // Saved order first; newly-starred entries tail-append in declared order.
      if (favoriteOrder) {
        for (const suffix of favoriteOrder) {
          const item = byKey.get(suffix);
          if (item && !seen.has(suffix)) {
            out.push(item);
            seen.add(suffix);
          }
        }
      }
      for (const k of declaredOrderKeys) {
        if (seen.has(k)) continue;
        const item = byKey.get(k);
        if (!item) continue;
        out.push(item);
        seen.add(k);
      }
      return out;
    }
    const out: PickedCardItem[] = [];
    for (const g of groups) {
      for (const p of g.properties) {
        if (out.length >= maxProps) break;
        out.push({ prop: p, leafPath: "" });
      }
      if (out.length >= maxProps) break;
    }
    return out;
  }, [groups, maxProps, classFavoriteSuffixes, favoriteOrder]);
  const itemIds = useMemo(
    () => picked.map((it) => (it.leafPath === "" ? it.prop.name : `${it.prop.name}#${it.leafPath}`)),
    [picked],
  );
  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 4 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates }),
  );
  const [activeId, setActiveId] = useState<string | null>(null);
  if (picked.length === 0) return null;
  // Suppress "+N more" once any favorites exist; exclusions are intentional.
  const hasAnyFavoriteForClass = classFavoriteSuffixes.size > 0;
  const totalProps = groups ? groups.reduce((n, g) => n + g.properties.length, 0) : 0;
  const moreCount = hasAnyFavoriteForClass ? 0 : Math.max(0, totalProps - picked.length);
  const reorderable = hasAnyFavoriteForClass && picked.length > 1;
  const activeItem =
    activeId !== null ? picked[itemIds.indexOf(activeId)] ?? null : null;
  const handleDragStart = (event: DragStartEvent) => setActiveId(String(event.active.id));
  const handleDragEnd = (event: DragEndEvent) => {
    setActiveId(null);
    const { active, over } = event;
    if (!over || active.id === over.id) return;
    const from = itemIds.indexOf(String(active.id));
    const to = itemIds.indexOf(String(over.id));
    if (from < 0 || to < 0) return;
    // Append overflow saved entries that didn't render so reordering doesn't drop them.
    const visibleNext = itemIds.slice();
    const [moved] = visibleNext.splice(from, 1);
    if (moved !== undefined) visibleNext.splice(to, 0, moved);
    const prevOrder = favoriteOrder ?? [];
    const overflow = prevOrder.filter((s) => !visibleNext.includes(s));
    overviewFavoritesActions.setOrder(className, [...visibleNext, ...overflow]);
  };
  const stripBody = (
    <div
      className="overview-card-property-strip"
      style={{
        display: "flex",
        flexDirection: "column",
        gap: density === "tile" ? 1 : 3,
        marginTop: density === "tile" ? 1 : 2,
        flex: "1 1 0",
        minHeight: 0,
        overflowY: "auto",
        overflowX: "hidden",
        marginRight: -12,
        paddingRight: 8,
      }}
    >
      {picked.map((item, i) => (
        <CardPropertyRow
          key={itemIds[i]}
          id={itemIds[i]!}
          client={client}
          obj={obj}
          prop={item.prop}
          leafPath={item.leafPath}
          reorderable={reorderable}
        />
      ))}
      {moreCount > 0 && (
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
            marginTop: 1,
          }}
        >
          +{moreCount} more
        </span>
      )}
    </div>
  );
  if (!reorderable) return stripBody;
  return (
    <DndContext
      sensors={sensors}
      collisionDetection={closestCenter}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      onDragCancel={() => setActiveId(null)}
    >
      <SortableContext items={itemIds} strategy={verticalListSortingStrategy}>
        {stripBody}
      </SortableContext>
      {/* Overlay floats the dragged row above the strip's overflow:hidden clip. */}
      <DragOverlay dropAnimation={null}>
        {activeItem ? (
          <CardPropertyRow
            id={activeId ?? "overlay"}
            client={client}
            obj={obj}
            prop={activeItem.prop}
            leafPath={activeItem.leafPath}
            reorderable
            overlay
          />
        ) : null}
      </DragOverlay>
    </DndContext>
  );
}

// `leafPath === ""` -> whole property; otherwise a leaf inside a composite value.
interface PickedCardItem {
  prop: PropertySpec;
  leafPath: string;
}

function CardPropertyRow({
  id,
  client,
  obj,
  prop,
  leafPath,
  reorderable,
  overlay = false,
}: {
  id: string;
  client: Client;
  obj: ObjectHandle;
  prop: PropertySpec;
  leafPath: string;
  reorderable: boolean;
  /** Portal overlay copy: skip useSortable, render with elevated chrome. */
  overlay?: boolean;
}) {
  const live = useLiveProperty(obj, prop.name);
  const sortable = useSortable({ id, disabled: !reorderable || overlay });
  const isDragging = !overlay && sortable.isDragging;
  let content: React.ReactNode;
  if (leafPath !== "") {
    const leafValue = extractAt(live.value, leafPath);
    const label = `${prop.name}.${leafPath}`;
    content = (
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "minmax(0, 1fr) auto",
          alignItems: "baseline",
          columnGap: 8,
        }}
      >
        <LeftTruncated
          text={label}
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
          }}
        />
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-base)",
            textAlign: "right",
            whiteSpace: "nowrap",
            overflow: "hidden",
            textOverflow: "ellipsis",
            minWidth: 0,
          }}
        >
          <ValueRenderer client={client} declaredType={prop.type} value={leafValue} />
        </span>
      </div>
    );
  } else if (isComposite(client, prop.type)) {
    content = (
      <div style={{ display: "flex", flexDirection: "column", gap: 1, minWidth: 0 }}>
        <LeftTruncated
          text={prop.name}
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
          }}
        />
        <div
          style={{
            paddingLeft: 10,
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-base)",
            minWidth: 0,
          }}
        >
          <ValueRenderer client={client} declaredType={prop.type} value={live.value} />
        </div>
      </div>
    );
  } else {
    content = (
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "minmax(0, 1fr) auto",
          alignItems: "baseline",
          columnGap: 8,
        }}
      >
        <LeftTruncated
          text={prop.name}
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
          }}
        />
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-base)",
            textAlign: "right",
            whiteSpace: "nowrap",
            overflow: "hidden",
            textOverflow: "ellipsis",
            minWidth: 0,
          }}
        >
          <InlineValue client={client} declaredType={prop.type} value={live.value} />
        </span>
      </div>
    );
  }
  if (!reorderable) return content;
  // col 1 = 14px grip (hover-revealed via overview.css); col 2 = content. Overlay pins grip on.
  const wrapperStyle: React.CSSProperties = overlay
    ? {
        display: "grid",
        gridTemplateColumns: "14px 1fr",
        alignItems: "center",
        columnGap: 4,
        background: "var(--bg-elevated)",
        boxShadow: "0 8px 20px rgba(0, 0, 0, 0.32)",
        borderRadius: "var(--radius-sm)",
        padding: "2px 6px",
      }
    : {
        display: "grid",
        gridTemplateColumns: "14px 1fr",
        alignItems: "center",
        columnGap: 4,
        transform: CSS.Transform.toString(sortable.transform),
        transition: sortable.transition,
        opacity: isDragging ? 0.2 : 1,
      };
  return (
    <div
      ref={overlay ? undefined : sortable.setNodeRef}
      className={overlay ? undefined : "overview-card-prop-row"}
      style={wrapperStyle}
      {...(overlay ? {} : sortable.attributes)}
    >
      <span
        ref={overlay ? undefined : sortable.setActivatorNodeRef}
        {...(overlay ? {} : sortable.listeners)}
        className={overlay ? undefined : "overview-card-prop-grip"}
        title="Drag to reorder"
        style={{
          color: "var(--fg-faint)",
          cursor: isDragging || overlay ? "grabbing" : "grab",
          display: "inline-flex",
          alignItems: "center",
          justifyContent: "center",
          width: 14,
          height: 14,
          touchAction: "none",
          opacity: overlay ? 1 : 0,
          transition: "opacity 120ms ease",
        }}
      >
        <DotsGripGlyph />
      </span>
      {content}
    </div>
  );
}

function DotsGripGlyph() {
  return (
    <svg width="10" height="10" viewBox="0 0 10 10" aria-hidden>
      <circle cx="3" cy="2" r="0.9" fill="currentColor" />
      <circle cx="7" cy="2" r="0.9" fill="currentColor" />
      <circle cx="3" cy="5" r="0.9" fill="currentColor" />
      <circle cx="7" cy="5" r="0.9" fill="currentColor" />
      <circle cx="3" cy="8" r="0.9" fill="currentColor" />
      <circle cx="7" cy="8" r="0.9" fill="currentColor" />
    </svg>
  );
}
