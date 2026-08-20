// === events_drawer_tabs.tsx ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo } from "react";

import { makeStore, useSelector } from "@sen/client/react";

import {
  makeEventTypeKey,
  type EventTypeKey,
  type EventTypeKeyParts,
} from "../core/watch_keys.js";

// Tracks the closable per-type tabs and the active selection; the Stream tab is implicit.

export interface EventTypeTab extends EventTypeKeyParts {
  key: EventTypeKey;
  /** `null` = no filter; `[]` = no rows pass; `[...]` = restrict to listed object names. */
  sourceFilters: readonly string[] | null;
}

export const STREAM_TAB = "stream" as const;
export type StreamTab = typeof STREAM_TAB;
export type ActiveTab = StreamTab | EventTypeKey;

interface EventsDrawerTabsState {
  readonly openTypes: readonly EventTypeTab[];
  readonly active: ActiveTab;
}

interface PersistedTab {
  interestName: string;
  className: string;
  eventName: string;
  sourceFilters?: readonly string[] | null;
}

interface PersistedState {
  openTypes?: readonly PersistedTab[];
  active?: string;
}

const STORAGE_KEY = "webex.events.drawerTabs";

function readPersisted(): EventsDrawerTabsState {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return { openTypes: [], active: STREAM_TAB };
    const parsed = JSON.parse(raw) as PersistedState;
    const openTypes: EventTypeTab[] = Array.isArray(parsed.openTypes)
      ? parsed.openTypes
          .filter(
            (t): t is PersistedTab =>
              !!t &&
              typeof t.interestName === "string" &&
              typeof t.className === "string" &&
              typeof t.eventName === "string",
          )
          .map((t) => {
            const parts: EventTypeKeyParts = {
              interestName: t.interestName,
              className: t.className,
              eventName: t.eventName,
            };
            const sourceFilters: readonly string[] | null = Array.isArray(t.sourceFilters)
              ? t.sourceFilters.filter((s): s is string => typeof s === "string")
              : null;
            return { ...parts, key: makeEventTypeKey(parts), sourceFilters };
          })
      : [];
    const active: ActiveTab =
      parsed.active === STREAM_TAB
        ? STREAM_TAB
        : openTypes.find((t) => t.key === parsed.active)?.key ?? STREAM_TAB;
    return { openTypes, active };
  } catch {
    return { openTypes: [], active: STREAM_TAB };
  }
}

function writeLocalStorage(state: EventsDrawerTabsState): void {
  try {
    const data: PersistedState = {
      openTypes: state.openTypes.map((t) => ({
        interestName: t.interestName,
        className: t.className,
        eventName: t.eventName,
        sourceFilters: t.sourceFilters,
      })),
      active: state.active,
    };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
  } catch {
  }
}

const eventsDrawerTabsStore = makeStore<EventsDrawerTabsState>(readPersisted());

// Seeded from current state so the first no-op tick after load is skipped.
let lastPersisted: EventsDrawerTabsState = eventsDrawerTabsStore.getState();
eventsDrawerTabsStore.subscribe(() => {
  const next = eventsDrawerTabsStore.getState();
  if (next === lastPersisted) return;
  lastPersisted = next;
  writeLocalStorage(next);
});

// `ensureSourceVisible`: new tab is narrowed to that source; existing tab with a
// non-null filter has the source appended (if missing).
function openType(
  parts: EventTypeKeyParts,
  options: { ensureSourceVisible?: string } = {},
): void {
  const key = makeEventTypeKey(parts);
  eventsDrawerTabsStore.setState((prev) => {
    const existing = prev.openTypes.find((t) => t.key === key);
    const source = options.ensureSourceVisible;
    if (existing) {
      let needsAppend = false;
      let nextFilters: readonly string[] | null = existing.sourceFilters;
      if (source !== undefined && existing.sourceFilters !== null && !existing.sourceFilters.includes(source)) {
        needsAppend = true;
        nextFilters = [...existing.sourceFilters, source];
      }
      const wasActive = prev.active === key;
      if (!needsAppend) {
        return wasActive ? prev : { ...prev, active: key };
      }
      const updated: EventTypeTab = { ...existing, sourceFilters: nextFilters };
      return {
        openTypes: prev.openTypes.map((t) => (t.key === key ? updated : t)),
        active: key,
      };
    }
    const tab: EventTypeTab = {
      ...parts,
      key,
      sourceFilters: source !== undefined ? [source] : null,
    };
    return { openTypes: [...prev.openTypes, tab], active: key };
  });
}

function setSourceFilters(
  key: EventTypeKey,
  filters: readonly string[] | null,
): void {
  eventsDrawerTabsStore.setState((prev) => {
    const idx = prev.openTypes.findIndex((t) => t.key === key);
    if (idx < 0) return prev;
    const existing = prev.openTypes[idx]!;
    if (sameFilters(existing.sourceFilters, filters)) return prev;
    const next = [...prev.openTypes];
    next[idx] = { ...existing, sourceFilters: filters };
    return { ...prev, openTypes: next };
  });
}

// Empty list stays []; we do NOT collapse to null (which would mean "no filter").
function removeSourceFromTab(parts: EventTypeKeyParts, objectName: string): void {
  const key = makeEventTypeKey(parts);
  eventsDrawerTabsStore.setState((prev) => {
    const idx = prev.openTypes.findIndex((t) => t.key === key);
    if (idx < 0) return prev;
    const existing = prev.openTypes[idx]!;
    if (existing.sourceFilters === null) return prev;
    if (!existing.sourceFilters.includes(objectName)) return prev;
    const next: EventTypeTab = {
      ...existing,
      sourceFilters: existing.sourceFilters.filter((s) => s !== objectName),
    };
    const nextOpenTypes = [...prev.openTypes];
    nextOpenTypes[idx] = next;
    return { ...prev, openTypes: nextOpenTypes };
  });
}

function sameFilters(
  a: readonly string[] | null,
  b: readonly string[] | null,
): boolean {
  if (a === b) return true;
  if (a === null || b === null) return false;
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

// If active, falls back to the left neighbour or Stream.
function closeType(key: EventTypeKey): void {
  eventsDrawerTabsStore.setState((prev) => {
    const idx = prev.openTypes.findIndex((t) => t.key === key);
    if (idx < 0) return prev;
    const openTypes = prev.openTypes.filter((t) => t.key !== key);
    let active: ActiveTab = prev.active;
    if (prev.active === key) {
      active = openTypes[idx - 1]?.key ?? openTypes[idx]?.key ?? STREAM_TAB;
    }
    return { openTypes, active };
  });
}

function setActive(tab: ActiveTab): void {
  eventsDrawerTabsStore.setState((prev) => (prev.active === tab ? prev : { ...prev, active: tab }));
}

export const eventsDrawerTabsActions = Object.freeze({
  openType,
  closeType,
  setActive,
  setSourceFilters,
  removeSourceFromTab,
});

export function useOpenEventTypeTabs(): readonly EventTypeTab[] {
  return useSelector(eventsDrawerTabsStore, (s) => s.openTypes);
}

export function useOpenEventTypeTabsByKey(): ReadonlyMap<EventTypeKey, EventTypeTab> {
  const openTypes = useOpenEventTypeTabs();
  return useMemo(() => {
    const m = new Map<EventTypeKey, EventTypeTab>();
    for (const t of openTypes) m.set(t.key, t);
    return m;
  }, [openTypes]);
}

export function useActiveEventsTab(): ActiveTab {
  return useSelector(eventsDrawerTabsStore, (s) => s.active);
}

/** True when the tab exists AND its filter is null or includes `objectName`. */
export function useIsEventVisibleInTable(
  parts: EventTypeKeyParts,
  objectName: string,
): boolean {
  const key = makeEventTypeKey(parts);
  return useSelector(eventsDrawerTabsStore, (s) => {
    const tab = s.openTypes.find((t) => t.key === key);
    if (!tab) return false;
    if (tab.sourceFilters === null) return true;
    return tab.sourceFilters.includes(objectName);
  });
}

/** @internal */
export function __resetEventsDrawerTabsForTests(): void {
  eventsDrawerTabsStore.setState({ openTypes: [], active: STREAM_TAB });
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    eventsDrawerTabsStore.dispose();
  });
}
