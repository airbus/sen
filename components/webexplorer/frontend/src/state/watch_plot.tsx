// === watch_plot.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

import type { PanelKind } from "../core/panels.js";
import {
  makePlotKeyFromSource,
  makePropertyKey,
  makeWatchKey,
  type PlotKey,
  type PropertyKey,
  type WatchKey,
  type WatchSource,
} from "../core/watch_keys.js";
import {
  addNewPanel,
  addSeriesToPanel as addSeriesToPanelHelper,
  findSeriesPanel,
  plottedLeavesOf,
  removeSeries as removeSeriesHelper,
  type PlotBoardPanel,
  type PlotSeries,
} from "./plot_board.js";

// Watch list and plot board move together: removing a watch prunes orphan series;
// togglePlottedLeaf adds the minimal watch needed to keep the leaf subscribed.
// Derived sets answer per-leaf selectors in O(1).

export interface WatchPlotState {
  readonly watchSources: readonly WatchSource[];
  readonly watchedKeys: ReadonlySet<WatchKey>;
  /** Covered properties (any leaf), for O(1) useHasAnyWatchOnProperty. */
  readonly propertyKeys: ReadonlySet<PropertyKey>;
  /** Auto-elected by togglePlottedLeaf; scrubbed on un-plot, promoted to user-elected
   *  by any explicit watch action. Session-only. */
  readonly autoWatchedKeys: ReadonlySet<WatchKey>;
  readonly panels: readonly PlotBoardPanel[];
  readonly plottedLeaves: ReadonlySet<PlotKey>;
}

function emptyState(): WatchPlotState {
  return {
    watchSources: [],
    watchedKeys: new Set(),
    propertyKeys: new Set(),
    autoWatchedKeys: new Set(),
    panels: [],
    plottedLeaves: new Set(),
  };
}

function deriveWatchSources(
  prev: WatchPlotState,
  watchSources: readonly WatchSource[],
): WatchPlotState {
  const watchedKeys = new Set<WatchKey>(watchSources.map(makeWatchKey));
  const propertyKeys = new Set<PropertyKey>(watchSources.map(makePropertyKey));
  return { ...prev, watchSources, watchedKeys, propertyKeys };
}

function derivePanels(
  prev: WatchPlotState,
  panels: readonly PlotBoardPanel[],
): WatchPlotState {
  // Same-ref short-circuit: addSeriesToPanel's no-op branch returns the same array.
  if (panels === prev.panels) return prev;
  return { ...prev, panels, plottedLeaves: plottedLeavesOf(panels) };
}

const WATCH_PLOT_STORAGE_KEY = "webex.watchPlot";
const WATCH_PLOT_STORAGE_VERSION = 1;

interface PersistedWatchPlot {
  version: number;
  watchSources: readonly WatchSource[];
  panels: readonly PlotBoardPanel[];
}

function isValidPersisted(raw: unknown): raw is PersistedWatchPlot {
  if (!raw || typeof raw !== "object") return false;
  const r = raw as Partial<PersistedWatchPlot>;
  return (
    r.version === WATCH_PLOT_STORAGE_VERSION &&
    Array.isArray(r.watchSources) &&
    Array.isArray(r.panels)
  );
}

function isValidWatchSource(raw: unknown): raw is WatchSource {
  if (!raw || typeof raw !== "object") return false;
  const r = raw as Partial<WatchSource>;
  return (
    typeof r.interestName === "string" &&
    typeof r.objectName === "string" &&
    typeof r.propertyName === "string" &&
    typeof r.className === "string" &&
    typeof r.declaredType === "string" &&
    typeof r.sessionName === "string" &&
    typeof r.busName === "string" &&
    (r.leafPath === undefined || typeof r.leafPath === "string")
  );
}

function isValidPlotSeries(raw: unknown): raw is PlotSeries {
  if (!raw || typeof raw !== "object") return false;
  const r = raw as Partial<PlotSeries>;
  return (
    typeof r.sourceKey === "string" &&
    typeof r.leafPath === "string" &&
    typeof r.color === "string" &&
    typeof r.objectName === "string" &&
    typeof r.propertyName === "string"
  );
}

function isValidPlotBoardPanel(raw: unknown): raw is PlotBoardPanel {
  if (!raw || typeof raw !== "object") return false;
  const r = raw as Partial<PlotBoardPanel>;
  return (
    typeof r.id === "string" &&
    (r.kind === "numeric" || r.kind === "discrete") &&
    Array.isArray(r.series) &&
    r.series.every(isValidPlotSeries)
  );
}

function readPersistedWatchPlot(): WatchPlotState {
  try {
    const raw = localStorage.getItem(WATCH_PLOT_STORAGE_KEY);
    if (!raw) return emptyState();
    const parsed = JSON.parse(raw) as unknown;
    if (!isValidPersisted(parsed)) return emptyState();
    // A corrupt entry doesn't poison the whole persisted state.
    const validSources = parsed.watchSources.filter(isValidWatchSource);
    const validPanels = parsed.panels.filter(isValidPlotBoardPanel);
    const base = emptyState();
    const withSources = deriveWatchSources(base, validSources);
    return derivePanels(withSources, validPanels);
  } catch {
    return emptyState();
  }
}

function writePersistedWatchPlot(state: WatchPlotState): void {
  try {
    const payload: PersistedWatchPlot = {
      version: WATCH_PLOT_STORAGE_VERSION,
      watchSources: state.watchSources,
      panels: state.panels,
    };
    localStorage.setItem(WATCH_PLOT_STORAGE_KEY, JSON.stringify(payload));
  } catch {
  }
}

const watchStore = makeStore<WatchPlotState>(readPersistedWatchPlot());

// Source/panel identity flips proxy "user action"; derived sets move in lockstep.
let lastPersistedSources = watchStore.getState().watchSources;
let lastPersistedPanels = watchStore.getState().panels;
watchStore.subscribe(() => {
  const s = watchStore.getState();
  if (s.watchSources === lastPersistedSources && s.panels === lastPersistedPanels) return;
  lastPersistedSources = s.watchSources;
  lastPersistedPanels = s.panels;
  writePersistedWatchPlot(s);
});

function addWatch(source: WatchSource): void {
  watchStore.setState((prev) => {
    const key = makeWatchKey(source);
    if (prev.watchedKeys.has(key)) {
      // Upgrade auto-watched to user-elected.
      if (prev.autoWatchedKeys.has(key)) {
        const nextAuto = new Set(prev.autoWatchedKeys);
        nextAuto.delete(key);
        return { ...prev, autoWatchedKeys: nextAuto };
      }
      return prev;
    }
    return deriveWatchSources(prev, [...prev.watchSources, source]);
  });
}

function removeWatch(key: WatchKey): void {
  // Identity-equality short-circuit for unknown keys.
  if (!watchStore.getState().watchedKeys.has(key)) return;
  batchEdit([key], []);
}

// One setState for collapse/expand flows (whole property <-> N leaves) avoids N+1
// fanouts to every per-leaf useIsWatched selector.
function batchEdit(remove: readonly WatchKey[], add: readonly WatchSource[]): void {
  if (remove.length === 0 && add.length === 0) return;
  watchStore.setState((prev) => {
    const removeSet = new Set(remove);
    const droppedPropKeys = new Set<PropertyKey>();
    const finalSources: WatchSource[] = [];
    const finalKeys = new Set<WatchKey>();
    const finalPropKeys = new Set<PropertyKey>();
    for (const s of prev.watchSources) {
      const key = makeWatchKey(s);
      const propKey = makePropertyKey(s);
      if (removeSet.has(key)) {
        droppedPropKeys.add(propKey);
      } else {
        finalSources.push(s);
        finalKeys.add(key);
        finalPropKeys.add(propKey);
      }
    }
    for (const s of add) {
      const key = makeWatchKey(s);
      if (finalKeys.has(key)) continue;
      finalSources.push(s);
      finalKeys.add(key);
      finalPropKeys.add(makePropertyKey(s));
    }
    // Explicit add/remove drops the key from auto-set (upgrade on add; clean slate on remove).
    let nextAuto = prev.autoWatchedKeys;
    if (prev.autoWatchedKeys.size > 0) {
      let touchedAuto = false;
      const candidate = new Set(prev.autoWatchedKeys);
      for (const k of removeSet) {
        if (candidate.delete(k)) touchedAuto = true;
      }
      for (const s of add) {
        if (candidate.delete(makeWatchKey(s))) touchedAuto = true;
      }
      if (touchedAuto) nextAuto = candidate;
    }
    let next: WatchPlotState = {
      ...prev,
      watchSources: finalSources,
      watchedKeys: finalKeys,
      propertyKeys: finalPropKeys,
      autoWatchedKeys: nextAuto,
    };
    if (droppedPropKeys.size > 0) {
      // Orphan = dropped propKey not covered by any surviving source.
      const orphanedProps = new Set<PropertyKey>();
      for (const propKey of droppedPropKeys) {
        if (!finalPropKeys.has(propKey)) orphanedProps.add(propKey);
      }
      if (orphanedProps.size > 0) {
        const prunedPanels = next.panels
          .map((p) => ({ ...p, series: p.series.filter((s) => !orphanedProps.has(s.sourceKey)) }))
          .filter((p) => p.series.length > 0);
        next = derivePanels(next, prunedPanels);
      }
    }
    return next;
  });
}

function togglePlottedLeaf(
  source: WatchSource,
  leafPath: string,
  kind: PanelKind = "numeric",
): void {
  watchStore.setState((prev) => {
    const propKey = makePropertyKey(source);
    const idx = findSeriesPanel(prev.panels, propKey, leafPath);
    if (idx >= 0) {
      // Un-plot: drop the series, then scrub an auto-watched leaf if no plot covers it,
      // or the Watches surface fills with leaves the user never explicitly asked for.
      const nextPanels = removeSeriesHelper(prev.panels, propKey, leafPath);
      let next = derivePanels(prev, nextPanels);
      const leafKey = makeWatchKey({ ...source, leafPath });
      const stillPlotted = next.plottedLeaves.has(
        makePlotKeyFromSource(source, leafPath),
      );
      const wholeWatched = next.watchSources.some(
        (s) => makePropertyKey(s) === propKey && (s.leafPath ?? "") === "",
      );
      if (!stillPlotted && !wholeWatched && next.autoWatchedKeys.has(leafKey)) {
        const sourceIdx = next.watchSources.findIndex(
          (s) => makeWatchKey(s) === leafKey,
        );
        if (sourceIdx >= 0) {
          const trimmedSources = next.watchSources.filter((_, i) => i !== sourceIdx);
          next = deriveWatchSources(next, trimmedSources);
          const nextAuto = new Set(next.autoWatchedKeys);
          nextAuto.delete(leafKey);
          next = { ...next, autoWatchedKeys: nextAuto };
        }
      }
      return next;
    }
    // Plot implies watch (of just the plotted leaf), unless the whole property is
    // already watched (covers transitively).
    let next = derivePanels(prev, addNewPanel(prev.panels, source, leafPath, kind));
    const wholeWatched = next.watchSources.some(
      (s) => makePropertyKey(s) === propKey && (s.leafPath ?? "") === "",
    );
    if (!wholeWatched) {
      const leafSource: WatchSource = { ...source, leafPath };
      const leafKey = makeWatchKey(leafSource);
      if (!next.watchedKeys.has(leafKey)) {
        next = deriveWatchSources(next, [...next.watchSources, leafSource]);
        // Track so the symmetric un-plot can scrub the orphan.
        const nextAuto = new Set(next.autoWatchedKeys);
        nextAuto.add(leafKey);
        next = { ...next, autoWatchedKeys: nextAuto };
      }
    }
    return next;
  });
}

function removeSeries(sourceKey: PropertyKey, leafPath: string): void {
  watchStore.setState((prev) =>
    derivePanels(prev, removeSeriesHelper(prev.panels, sourceKey, leafPath)),
  );
}

// Caller refuses drops that cross kinds; missing source is a no-op.
function moveSeriesToPanel(panelId: string, sourceKey: PropertyKey, leafPath: string): void {
  watchStore.setState((prev) => {
    const source = prev.watchSources.find((s) => makePropertyKey(s) === sourceKey);
    if (!source) return prev;
    return derivePanels(prev, addSeriesToPanelHelper(prev.panels, panelId, source, leafPath));
  });
}

// Un-merge a series into its own panel, inheriting the kind of the current owner.
function moveSeriesToNewPanel(sourceKey: PropertyKey, leafPath: string): void {
  watchStore.setState((prev) => {
    const source = prev.watchSources.find((s) => makePropertyKey(s) === sourceKey);
    if (!source) return prev;
    const idx = findSeriesPanel(prev.panels, sourceKey, leafPath);
    const kind: PanelKind = idx >= 0 ? prev.panels[idx]!.kind : "numeric";
    const cleaned =
      idx >= 0 ? removeSeriesHelper(prev.panels, sourceKey, leafPath) : prev.panels;
    return derivePanels(prev, addNewPanel(cleaned, source, leafPath, kind));
  });
}

function clearAllWatches(): void {
  watchStore.setState((prev) => {
    if (prev.watchSources.length === 0) return prev;
    // Wipe panels too, or Clear-all leaves orphan empty panels.
    const cleared = derivePanels(deriveWatchSources(prev, []), []);
    return prev.autoWatchedKeys.size === 0
      ? cleared
      : { ...cleared, autoWatchedKeys: new Set<WatchKey>() };
  });
}

// Doesn't touch watches: a user can watch without plotting.
function clearAllPanels(): void {
  watchStore.setState((prev) => (prev.panels.length === 0 ? prev : derivePanels(prev, [])));
}

/** Stable identities for the lifetime of the page. */
export const watchActions = Object.freeze({
  addWatch,
  removeWatch,
  batchEdit,
  clearAllWatches,
  clearAllPanels,
  togglePlottedLeaf,
  removeSeries,
  moveSeriesToPanel,
  moveSeriesToNewPanel,
});

export function useIsWatched(key: WatchKey): boolean {
  return useSelector(watchStore, (s) => s.watchedKeys.has(key));
}

/** `null` accepted (non-plottable leaves) so callers keep stable hook order. */
export function useIsPlotted(key: PlotKey | null): boolean {
  return useSelector(watchStore, (s) => key !== null && s.plottedLeaves.has(key));
}

export function useHasAnyWatchOnProperty(propertyKey: PropertyKey): boolean {
  return useSelector(watchStore, (s) => s.propertyKeys.has(propertyKey));
}

/** Synchronous snapshot for click handlers at toggle time. */
export function getWatchedKeysForProperty(propertyKey: PropertyKey): WatchKey[] {
  const sources = watchStore.getState().watchSources;
  const out: WatchKey[] = [];
  for (const s of sources) {
    if (makePropertyKey(s) === propertyKey) out.push(makeWatchKey(s));
  }
  return out;
}

export function useWatchSources(): readonly WatchSource[] {
  return useSelector(watchStore, (s) => s.watchSources);
}

export function useWatchedKeys(): ReadonlySet<WatchKey> {
  return useSelector(watchStore, (s) => s.watchedKeys);
}

export function usePanels(): readonly PlotBoardPanel[] {
  return useSelector(watchStore, (s) => s.panels);
}

export function usePlottedLeaves(): ReadonlySet<PlotKey> {
  return useSelector(watchStore, (s) => s.plottedLeaves);
}

/** @internal */
export function __getWatchPlotStateForTests(): WatchPlotState {
  return watchStore.getState();
}

/** @internal */
export function __resetWatchPlotForTests(): void {
  watchStore.setState(emptyState());
}

// HMR: dispose so dev iterations don't accumulate orphan listeners.
if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    watchStore.dispose();
  });
}
