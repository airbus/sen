// === event_watches.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

import {
  makeEventWatchKey,
  type EventWatchKey,
  type EventWatchSource,
} from "../core/watch_keys.js";

// Module-scoped store for watched event types. Separate from the property-watch store
// because events have no "current value" - just a delivery stream - and "watch" splits
// from "open as a table tab".

export interface EventWatchesState {
  readonly sources: readonly EventWatchSource[];
  readonly watchedKeys: ReadonlySet<EventWatchKey>;
}

function emptyState(): EventWatchesState {
  return { sources: [], watchedKeys: new Set() };
}

function derive(sources: readonly EventWatchSource[]): EventWatchesState {
  return { sources, watchedKeys: new Set(sources.map(makeEventWatchKey)) };
}

// `sources` persist; `watchedKeys` is recomputed on load. Versioned so a shape change
// can detect-and-drop stale payloads.

const EVENT_WATCHES_STORAGE_KEY = "webex.eventWatches";
const EVENT_WATCHES_STORAGE_VERSION = 1;

interface PersistedEventWatches {
  version: number;
  sources: readonly EventWatchSource[];
}

function isValidPersisted(raw: unknown): raw is PersistedEventWatches {
  if (!raw || typeof raw !== "object") return false;
  const r = raw as Partial<PersistedEventWatches>;
  return r.version === EVENT_WATCHES_STORAGE_VERSION && Array.isArray(r.sources);
}

function readPersisted(): EventWatchesState {
  try {
    const raw = localStorage.getItem(EVENT_WATCHES_STORAGE_KEY);
    if (!raw) return emptyState();
    const parsed = JSON.parse(raw) as unknown;
    if (!isValidPersisted(parsed)) return emptyState();
    return derive(parsed.sources);
  } catch {
    return emptyState();
  }
}

function writePersisted(sources: readonly EventWatchSource[]): void {
  try {
    const payload: PersistedEventWatches = {
      version: EVENT_WATCHES_STORAGE_VERSION,
      sources,
    };
    localStorage.setItem(EVENT_WATCHES_STORAGE_KEY, JSON.stringify(payload));
  } catch {
  }
}

const eventWatchStore = makeStore<EventWatchesState>(readPersisted());

// Identity-guard: sources reference only flips on real changes.
let lastPersistedSources = eventWatchStore.getState().sources;
eventWatchStore.subscribe(() => {
  const { sources } = eventWatchStore.getState();
  if (sources === lastPersistedSources) return;
  lastPersistedSources = sources;
  writePersisted(sources);
});

function addWatch(source: EventWatchSource): void {
  eventWatchStore.setState((prev) => {
    const key = makeEventWatchKey(source);
    if (prev.watchedKeys.has(key)) return prev;
    return derive([...prev.sources, source]);
  });
}

function removeWatch(key: EventWatchKey): void {
  eventWatchStore.setState((prev) => {
    if (!prev.watchedKeys.has(key)) return prev;
    return derive(prev.sources.filter((s) => makeEventWatchKey(s) !== key));
  });
}

function toggleWatch(source: EventWatchSource): void {
  const key = makeEventWatchKey(source);
  if (eventWatchStore.getState().watchedKeys.has(key)) removeWatch(key);
  else addWatch(source);
}

function clearAllWatches(): void {
  eventWatchStore.setState((prev) => (prev.sources.length === 0 ? prev : derive([])));
}

/** Stable identities for the lifetime of the page. */
export const eventWatchActions = Object.freeze({
  addWatch,
  removeWatch,
  toggleWatch,
  clearAllWatches,
});

export function useIsEventWatched(key: EventWatchKey): boolean {
  return useSelector(eventWatchStore, (s) => s.watchedKeys.has(key));
}

export function useEventWatches(): readonly EventWatchSource[] {
  return useSelector(eventWatchStore, (s) => s.sources);
}

/** @internal */
export function __resetEventWatchesForTests(): void {
  eventWatchStore.setState(emptyState());
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    eventWatchStore.dispose();
  });
}
