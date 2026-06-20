// === event_store.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, type ReactNode } from "react";

import {
  makeBufferCell,
  makeBufferStore,
  useCell,
  useView,
} from "@sen/client/react";
import type { ObjectHandle, Var } from "@sen/client";

import { objectEventKey } from "../core/keys.js";

// Time-windowed per-object event log plus a merged global log. Collectors are ref-counted
// by (interest, object) so multiple mounts share one wire callback.

const RETENTION_SECONDS_DEFAULT = 60;
const TRIM_INTERVAL_MS = 1000;
const HARD_EVENT_CAP = 4000;
const ALL_EVENTS_CAP = 5000;
// Slack above each cap; amortizes the O(N) shift over DROP_CHUNK appends.
const DROP_CHUNK = 256;

function dropFront<T>(buf: T[], n: number): void {
  if (n <= 0) return;
  if (n >= buf.length) {
    buf.length = 0;
    return;
  }
  buf.copyWithin(0, n);
  buf.length -= n;
}

export interface EventDelivery {
  seq: number;
  interestName: string;
  sessionName: string;
  busName: string;
  objectName: string;
  className: string;
  eventName: string;
  args: Var[];
  /** Server-side wire timestamp (`YYYY-MM-DD HH:MM:SS uuuuuu`, UTC). */
  timestamp: string;
  /** Local-clock ms; drives the TTL trim. */
  at: number;
  /** Lowercased `eventName \0 objectName \0 className \0 interestName` for filter scans. */
  lowerSearch: string;
}

const EMPTY_EVENTS: readonly EventDelivery[] = Object.freeze([]);

// Wrapper identity flips per invalidate; the inner `.events` array is the live mutable
// backing. Downstream useMemo deps must point at the wrapper, never at `.events`.
export interface EventLogView {
  readonly events: readonly EventDelivery[];
}

const EMPTY_EVENT_LOG: EventLogView = Object.freeze({ events: EMPTY_EVENTS });

const eventsByObjectBacking = new Map<string, EventDelivery[]>();
const allEventsBacking: EventDelivery[] = [];

const eventsByObjectStore = makeBufferStore<string, EventLogView>({
  produce: (key) => ({ events: eventsByObjectBacking.get(key) ?? EMPTY_EVENTS }),
});

const allEventsCell = makeBufferCell<EventLogView>({
  produce: () => ({ events: allEventsBacking }),
});

let seq = 0;
let nextGeneration = 0;
let retentionSecondsRef = RETENTION_SECONDS_DEFAULT;

export interface EventCollectorSpec {
  obj: ObjectHandle;
  interestName: string;
  sessionName: string;
  busName: string;
  objectName: string;
  className: string;
  eventNames: readonly string[];
  /** Precomputed `${objectName}\0${className}\0${interestName}`.toLowerCase() suffix. */
  lowerSuffix: string;
}

interface CollectorEntry {
  // Releasers capture their issued generation and no-op if rotated, so a stale releaser
  // can't decrement a new cohort's count.
  generation: number;
  count: number;
  cancel: () => void;
  obj: ObjectHandle;
  /** Wire-subscribed event names; joining acquirers reconcile against this so a late
   *  sibling asking for a new event still gets it installed. */
  boundEventNames: Set<string>;
  // Latest acquirer's spec; per-delivery builders read display metadata here so joins
  // and reconnects don't bake the first acquirer's stale strings into every delivery.
  spec: EventCollectorSpec;
  cancels: Array<() => void>;
}

const collectors = new Map<string, CollectorEntry>();

function makeReleaser(objectKey: string, generation: number): () => void {
  return () => {
    const e = collectors.get(objectKey);
    // Generation rotated; the swap already reset count.
    if (!e || e.generation !== generation) return;
    e.count--;
    if (e.count === 0) {
      e.cancel();
      collectors.delete(objectKey);
    }
  };
}

/** Refcounted by objectKey; generation tokens guard against handle-swap races. */
export function acquireCollector(objectKey: string, spec: EventCollectorSpec): () => void {
  const existing = collectors.get(objectKey);
  if (existing && existing.obj === spec.obj) {
    existing.count++;
    existing.spec = spec;
    installEventCallbacks(objectKey, existing, spec);
    return makeReleaser(objectKey, existing.generation);
  }
  if (existing) {
    // Handle swapped (reconnect / re-resolve); tear down old callbacks.
    existing.cancel();
  }
  // cancel() flips this synchronously so in-flight callbacks on the old handle early-return.
  const cohort = { alive: true };
  const cancels: Array<() => void> = [];
  const boundEventNames = new Set<string>();
  const generation = ++nextGeneration;
  const entry: CollectorEntry = {
    generation,
    count: 1,
    cancel: () => {
      cohort.alive = false;
      for (const c of cancels) c();
    },
    obj: spec.obj,
    boundEventNames,
    spec,
    cancels,
  };
  collectors.set(objectKey, entry);
  installEventCallbacks(objectKey, entry, spec, cohort);
  return makeReleaser(objectKey, generation);
}

function installEventCallbacks(
  objectKey: string,
  entry: CollectorEntry,
  spec: EventCollectorSpec,
  cohort?: { alive: boolean },
): void {
  // Joining closures don't need the cohort flag: cancel unhooks the wire callback before
  // any new closure can fire.
  const liveness = cohort ?? { alive: true };
  for (const name of spec.eventNames) {
    if (entry.boundEventNames.has(name)) continue;
    entry.boundEventNames.add(name);
    const lowerName = name.toLowerCase();
    entry.cancels.push(
      spec.obj.onEventTriggered(name, (args, info) => {
        if (!liveness.alive) return;
        const current = entry.spec;
        const event: EventDelivery = {
          seq: ++seq,
          interestName: current.interestName,
          sessionName: current.sessionName,
          busName: current.busName,
          objectName: current.objectName,
          className: current.className,
          eventName: name,
          args,
          timestamp: info.timestamp,
          at: Date.now(),
          lowerSearch: `${lowerName}\0${current.lowerSuffix}`,
        };
        appendEvent(objectKey, event);
      }),
    );
  }
}

function appendEvent(objectKey: string, event: EventDelivery): void {
  let buf = eventsByObjectBacking.get(objectKey);
  if (!buf) {
    buf = [];
    eventsByObjectBacking.set(objectKey, buf);
  }
  buf.push(event);
  if (buf.length >= HARD_EVENT_CAP + DROP_CHUNK) {
    dropFront(buf, buf.length - HARD_EVENT_CAP);
  }
  eventsByObjectStore.invalidate(objectKey);

  allEventsBacking.push(event);
  if (allEventsBacking.length >= ALL_EVENTS_CAP + DROP_CHUNK) {
    dropFront(allEventsBacking, allEventsBacking.length - ALL_EVENTS_CAP);
  }
  allEventsCell.invalidate();
}

function trimOnce(): void {
  const cutoff = Date.now() - retentionSecondsRef * 1000;
  let globalTouched = false;
  for (const [key, buf] of eventsByObjectBacking) {
    let drop = 0;
    while (drop < buf.length && buf[drop]!.at < cutoff) drop++;
    if (drop > 0) {
      dropFront(buf, drop);
      eventsByObjectStore.invalidate(key);
    }
    if (buf.length === 0) {
      eventsByObjectBacking.delete(key);
      eventsByObjectStore.dropKey(key);
    }
  }
  let allDrop = 0;
  while (allDrop < allEventsBacking.length && allEventsBacking[allDrop]!.at < cutoff) allDrop++;
  if (allDrop > 0) {
    dropFront(allEventsBacking, allDrop);
    globalTouched = true;
  }
  if (globalTouched) allEventsCell.invalidate();
}

let trimIntervalId: ReturnType<typeof setInterval> | null = null;
let trimMountCount = 0;

function startTrim(): () => void {
  trimMountCount++;
  if (trimIntervalId === null) {
    trimIntervalId = setInterval(trimOnce, TRIM_INTERVAL_MS);
  }
  return () => {
    trimMountCount--;
    if (trimMountCount <= 0 && trimIntervalId !== null) {
      clearInterval(trimIntervalId);
      trimIntervalId = null;
      trimMountCount = 0;
    }
  };
}

export function setEventRetentionSeconds(s: number): void {
  retentionSecondsRef = s;
}

export function EventStoreProvider({
  retentionSeconds = RETENTION_SECONDS_DEFAULT,
  children,
}: {
  retentionSeconds?: number;
  children: ReactNode;
}) {
  useEffect(() => {
    setEventRetentionSeconds(retentionSeconds);
  }, [retentionSeconds]);
  useEffect(() => startTrim(), []);
  return <>{children}</>;
}

export function useObjectEvents(
  interestName: string | null,
  objectName: string | null,
): EventLogView {
  const key = interestName && objectName ? objectEventKey(interestName, objectName) : null;
  return useView(eventsByObjectStore, key) ?? EMPTY_EVENT_LOG;
}

export function useAllEvents(): EventLogView {
  return useCell(allEventsCell);
}

/** @internal */
export function __resetEventStoreForTests(): void {
  for (const e of collectors.values()) e.cancel();
  collectors.clear();
  eventsByObjectBacking.clear();
  allEventsBacking.length = 0;
  seq = 0;
  nextGeneration = 0;
  retentionSecondsRef = RETENTION_SECONDS_DEFAULT;
}

/** @internal */
export function __appendEventForTests(event: EventDelivery): void {
  appendEvent(objectEventKey(event.interestName, event.objectName), event);
}

/** @internal Live merged log (mutable; slice for a snapshot). */
export function __getAllEventsForTests(): readonly EventDelivery[] {
  return allEventsBacking;
}

/** @internal Live per-object buffer (mutable; slice for a snapshot). */
export function __getObjectEventsForTests(
  interestName: string,
  objectName: string,
): readonly EventDelivery[] {
  return eventsByObjectBacking.get(objectEventKey(interestName, objectName)) ?? EMPTY_EVENTS;
}

/** @internal */
export function __trimOnceForTests(): void {
  trimOnce();
}

// HMR: the prior module's trim timer would otherwise outlive its backing maps.
if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    if (trimIntervalId !== null) {
      clearInterval(trimIntervalId);
      trimIntervalId = null;
    }
    for (const e of collectors.values()) e.cancel();
    collectors.clear();
    eventsByObjectBacking.clear();
    allEventsBacking.length = 0;
    eventsByObjectStore.dispose();
    allEventsCell.dispose();
  });
}
