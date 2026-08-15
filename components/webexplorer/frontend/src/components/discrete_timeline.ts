// === discrete_timeline.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// uPlot has no native "state timeline"; we paint per-run filled rectangles inside its
// `draw` hook against a per-series state map.

import type uPlot from "uplot";

import { EMPTY_VIEW, timeMsAt, type BufferView } from "../state/leaf_samples.js";
import { seriesLeafKey, type PlotSeries } from "../state/plot_board.js";

// Color is a stable hash(VALUE) -> palette index so a state keeps its color even after
// buffer trims drop the samples that first introduced it. Booleans get dedicated colors
// outside the cycle so true/false are consistent across panels.
const DISCRETE_PALETTE = [
  "#5b8db1", // steel blue
  "#c97d4a", // burnt amber
  "#8aae6e", // sage green
  "#b56d6d", // terracotta
  "#9d7fc4", // lavender
  "#3fa39a", // teal (deeper than the boolean teal so they're distinct)
  "#d4b14a", // mustard
  "#c98aac", // plum-rose
  "#7d8da0", // slate gray
  "#5fa468", // forest green
  "#d97c52", // rust
  "#4f74b3", // indigo
];

const BOOL_TRUE_COLOR = "#6db4ac"; // teal - "on" / active
const BOOL_FALSE_COLOR = "#6b727e"; // slate - neutral "off"

// FNV-1a: better avalanche than djb2 on short tokens so similar enum names don't collide.
function hashString(s: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 0x01000193);
  }
  return h >>> 0;
}

export function stateColorFor(v: boolean | string): string {
  if (typeof v === "boolean") return v ? BOOL_TRUE_COLOR : BOOL_FALSE_COLOR;
  return DISCRETE_PALETTE[hashString(v) % DISCRETE_PALETTE.length]!;
}

export function stateLabel(v: boolean | string): string {
  return typeof v === "boolean" ? (v ? "true" : "false") : v;
}

// Runs come from the raw source samples (not the y-projected uPlot data), so we have the
// original boolean|string values. Open run is extended +1 day past the last sample so the
// band always reaches the live edge regardless of the visible window's right limit.
export function drawDiscreteTimeline(
  u: uPlot,
  series: PlotSeries[],
  samplesByLeaf: ReadonlyMap<string, BufferView>,
  discreteStateMaps: Map<string | boolean, number>[],
): void {
  const ctx = u.ctx;
  const left = u.bbox.left;
  const top = u.bbox.top;
  const width = u.bbox.width;
  const height = u.bbox.height;
  // uPlot's canvas is device-pixel sized; multiply CSS values by dpr in custom draw hooks
  // (uPlot does this internally for axes, not here).
  const dpr = window.devicePixelRatio || 1;

  ctx.save();
  ctx.beginPath();
  ctx.rect(left, top, width, height);
  ctx.clip();

  const rowPad = 4 * dpr;

  for (let sIdx = 0; sIdx < series.length; sIdx++) {
    const s = series[sIdx]!;
    const view = samplesByLeaf.get(seriesLeafKey(s)) ?? EMPTY_VIEW;
    const stateMap = discreteStateMaps[sIdx];
    if (!stateMap || view.size === 0) continue;
    const readDiscrete: (i: number) => boolean | string | undefined =
      view.vKind === "boolean"
        ? (i) => view.vBool![i] === 1
        : view.vKind === "string"
          ? (i) => view.vStr![i]
          : () => undefined;

    const yTop = u.valToPos(sIdx + 1, "y", true);
    const yBot = u.valToPos(sIdx, "y", true);
    const bandTop = Math.min(yTop, yBot) + rowPad;
    const bandBot = Math.max(yTop, yBot) - rowPad;
    const bandH = Math.max(0, bandBot - bandTop);
    if (bandH <= 0) continue;

    // Gap sentinels close the open run and leave a blank slot until the next real sample.
    type Run = { start: number; end: number; v: boolean | string };
    const runs: Run[] = [];
    const vGap = view.vGap;
    let openStart: number | null = null;
    let openValue: boolean | string | null = null;
    for (let i = 0; i < view.size; i++) {
      const t = timeMsAt(view, i) / 1000;
      if (vGap !== null && vGap[i] === 1) {
        if (openStart !== null) {
          runs.push({ start: openStart, end: t, v: openValue! });
          openStart = null;
          openValue = null;
        }
        continue;
      }
      const v = readDiscrete(i);
      if (v === undefined) continue;
      if (openStart === null) {
        openStart = t;
        openValue = v;
      } else if (v !== openValue) {
        runs.push({ start: openStart, end: t, v: openValue! });
        openStart = t;
        openValue = v;
      }
    }
    if (openStart !== null) {
      const lastT = timeMsAt(view, view.size - 1) / 1000;
      runs.push({ start: openStart, end: lastT + 86400, v: openValue! });
    }

    ctx.font = `${12 * dpr}px ui-monospace, monospace`;
    ctx.textBaseline = "middle";
    ctx.textAlign = "left";

    // Cache gradients per base color; gradient Y range is the band, so all same-color runs
    // share one fill. ~60% alpha at top -> ~20% at bottom.
    const gradientByColor = new Map<string, CanvasGradient>();
    const fillForColor = (baseColor: string): CanvasGradient => {
      const cached = gradientByColor.get(baseColor);
      if (cached) return cached;
      const g = ctx.createLinearGradient(0, bandTop, 0, bandBot);
      g.addColorStop(0, baseColor + "99");
      g.addColorStop(1, baseColor + "33");
      gradientByColor.set(baseColor, g);
      return g;
    };

    for (const run of runs) {
      const x0 = u.valToPos(run.start, "x", true);
      const x1 = u.valToPos(run.end, "x", true);
      if (x1 < left || x0 > left + width) continue;
      const drawX0 = Math.max(left, x0);
      const drawX1 = Math.min(left + width, x1);
      const drawW = drawX1 - drawX0;
      if (drawW <= 0) continue;

      // Color is a function of the value; stateMap only drives Y positioning.
      ctx.fillStyle = fillForColor(stateColorFor(run.v));
      ctx.fillRect(drawX0, bandTop, drawW, bandH);

      // Stroke-then-fill faux text-shadow for contrast against any palette color.
      const label = stateLabel(run.v);
      const textWidth = ctx.measureText(label).width;
      if (drawW > textWidth + 10 * dpr && bandH >= 12 * dpr) {
        const textX = drawX0 + 6 * dpr;
        const textY = bandTop + bandH / 2;
        ctx.lineWidth = 3 * dpr;
        ctx.strokeStyle = "rgba(0,0,0,0.55)";
        ctx.strokeText(label, textX, textY);
        ctx.fillStyle = "#ffffff";
        ctx.fillText(label, textX, textY);
      }
    }
  }

  ctx.restore();
}
