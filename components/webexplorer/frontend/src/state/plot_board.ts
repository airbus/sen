// === plot_board.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { PanelKind } from "../core/panels.js";
import { makePlotKey, makePropertyKey, type PlotKey, type PropertyKey, type WatchSource } from "../core/watch_keys.js";

export type { PanelKind };

// crypto.randomUUID so a clock rewind (NTP, VM resume) can't mint a panel id that
// collides with one already in a saved layout.
function mintPanelId(): string {
  return `panel-${crypto.randomUUID()}`;
}

export interface PlotSeries {
  /** Property-identity key; plotting a leaf doesn't care whether the user watches the
   *  whole property or just that field. */
  sourceKey: PropertyKey;
  /** `""` for a scalar property. */
  leafPath: string;
  color: string;
  /** Cached so the legend doesn't round-trip through the registry. */
  objectName: string;
  propertyName: string;
}

export interface PlotBoardPanel {
  id: string;
  kind: PanelKind;
  series: PlotSeries[];
}

/** First color matches the UI accent. */
export const SERIES_PALETTE = [
  "#4f93ff",
  "#46c08a",
  "#d6a85e",
  "#9a8cff",
  "#56c7d6",
  "#e58fb0",
  "#e5675f",
  "#b8c87a",
] as const;

export function pickNextColor(panels: readonly PlotBoardPanel[]): string {
  const used = new Set<string>();
  for (const p of panels) for (const s of p.series) used.add(s.color);
  for (const c of SERIES_PALETTE) if (!used.has(c)) return c;
  return SERIES_PALETTE[panels.length % SERIES_PALETTE.length]!;
}

export function seriesLeafKey(s: { sourceKey: PropertyKey; leafPath: string }): PlotKey {
  return makePlotKey(s.sourceKey, s.leafPath);
}

export function addNewPanel(
  panels: readonly PlotBoardPanel[],
  source: WatchSource,
  leafPath: string,
  kind: PanelKind,
): PlotBoardPanel[] {
  const series: PlotSeries = {
    sourceKey: makePropertyKey(source),
    leafPath,
    color: pickNextColor(panels),
    objectName: source.objectName,
    propertyName: source.propertyName,
  };
  return [
    ...panels,
    {
      id: mintPanelId(),
      kind,
      series: [series],
    },
  ];
}

// Move-semantics: the series is removed from any OTHER panel before being added here,
// so a leaf only ever lives in one panel.
export function addSeriesToPanel(
  panels: readonly PlotBoardPanel[],
  panelId: string,
  source: WatchSource,
  leafPath: string,
): readonly PlotBoardPanel[] {
  const sourceKey = makePropertyKey(source);
  const alreadyThere = panels.some(
    (p) =>
      p.id === panelId &&
      p.series.some((s) => s.sourceKey === sourceKey && s.leafPath === leafPath),
  );
  // Same reference so derivePanels short-circuits; a .slice() would refire every
  // per-leaf useIsPlotted selector for a no-op.
  if (alreadyThere) return panels;
  // Defensive: if the target panel id no longer exists (closed mid-drag, stale id),
  // removeSeries below would still remove from the source panel and the leaf would
  // silently vanish. No-op instead of data loss.
  if (!panels.some((p) => p.id === panelId)) return panels;
  const cleaned = removeSeries(panels, sourceKey, leafPath);
  const series: PlotSeries = {
    sourceKey,
    leafPath,
    color: pickNextColor(cleaned),
    objectName: source.objectName,
    propertyName: source.propertyName,
  };
  return cleaned.map((p) =>
    p.id === panelId ? { ...p, series: [...p.series, series] } : p,
  );
}

/** Returns -1 when no panel holds the series. */
export function findSeriesPanel(
  panels: readonly PlotBoardPanel[],
  sourceKey: PropertyKey,
  leafPath: string,
): number {
  return panels.findIndex((p) =>
    p.series.some((s) => s.sourceKey === sourceKey && s.leafPath === leafPath),
  );
}

/** Drops the panel if its last series leaves. */
export function removeSeries(
  panels: readonly PlotBoardPanel[],
  sourceKey: PropertyKey,
  leafPath: string,
): PlotBoardPanel[] {
  return panels
    .map((p) => ({
      ...p,
      series: p.series.filter((s) => !(s.sourceKey === sourceKey && s.leafPath === leafPath)),
    }))
    .filter((p) => p.series.length > 0);
}

export function plottedLeavesOf(panels: readonly PlotBoardPanel[]): Set<PlotKey> {
  const set = new Set<PlotKey>();
  for (const p of panels) for (const s of p.series) set.add(seriesLeafKey(s));
  return set;
}
