// === WatchGroups.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { createContext, useContext, useMemo, useState, type ReactNode } from "react";

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
import type { SyntheticListenerMap } from "@dnd-kit/core/dist/hooks/utilities/index.js";
import {
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";

import type { Client } from "@sen/client";

import {
  makeEventWatchKey,
  makePropertyKey,
  makeWatchKey,
  type EventWatchSource,
  type WatchSource,
} from "../core/watch_keys.js";
import { eventWatchActions } from "../state/event_watches.js";
import {
  useWatchGroupOrder,
  watchGroupOrderActions,
} from "../state/watch_group_order.js";
import { watchActions } from "../state/watch_plot.js";
import { ObjectLink } from "../ui/explorer_links.js";
import { HoverIconButton } from "../ui/buttons.js";
import { ChevronDownIcon, ChevronRightIcon, CloseIcon } from "../ui/icons.js";
import { TypeChip } from "../ui/chips.js";
import { FlashEnabledContext } from "../widgets/value_render/value_row.js";
import { EventWatchCard } from "./event_watch_card.js";
import { LeafGroupCard } from "./leaf_group_card.js";
import { ReadoutCard } from "./readout_card.js";
import "./watch.css";

export function ObjectGroupedGrid({
  client,
  sources,
  eventSources,
  modeOverrideKeys,
  toggleModeOverride,
  collapsedGroupKeys,
  toggleGroupCollapsed,
}: {
  client: Client | null;
  sources: readonly WatchSource[];
  /** Event watches sit alongside property cards in the same object group. */
  eventSources: readonly EventWatchSource[];
  modeOverrideKeys: ReadonlySet<string>;
  toggleModeOverride: (key: string) => void;
  collapsedGroupKeys: ReadonlySet<string>;
  toggleGroupCollapsed: (key: string) => void;
}) {
  const groups = useMemo(() => {
    const map = new Map<
      string,
      {
        objectName: string;
        sessionName: string;
        busName: string;
        className: string;
        sources: WatchSource[];
        eventSources: EventWatchSource[];
      }
    >();
    const ensure = (
      s: { objectName: string; sessionName: string; busName: string; className: string },
    ) => {
      const k = objectGroupKey(s);
      let g = map.get(k);
      if (!g) {
        g = {
          objectName: s.objectName,
          sessionName: s.sessionName,
          busName: s.busName,
          className: s.className,
          sources: [],
          eventSources: [],
        };
        map.set(k, g);
      }
      return g;
    };
    for (const s of sources) ensure(s).sources.push(s);
    for (const s of eventSources) ensure(s).eventSources.push(s);
    return Array.from(map.values());
  }, [sources, eventSources]);
  // Saved order floats curated groups to the top; new groups fall in at the tail.
  const groupOrder = useWatchGroupOrder();
  const orderedGroups = useMemo(() => {
    if (groupOrder.length === 0) return groups;
    const byKey = new Map(groups.map((g) => [objectGroupKey(g), g]));
    const out: typeof groups = [];
    const seen = new Set<string>();
    for (const k of groupOrder) {
      const g = byKey.get(k);
      if (g && !seen.has(k)) {
        out.push(g);
        seen.add(k);
      }
    }
    for (const g of groups) {
      const k = objectGroupKey(g);
      if (!seen.has(k)) {
        out.push(g);
        seen.add(k);
      }
    }
    return out;
  }, [groups, groupOrder]);
  const groupKeys = useMemo(() => orderedGroups.map(objectGroupKey), [orderedGroups]);
  const objectNameCounts = useMemo(() => {
    const m = new Map<string, number>();
    for (const g of orderedGroups) m.set(g.objectName, (m.get(g.objectName) ?? 0) + 1);
    return m;
  }, [orderedGroups]);
  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 4 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates }),
  );
  const [activeId, setActiveId] = useState<string | null>(null);
  const activeGroup =
    activeId !== null ? orderedGroups.find((g) => objectGroupKey(g) === activeId) ?? null : null;
  const handleDragStart = (event: DragStartEvent) => setActiveId(String(event.active.id));
  const handleDragEnd = (event: DragEndEvent) => {
    setActiveId(null);
    const { active, over } = event;
    if (!over || active.id === over.id) return;
    const from = groupKeys.indexOf(String(active.id));
    const to = groupKeys.indexOf(String(over.id));
    if (from < 0 || to < 0) return;
    const nextOrder = groupKeys.slice();
    const [moved] = nextOrder.splice(from, 1);
    if (moved !== undefined) nextOrder.splice(to, 0, moved);
    // Preserve saved order entries for groups not in the current grid (offscreen / other tab).
    const overflow = groupOrder.filter((k) => !nextOrder.includes(k));
    watchGroupOrderActions.setOrder([...nextOrder, ...overflow]);
  };
  return (
    <FlashEnabledContext.Provider value={true}>
    <div style={{ padding: 5, display: "flex", flexDirection: "column", gap: 5 }}>
      <DndContext
        sensors={sensors}
        collisionDetection={closestCenter}
        onDragStart={handleDragStart}
        onDragEnd={handleDragEnd}
        onDragCancel={() => setActiveId(null)}
      >
        <SortableContext items={groupKeys} strategy={verticalListSortingStrategy}>
          {orderedGroups.map((g) => {
            const groupKey = objectGroupKey(g);
            const groupCollapsed = collapsedGroupKeys.has(groupKey);
            const showBus = (objectNameCounts.get(g.objectName) ?? 0) > 1;
            return (
              <SortableObjectGroup
                key={groupKey}
                groupKey={groupKey}
                groupCollapsed={groupCollapsed}
                reorderable={orderedGroups.length > 1}
              >
                <div style={{ display: "flex", alignItems: "center", gap: 6, padding: "0 2px" }}>
                  <HoverIconButton
                    onClick={() => toggleGroupCollapsed(groupKey)}
                    ariaLabel={groupCollapsed ? "expand object group" : "collapse object group"}
                    tooltip={groupCollapsed ? "Expand" : "Collapse"}
                    icon={groupCollapsed ? <ChevronRightIcon /> : <ChevronDownIcon />}
                    size={16}
                  />
                  <SortableObjectGroupGrip
                    groupKey={groupKey}
                    disabled={orderedGroups.length <= 1}
                  />
                  <ObjectLink
                    selection={{
                      interestName:
                        (g.sources[0]?.interestName ?? g.eventSources[0]?.interestName) ?? "",
                      objectName: g.objectName,
                      className: g.className,
                      sessionName: g.sessionName,
                      busName: g.busName,
                    }}
                    style={{
                      fontFamily: "var(--font-mono)",
                      fontSize: "var(--fs-lg)",
                      fontWeight: 500,
                      color: "var(--fg-base)",
                      overflow: "hidden",
                      textOverflow: "ellipsis",
                      whiteSpace: "nowrap",
                      flex: "none",
                      maxWidth: "100%",
                      display: "inline-block",
                    }}
                    title={g.objectName}
                  />
                  {g.className && (
                    <span style={{ flex: "none" }}>
                      <TypeChip client={client} type={g.className} />
                    </span>
                  )}
                  {showBus && (g.sessionName || g.busName) && (
                    <span
                      style={{
                        fontFamily: "var(--font-mono)",
                        fontSize: "var(--fs-sm)",
                        color: "var(--fg-subtle)",
                        whiteSpace: "nowrap",
                      }}
                    >
                      {g.sessionName}.{g.busName}
                    </span>
                  )}
                  <span style={{ marginLeft: "auto", flex: "none" }}>
                    <HoverIconButton
                      onClick={() => {
                        for (const s of g.sources) watchActions.removeWatch(makeWatchKey(s));
                        for (const e of g.eventSources)
                          eventWatchActions.removeWatch(makeEventWatchKey(e));
                      }}
                      ariaLabel={`stop watching ${g.objectName}`}
                      tooltip="Stop watching this object"
                      icon={<CloseIcon />}
                      danger
                    />
                  </span>
                </div>
                {!groupCollapsed && (
                  <div style={{ columnWidth: 280, columnGap: 6 }}>
                    {groupByProperty(g.sources).map((pg) => {
                      const k = `${pg.source.propertyName}\0${pg.leafPaths.join(",")}`;
                      return (
                        <div
                          key={k}
                          style={{ breakInside: "avoid", marginBottom: 5, display: "block" }}
                        >
                          {pg.whole ? (
                            <ReadoutCard
                              client={client}
                              source={pg.source}
                              modeOverridden={modeOverrideKeys.has(makeWatchKey(pg.source))}
                              onToggleModeOverride={() =>
                                toggleModeOverride(makeWatchKey(pg.source))
                              }
                            />
                          ) : (
                            <LeafGroupCard
                              client={client}
                              source={pg.source}
                              leafPaths={pg.leafPaths}
                            />
                          )}
                        </div>
                      );
                    })}
                    {g.eventSources.map((es) => {
                      const key = makeEventWatchKey(es);
                      return (
                        <div
                          key={`event\0${es.eventName}`}
                          style={{ breakInside: "avoid", marginBottom: 5, display: "block" }}
                        >
                          <EventWatchCard
                            client={client}
                            source={es}
                            modeOverridden={modeOverrideKeys.has(key)}
                            onToggleModeOverride={() => toggleModeOverride(key)}
                          />
                        </div>
                      );
                    })}
                  </div>
                )}
              </SortableObjectGroup>
            );
          })}
        </SortableContext>
        {/* Lightweight overlay: rendering the full group would double subscription cost. */}
        <DragOverlay dropAnimation={null}>
          {activeGroup ? (
            <DragOverlayGroupHeader
              client={client}
              objectName={activeGroup.objectName}
              className={activeGroup.className}
            />
          ) : null}
        </DragOverlay>
      </DndContext>
    </div>
    </FlashEnabledContext.Provider>
  );
}

function SortableObjectGroup({
  groupKey,
  groupCollapsed,
  reorderable,
  children,
}: {
  groupKey: string;
  groupCollapsed: boolean;
  reorderable: boolean;
  children: ReactNode;
}) {
  const sortable = useSortable({ id: groupKey, disabled: !reorderable });
  return (
    <div
      ref={sortable.setNodeRef}
      className="watch-object-group"
      style={{
        background: "var(--watch-level-1)",
        border: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-lg)",
        padding: groupCollapsed ? "3px 6px" : "4px 6px 5px 6px",
        display: "flex",
        flexDirection: "column",
        gap: 4,
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
        transform: CSS.Transform.toString(sortable.transform),
        transition: sortable.transition,
        // Faint placeholder so the slot stays visible while the overlay shows the drag.
        opacity: sortable.isDragging ? 0.25 : 1,
      }}
      {...sortable.attributes}
    >
      <SortableGroupHandleContext.Provider
        value={{
          setActivatorNodeRef: sortable.setActivatorNodeRef,
          listeners: sortable.listeners,
          isDragging: sortable.isDragging,
        }}
      >
        {children}
      </SortableGroupHandleContext.Provider>
    </div>
  );
}

const SortableGroupHandleContext = createContext<{
  setActivatorNodeRef: (el: HTMLElement | null) => void;
  listeners: SyntheticListenerMap | undefined;
  isDragging: boolean;
} | null>(null);

function SortableObjectGroupGrip({
  disabled,
  groupKey: _groupKey,
}: {
  disabled: boolean;
  groupKey: string;
}) {
  const ctx = useContext(SortableGroupHandleContext);
  if (disabled || !ctx) return null;
  return (
    <span
      ref={ctx.setActivatorNodeRef}
      {...ctx.listeners}
      className="watch-object-group-grip"
      title="Drag to reorder"
      style={{
        color: "var(--fg-faint)",
        cursor: ctx.isDragging ? "grabbing" : "grab",
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        width: 14,
        height: 14,
        touchAction: "none",
        // Hover-reveal handled by watch.css rule; opacity 1 while dragging.
        opacity: ctx.isDragging ? 1 : 0,
        transition: "opacity 120ms ease",
      }}
    >
      <DotsGripGlyph />
    </span>
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

function DragOverlayGroupHeader({
  client,
  objectName,
  className,
}: {
  client: Client | null;
  objectName: string;
  className: string;
}) {
  return (
    <div
      style={{
        background: "rgba(20, 24, 36, 0.85)",
        border: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-lg)",
        boxShadow: "0 14px 32px rgba(0, 0, 0, 0.45)",
        padding: "4px 10px",
        display: "flex",
        alignItems: "center",
        gap: 6,
        cursor: "grabbing",
      }}
    >
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-lg)",
          fontWeight: 500,
          color: "var(--fg-base)",
        }}
      >
        {objectName}
      </span>
      {className && <TypeChip client={client} type={className} />}
    </div>
  );
}

// Whole-property + per-leaf watches shouldn't co-exist (smart-toggle prevents it); if they
// do, whole wins and the leaves are dropped from this view.
interface PropertyGroup {
  source: WatchSource;
  whole: boolean;
  leafPaths: string[];
}

function groupByProperty(sources: readonly WatchSource[]): PropertyGroup[] {
  const map = new Map<string, PropertyGroup>();
  for (const s of sources) {
    const key = makePropertyKey(s);
    let g = map.get(key);
    if (!g) {
      g = { source: { ...s, leafPath: "" }, whole: false, leafPaths: [] };
      map.set(key, g);
    }
    const lp = s.leafPath ?? "";
    if (lp === "") {
      g.whole = true;
    } else {
      g.leafPaths.push(lp);
    }
  }
  return Array.from(map.values());
}

// (object, session, bus) so the same name on two buses lands in two groups.
function objectGroupKey(s: { objectName: string; sessionName: string; busName: string }): string {
  return `${s.objectName}\0${s.sessionName}\0${s.busName}`;
}
