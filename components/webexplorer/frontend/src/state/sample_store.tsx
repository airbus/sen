// === sample_store.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, type ReactNode } from "react";

import { makeBufferStore, useViews } from "@sen/client/react";

import { EMPTY_VIEW, type BufferView, type SampleLeaf } from "./leaf_samples.js";
import {
  makeBuffer,
  pushSentinelToBuffer,
  pushToBuffer,
  trimBuffer,
  viewOfBuffer,
  type LeafBuffer,
} from "./sample_buffer.js";
import {
  makePlotKey,
  splitPlotKey,
  type PlotKey,
  type PropertyKey,
} from "../core/watch_keys.js";

// Time-windowed per-leaf sample store. Page-lifetime so history survives tab swaps.

const RETENTION_SECONDS_DEFAULT = 60;
const TRIM_INTERVAL_MS = 1000;
/** Safety cap for very-high-frequency series; retention is the binding constraint at typical rates. */
export const HARD_SAMPLE_CAP = 4000;

// Nested `sourceKey -> leafPath -> LeafBuffer` so trim can iterate per property.
const samplesBacking = new Map<string, Map<string, LeafBuffer>>();


let retentionSeconds = RETENTION_SECONDS_DEFAULT;

const sampleStore = makeBufferStore<PlotKey, BufferView>({
  produce: (key) => {
    const { propertyKey, leafPath } = splitPlotKey(key);
    const buf = samplesBacking.get(propertyKey)?.get(leafPath);
    return buf ? viewOfBuffer(buf) : EMPTY_VIEW;
  },
});

export function reportDelivery(
  sourceKey: PropertyKey,
  leaves: ReadonlyArray<SampleLeaf>,
  at: number,
  tServer: number | null,
): void {
  let perSource = samplesBacking.get(sourceKey);
  if (!perSource) {
    perSource = new Map();
    samplesBacking.set(sourceKey, perSource);
  }
  for (let i = 0; i < leaves.length; i++) {
    const leaf = leaves[i]!;
    const path = leaf[0];
    let buf = perSource.get(path);
    if (!buf) {
      buf = makeBuffer(HARD_SAMPLE_CAP, makePlotKey(sourceKey, path));
      perSource.set(path, buf);
    }
    pushToBuffer(buf, at, tServer, leaf[1], leaf[2]);
    sampleStore.invalidate(buf.plotKey);
  }
}

// Pass tServer of the absence-revealing delivery so the gap plots on the same clock
// as surrounding samples. Idempotent across calls with no intervening samples.
export function markDeadLeaf(
  sourceKey: PropertyKey,
  leafPath: string,
  at: number,
  tServer: number | null,
): void {
  const buf = samplesBacking.get(sourceKey)?.get(leafPath);
  if (!buf) return;
  if (pushSentinelToBuffer(buf, at, tServer)) sampleStore.invalidate(buf.plotKey);
}

/** Collector-cleanup path; sentinel uses local clock only (no delivery in flight). */
export function markDeadProperty(sourceKey: PropertyKey, at: number): void {
  const perSource = samplesBacking.get(sourceKey);
  if (!perSource) return;
  for (const buf of perSource.values()) {
    if (pushSentinelToBuffer(buf, at, null)) sampleStore.invalidate(buf.plotKey);
  }
}

function trimOnce(): void {
  const now = Date.now();
  const cutoff = now - retentionSeconds * 1000;
  // Drop wholesale after 2x retention with no pushes; trimBuffer alone keeps one sample.
  const evictCutoff = now - retentionSeconds * 2 * 1000;
  for (const [sourceKey, perSource] of samplesBacking) {
    for (const [path, buf] of perSource) {
      if (buf.lastPushAt < evictCutoff) {
        perSource.delete(path);
        sampleStore.dropKey(buf.plotKey);
        continue;
      }
      if (trimBuffer(buf, cutoff)) sampleStore.invalidate(buf.plotKey);
    }
    if (perSource.size === 0) samplesBacking.delete(sourceKey);
  }
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

export function setSampleRetentionSeconds(s: number): void {
  retentionSeconds = s;
}

/** Owns the trim-interval lifecycle. */
export function SampleStoreProvider({
  retentionSeconds: retention = RETENTION_SECONDS_DEFAULT,
  children,
}: {
  retentionSeconds?: number;
  children: ReactNode;
}) {
  useEffect(() => {
    setSampleRetentionSeconds(retention);
  }, [retention]);
  useEffect(() => startTrim(), []);
  return <>{children}</>;
}

// Each returned BufferView wraps live typed-array columns; reads per render or in draw
// hooks see current state. Stashing a view to read later returns DIFFERENT data (the
// arrays mutate in place). For a snapshot, slice columns eagerly.
export function useMultipleLeafHistories(
  series: ReadonlyArray<{ sourceKey: PropertyKey; leafPath: string }>,
): ReadonlyMap<PlotKey, BufferView> {
  const keys = useMemo(
    () => series.map((s) => makePlotKey(s.sourceKey, s.leafPath)),
    [series],
  );
  return useViews(sampleStore, keys);
}

/** @internal */
export function __resetSampleStoreForTests(): void {
  samplesBacking.clear();
  retentionSeconds = RETENTION_SECONDS_DEFAULT;
}

/** @internal */
export function __reportDeliveryForTests(
  sourceKey: string,
  leaves: ReadonlyArray<SampleLeaf>,
  at: number,
  tServer: number | null,
): void {
  reportDelivery(sourceKey as PropertyKey, leaves, at, tServer);
}

/** @internal Bypasses the buffer-store cache; tests see fresh sizes without flushing rAF batches. */
export function __getSampleViewForTests(sourceKey: string, leafPath: string): BufferView {
  const buf = samplesBacking.get(sourceKey)?.get(leafPath);
  return buf ? viewOfBuffer(buf) : EMPTY_VIEW;
}

/** @internal */
export function __trimOnceForTests(): void {
  trimOnce();
}

/** @internal */
export function __getLeafCountForTests(): number {
  let n = 0;
  for (const perSource of samplesBacking.values()) n += perSource.size;
  return n;
}

// HMR: trim interval would otherwise outlive its backing maps.
if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    if (trimIntervalId !== null) {
      clearInterval(trimIntervalId);
      trimIntervalId = null;
    }
    samplesBacking.clear();
    sampleStore.dispose();
  });
}
