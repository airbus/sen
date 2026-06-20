// === UPlotChart.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";
import { createPortal } from "react-dom";
import uPlot from "uplot";

import type { PanelKind } from "../core/panels.js";
import { useColorScheme } from "../state/color_scheme.js";
import { EMPTY_VIEW, timeMsAt, valueAt, type BufferView } from "../state/leaf_samples.js";
import { seriesLeafKey, type PlotSeries } from "../state/plot_board.js";
import { HARD_SAMPLE_CAP } from "../state/sample_store.js";
import { formatUnitAbbreviation } from "../state/unit_format.js";
import { formatValue } from "../core/format.js";
import { drawDiscreteTimeline, stateLabel } from "./discrete_timeline.js";

import "uplot/dist/uPlot.min.css";
import "./plot.css";

const MIN_CANVAS_HEIGHT = 60;

// Builders extend the last sample +1 day so the live tail always reaches the visible window.
const HARD_PLOT_CAP = HARD_SAMPLE_CAP + 1;

const FILL_GRADIENT_ENABLED = true;

function seriesFillGradient(color: string): (u: uPlot) => CanvasGradient {
  return (u) => {
    const ctx = u.ctx;
    const { top, height } = u.bbox;
    const g = ctx.createLinearGradient(0, top, 0, top + height);
    g.addColorStop(0, color + "30");
    g.addColorStop(1, color + "00");
    return g;
  };
}

// Sample-and-hold stepped path; `null` y starts a fresh sub-path (gap), `undefined`
// (uPlot.join alignment) is skipped without breaking the line.
const STEPPED_PATH: (u: uPlot, sIdx: number, i0: number, i1: number) => uPlot.Series.Paths = (
  u,
  sIdx,
  i0,
  i1,
) => {
  const xs = u.data[0]!;
  const ys = u.data[sIdx] as ReadonlyArray<number | null | undefined>;
  const scaleY = u.series[sIdx]!.scale ?? "y";
  const stroke = new Path2D();
  const fill = FILL_GRADIENT_ENABLED ? new Path2D() : null;
  const baseline = u.bbox.top + u.bbox.height;
  let segStartX = 0;
  let prevX = 0;
  let prevY = 0;
  let pendingRestart = true;
  for (let i = i0; i <= i1; i++) {
    const yVal = ys[i];
    if (yVal === null) {
      if (!pendingRestart && fill) {
        fill.lineTo(prevX, baseline);
        fill.lineTo(segStartX, baseline);
        fill.closePath();
      }
      pendingRestart = true;
      continue;
    }
    if (yVal === undefined) continue;
    const x = u.valToPos(xs[i] as number, "x", true);
    const y = u.valToPos(yVal, scaleY, true);
    if (pendingRestart) {
      stroke.moveTo(x, y);
      if (fill) {
        fill.moveTo(x, baseline);
        fill.lineTo(x, y);
      }
      segStartX = x;
      pendingRestart = false;
    } else {
      stroke.lineTo(x, prevY);
      stroke.lineTo(x, y);
      if (fill) {
        fill.lineTo(x, prevY);
        fill.lineTo(x, y);
      }
    }
    prevX = x;
    prevY = y;
  }
  if (!pendingRestart && fill) {
    fill.lineTo(prevX, baseline);
    fill.lineTo(segStartX, baseline);
    fill.closePath();
  }
  // clip:null bypasses uPlot's findGaps/clipGaps; we segment via moveTo already.
  return { stroke, fill, clip: null, band: null, gaps: [], flags: 0 };
};

const EMPTY: BufferView = EMPTY_VIEW;

export interface XRange {
  min: number;
  max: number;
}

// Anchored to wall-clock; visible window shifts by (Date.now() - anchorAt) each tick.
export interface AnchoredRange {
  range: XRange;
  anchorAt: number;
}

export interface UPlotChartProps {
  series: PlotSeries[];
  kind: PanelKind;
  samplesByLeaf: ReadonlyMap<string, BufferView>;
  windowSeconds: number;
  sharedRange: AnchoredRange | null;
  paused: boolean;
  onUserSetRange: (range: XRange) => void;
  onUserGoLive: () => void;
  onUserStartZoom: () => void;
}

export function UPlotChart({
  series,
  kind,
  samplesByLeaf,
  windowSeconds,
  sharedRange,
  paused,
  onUserSetRange,
  onUserGoLive,
  onUserStartZoom,
}: UPlotChartProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const plotRef = useRef<uPlot | null>(null);
  const sizeRef = useRef<{ width: number; height: number }>({ width: 0, height: 0 });
  // uPlot bakes axis/grid CSS vars at construction; chart must be rebuilt on scheme change.
  const colorScheme = useColorScheme();
  const onSetRangeRef = useRef(onUserSetRange);
  onSetRangeRef.current = onUserSetRange;
  const onGoLiveRef = useRef(onUserGoLive);
  onGoLiveRef.current = onUserGoLive;
  const onStartZoomRef = useRef(onUserStartZoom);
  onStartZoomRef.current = onUserStartZoom;
  const dragInProgressRef = useRef(false);
  // cursor.move fires at 250+ Hz; only mount/unmount of the overlay goes through React.
  const [cursorActive, setCursorActive] = useState(false);
  const cursorLeftRef = useRef<number | null>(null);
  const labelRefsRef = useRef<Map<string, HTMLSpanElement | null>>(new Map());

  // mouseleave is missed when the OS captures the pointer on focus change.
  useEffect(() => {
    const onBlur = () => {
      cursorLeftRef.current = null;
      setCursorActive(false);
    };
    const onVis = () => {
      if (document.visibilityState !== "visible") {
        cursorLeftRef.current = null;
        setCursorActive(false);
      }
    };
    window.addEventListener("blur", onBlur);
    document.addEventListener("visibilitychange", onVis);
    return () => {
      window.removeEventListener("blur", onBlur);
      document.removeEventListener("visibilitychange", onVis);
    };
  }, []);

  const seriesUnits = useMemo(
    () => series.map((s) => samplesByLeaf.get(seriesLeafKey(s))?.unit ?? null),
    [series, samplesByLeaf],
  );

  // 1st distinct unit -> left axis, 2nd -> right (y2), more fall back to left.
  const unitToScale = useMemo(() => {
    const map = new Map<string | null, string>();
    for (const u of seriesUnits) {
      if (map.has(u)) continue;
      map.set(u, map.size === 1 ? "y2" : "y");
    }
    return map;
  }, [seriesUnits]);

  const distinctUnits = useMemo(() => Array.from(unitToScale.keys()), [unitToScale]);
  const leftUnit = distinctUnits[0] ?? null;
  const rightUnit = distinctUnits.length >= 2 ? distinctUnits[1] ?? null : null;

  // uPlot can't add/remove series after init; rebuild the chart on signature change.
  const seriesSig = useMemo(
    () =>
      kind +
      "\n" +
      series
        .map(
          (s, i) =>
            `${s.sourceKey}|${s.leafPath}|${s.color}|${seriesUnits[i] ?? ""}|${unitToScale.get(seriesUnits[i] ?? null) ?? "y"}`,
        )
        .join("\n"),
    [kind, series, seriesUnits, unitToScale],
  );

  // Refs so cursor/draw hooks can read the latest inputs without re-creating the chart.
  const samplesByLeafRef = useRef(samplesByLeaf);
  samplesByLeafRef.current = samplesByLeaf;
  const seriesRef = useRef(series);
  seriesRef.current = series;
  const seriesUnitsRef = useRef(seriesUnits);
  seriesUnitsRef.current = seriesUnits;
  const unitToScaleRef = useRef(unitToScale);
  unitToScaleRef.current = unitToScale;
  const kindRef = useRef(kind);
  kindRef.current = kind;

  const updateCursorDom = useCallback((left: number | null) => {
    const plot = plotRef.current;
    if (!plot) return;
    if (left === null || left < 0) {
      for (const el of labelRefsRef.current.values()) {
        if (el) el.style.display = "none";
      }
      return;
    }
    const tSec = plot.posToVal(left, "x");
    if (tSec == null) return;
    const currentSeries = seriesRef.current;
    const currentSamples = samplesByLeafRef.current;
    const currentUnits = seriesUnitsRef.current;
    const currentScale = unitToScaleRef.current;
    const currentKind = kindRef.current;
    for (let sIdx = 0; sIdx < currentSeries.length; sIdx++) {
      const s = currentSeries[sIdx]!;
      const key = seriesLeafKey(s);
      const el = labelRefsRef.current.get(key);
      if (!el) continue;
      const view = currentSamples.get(key) ?? EMPTY;
      const idx = sampleIndexAtTime(view, tSec);
      if (idx < 0) {
        el.style.display = "none";
        continue;
      }
      // Gap sentinel: leaf was absent at this time, no label.
      if (view.vGap !== null && view.vGap[idx] === 1) {
        el.style.display = "none";
        continue;
      }
      const v = valueAt(view, idx);
      let yPx: number;
      let label: string;
      if (currentKind === "discrete") {
        if (typeof v !== "boolean" && typeof v !== "string") {
          el.style.display = "none";
          continue;
        }
        yPx = plot.valToPos(sIdx + 0.5, "y");
        label = stateLabel(v);
      } else {
        if (typeof v !== "number") {
          el.style.display = "none";
          continue;
        }
        const scaleKey = currentScale.get(currentUnits[sIdx] ?? null) ?? "y";
        yPx = plot.valToPos(v, scaleKey);
        label = formatValue(v);
      }
      if (!Number.isFinite(yPx)) {
        el.style.display = "none";
        continue;
      }
      el.style.display = "block";
      el.style.transform = `translate(${left + 6}px, ${yPx - 8}px)`;
      el.textContent = label;
    }
  }, []);

  // Per-series state->index for discrete panels. Booleans pinned to {false:0,true:1};
  // strings get order-of-appearance indices. Only the new-sample suffix is walked each frame.
  const stateMapsRef = useRef<Map<string | boolean, number>[]>([]);
  const lastScannedRef = useRef<number[]>([]);
  const seriesIdentityKey = useMemo(
    () =>
      kind +
      "|" +
      series
        .map((s) => {
          const k = samplesByLeaf.get(seriesLeafKey(s))?.vKind ?? "?";
          return `${seriesLeafKey(s)}#${k}`;
        })
        .join(","),
    [kind, series, samplesByLeaf],
  );
  // Sentinel-ref + synchronous if = deterministic "run once per dep identity flip"
  // semantics. useMemo would let React 19 re-run the factory for cache eviction.
  const lastSeriesIdentityKeyRef = useRef<string | null>(null);
  if (lastSeriesIdentityKeyRef.current !== seriesIdentityKey) {
    lastSeriesIdentityKeyRef.current = seriesIdentityKey;
    if (kind !== "discrete") {
      stateMapsRef.current = [];
      lastScannedRef.current = [];
    } else {
      stateMapsRef.current = series.map((s) => {
        const view = samplesByLeaf.get(seriesLeafKey(s)) ?? EMPTY;
        if (view.vKind === "boolean") {
          return new Map<string | boolean, number>([
            [false, 0],
            [true, 1],
          ]);
        }
        return new Map<string | boolean, number>();
      });
      // Booleans seeded; mark fully-scanned so the per-render walk skips them.
      lastScannedRef.current = series.map((s) => {
        const view = samplesByLeaf.get(seriesLeafKey(s)) ?? EMPTY;
        return view.vKind === "boolean" ? Number.MAX_SAFE_INTEGER : 0;
      });
    }
  }
  const lastSamplesByLeafRef = useRef<typeof samplesByLeaf | null>(null);
  if (lastSamplesByLeafRef.current !== samplesByLeaf) {
    lastSamplesByLeafRef.current = samplesByLeaf;
    if (kind === "discrete") {
      for (let i = 0; i < series.length; i++) {
        const view = samplesByLeaf.get(seriesLeafKey(series[i]!)) ?? EMPTY;
        const map = stateMapsRef.current[i];
        if (!map || view.vKind !== "string" || view.vStr === null) continue;
        // Trim shrunk view past last-scanned; reset+rescan drops trimmed-only states.
        if (view.size < (lastScannedRef.current[i] ?? 0)) {
          map.clear();
          lastScannedRef.current[i] = 0;
        }
        const start = lastScannedRef.current[i] ?? 0;
        if (start >= view.size) continue;
        const vStr = view.vStr;
        for (let j = start; j < view.size; j++) {
          const s = vStr[j];
          if (s !== undefined && !map.has(s)) map.set(s, map.size);
        }
        lastScannedRef.current[i] = view.size;
      }
    }
  }
  const discreteStateMaps = stateMapsRef.current;
  const discreteStateMapsRef = stateMapsRef;

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const width = el.clientWidth || 480;
    const height = Math.max(MIN_CANVAS_HEIGHT, el.clientHeight || 140);
    sizeRef.current = { width, height };
    const opts: uPlot.Options = {
      width,
      height,
      padding: [4, 4, 0, null],
      cursor: {
        drag: { x: true, y: false, setScale: false },
        bind: {
          mousedown: (_u, _t, handler) => (e: MouseEvent) => {
            dragInProgressRef.current = true;
            onStartZoomRef.current();
            handler(e);
            return null;
          },
          mouseup: (_u, _t, handler) => (e: MouseEvent) => {
            handler(e);
            dragInProgressRef.current = false;
            const u = plotRef.current;
            if (!u) return null;
            if (u.select.width > 0) {
              const min = u.posToVal(u.select.left, "x");
              const max = u.posToVal(u.select.left + u.select.width, "x");
              if (min != null && max != null && max - min > 0) {
                onSetRangeRef.current({ min, max });
              }
              u.setSelect({ left: 0, top: 0, width: 0, height: 0 }, false);
            }
            return null;
          },
          dblclick: () => () => {
            onGoLiveRef.current();
            return null;
          },
        },
      },
      hooks: {
        setCursor: [
          (u) => {
            const left = u.cursor.left;
            const active = left != null && left >= 0;
            cursorLeftRef.current = active ? left : null;
            updateCursorDom(cursorLeftRef.current);
            setCursorActive((prev) => (prev === active ? prev : active));
          },
        ],
        ...(kind === "discrete"
          ? {
              draw: [
                (u: uPlot) => {
                  drawDiscreteTimeline(
                    u,
                    seriesRef.current,
                    samplesByLeafRef.current,
                    discreteStateMapsRef.current,
                  );
                },
              ],
            }
          : {}),
      },
      legend: { show: false },
      scales:
        kind === "discrete"
          ? {
              x: { time: true, auto: false },
              y: {
                auto: false,
                range: () => [-0.05, Math.max(1, series.length) + 0.05] as [number, number],
              },
            }
          : {
              x: { time: true, auto: false },
              y: { auto: true },
              ...(rightUnit !== null || distinctUnits.length >= 2
                ? { y2: { auto: true } }
                : {}),
            },
      axes: [
        {
          stroke: getCssVarOrDefault("--fg-subtle", "#6f7a8d"),
          grid: { stroke: getCssVarOrDefault("--border-default", "rgba(150,180,235,.10)") },
          ticks: { show: false },
          font: "10px ui-monospace, monospace",
          size: 18,
          gap: 2,
          values: (_u, splits) =>
            splits.map((t) => {
              const d = new Date(t * 1000);
              return `${pad2(d.getHours())}:${pad2(d.getMinutes())}:${pad2(d.getSeconds())}`;
            }),
        },
        ...(kind === "discrete"
          ? [{ scale: "y", show: false, size: 4 }]
          : [
              {
                scale: "y",
                stroke: getCssVarOrDefault("--fg-subtle", "#6f7a8d"),
                grid: {
                  stroke: getCssVarOrDefault("--border-default", "rgba(150,180,235,.10)"),
                },
                font: "10px ui-monospace, monospace",
                size: 44,
                ...(leftUnit
                  ? {
                      label: formatUnitAbbreviation(leftUnit),
                      labelSize: 14,
                      labelFont: "10px ui-monospace, monospace",
                    }
                  : {}),
              },
              ...(rightUnit !== null || distinctUnits.length >= 2
                ? [
                    {
                      scale: "y2",
                      side: 1 as const,
                      stroke: getCssVarOrDefault("--fg-subtle", "#6f7a8d"),
                      grid: { show: false },
                      font: "10px ui-monospace, monospace",
                      size: 44,
                      ...(rightUnit
                        ? {
                            label: formatUnitAbbreviation(rightUnit),
                            labelSize: 14,
                            labelFont: "10px ui-monospace, monospace",
                          }
                        : {}),
                    },
                  ]
                : []),
            ]),
      ],
      series: [
        {},
        ...series.map((s, i) => ({
          stroke: s.color,
          width: 1.4,
          points: { show: false },
          // null y values render as a discontinuity (builders emit null on vGap[i] === 1).
          spanGaps: false,
          paths: kind === "discrete" ? () => null : STEPPED_PATH,
          // Spread instead of `fill: undefined` because exactOptionalPropertyTypes
          // rejects explicit-undefined for uPlot's Fill optional.
          ...(kind === "discrete" || !FILL_GRADIENT_ENABLED
            ? {}
            : { fill: seriesFillGradient(s.color) }),
          scale: kind === "discrete" ? "y" : unitToScale.get(seriesUnits[i] ?? null) ?? "y",
        })),
      ],
    };
    const emptyData: uPlot.AlignedData = [[], ...series.map(() => [] as (number | null)[])];
    // Callback form: in the popout the div belongs to a different document and uPlot's
    // `then instanceof HTMLElement` check fails. Callback form sidesteps it.
    const plot = new uPlot(opts, emptyData, (self, init) => {
      el.appendChild(self.root);
      init();
    });
    plotRef.current = plot;

    const ro = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const w = entry.contentRect.width;
        const h = Math.max(MIN_CANVAS_HEIGHT, entry.contentRect.height);
        if (w > 0 && (w !== sizeRef.current.width || h !== sizeRef.current.height)) {
          sizeRef.current = { width: w, height: h };
          plot.setSize({ width: w, height: h });
        }
      }
    });
    ro.observe(el);

    return () => {
      ro.disconnect();
      plot.destroy();
      plotRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [seriesSig, colorScheme]);

  // Reusing scratch arrays drops the dominant per-frame allocation; grow/shrink at render
  // time so React 19 can't re-run the size logic.
  const scratchRef = useRef<{ xs: number[]; ys: (number | null)[] }[]>([]);
  if (scratchRef.current.length !== series.length) {
    const next: { xs: number[]; ys: (number | null)[] }[] = [];
    for (let i = 0; i < series.length; i++) {
      const prev = scratchRef.current[i];
      next.push(prev ?? { xs: new Array<number>(HARD_PLOT_CAP), ys: new Array<number | null>(HARD_PLOT_CAP) });
    }
    scratchRef.current = next;
  }

  // Numeric y/y2 use auto:true so setData rescales them; discrete has fixed range.
  useEffect(() => {
    const plot = plotRef.current;
    if (!plot) return;
    const nowSec = Date.now() / 1000;
    const inputs: uPlot.AlignedData[] = series.map((s, sIdx) => {
      const view = samplesByLeaf.get(seriesLeafKey(s)) ?? EMPTY;
      const scratch = scratchRef.current[sIdx]!;
      return kind === "numeric"
        ? buildNumericInput(view, nowSec, scratch)
        : buildDiscreteInput(view, sIdx, discreteStateMaps[sIdx], nowSec, scratch);
    });
    plot.setData(joinInputs(inputs));
    // Cursor pixel position unchanged but the value under it shifted with the data.
    if (cursorLeftRef.current !== null) {
      updateCursorDom(cursorLeftRef.current);
    }
  }, [kind, series, samplesByLeaf, discreteStateMaps, updateCursorDom, colorScheme]);

  // No samples dep: new samples shouldn't restart the rAF loop.
  useEffect(() => {
    const plot = plotRef.current;
    if (!plot) return;
    const applyScale = () => {
      if (dragInProgressRef.current) return;
      const nowSec = Date.now() / 1000;
      const range = sharedRange
        ? (() => {
            const elapsed = (Date.now() - sharedRange.anchorAt) / 1000;
            return {
              min: sharedRange.range.min + elapsed,
              max: sharedRange.range.max + elapsed,
            };
          })()
        : { min: nowSec - windowSeconds, max: nowSec };
      plot.setScale("x", range);
      // Reposition labels after the slide; otherwise they freeze at the mousemove value.
      if (cursorLeftRef.current !== null) {
        updateCursorDom(cursorLeftRef.current);
      }
    };
    applyScale();
    if (paused) return;
    // rAF: back-pressures under load and syncs with the compositor.
    let rafId = 0;
    const tick = () => {
      applyScale();
      rafId = requestAnimationFrame(tick);
    };
    rafId = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(rafId);
  }, [windowSeconds, sharedRange, paused, seriesSig, updateCursorDom, colorScheme]);

  const plot = plotRef.current;
  const overlay =
    cursorActive && plot
      ? createPortal(
          <CursorLabels series={series} labelRefs={labelRefsRef} />,
          plot.over,
        )
      : null;
  return (
    <>
      <div ref={containerRef} style={{ width: "100%", flex: 1, minHeight: 0 }} />
      {overlay}
    </>
  );
}

const EMPTY_DATA: uPlot.AlignedData = [[], []];

function buildNumericInput(
  view: BufferView,
  nowSec: number,
  scratch: { xs: number[]; ys: (number | null)[] },
): uPlot.AlignedData {
  if (view.size === 0 || view.vKind !== "number") return EMPTY_DATA;
  const { xs, ys } = scratch;
  let written = 0;
  const vNum = view.vNum!;
  const vGap = view.vGap;
  // Synthetic hold point before each gap; with align:1 stepped paths uPlot otherwise turns
  // the last real value into a 1px jump that becomes a phantom cross-gap connection.
  let lastRealY: number | null = null;
  for (let i = 0; i < view.size; i++) {
    if (vGap !== null && vGap[i] === 1) {
      const tGap = timeMsAt(view, i) / 1000;
      if (lastRealY !== null) {
        xs[written] = tGap - 0.001;
        ys[written] = lastRealY;
        written++;
      }
      xs[written] = tGap;
      ys[written] = null;
      written++;
      continue;
    }
    const v = vNum[i];
    // Non-gap NaNs aren't absence signals; let the line interpolate over them.
    if (v === undefined || !Number.isFinite(v)) continue;
    xs[written] = timeMsAt(view, i) / 1000;
    ys[written] = v;
    lastRealY = v;
    written++;
  }
  if (written === 0) return EMPTY_DATA;
  const lastT = xs[written - 1]!;
  const lastY = ys[written - 1]!;
  // Forward sentinel +1 day so the tail always reaches the visible window.
  xs[written] = Math.max(nowSec, lastT) + 86400;
  ys[written] = lastY;
  xs.length = written + 1;
  ys.length = written + 1;
  return [xs, ys];
}

// Each distinct state spreads evenly across [sIdx+0.15, sIdx+0.85].
function buildDiscreteInput(
  view: BufferView,
  sIdx: number,
  stateMap: Map<string | boolean, number> | undefined,
  nowSec: number,
  scratch: { xs: number[]; ys: (number | null)[] },
): uPlot.AlignedData {
  if (view.size === 0 || !stateMap) return EMPTY_DATA;
  const { xs, ys } = scratch;
  const denom = Math.max(1, stateMap.size - 1);
  const vGap = view.vGap;
  // vKind branch hoisted out of the loop to avoid a per-sample closure.
  let written = 0;
  if (view.vKind === "boolean") {
    const vBool = view.vBool!;
    for (let i = 0; i < view.size; i++) {
      if (vGap !== null && vGap[i] === 1) {
        xs[written] = timeMsAt(view, i) / 1000;
        ys[written] = null;
        written++;
        continue;
      }
      const idx = stateMap.get(vBool[i] === 1);
      if (idx === undefined) continue;
      xs[written] = timeMsAt(view, i) / 1000;
      ys[written] = sIdx + 0.15 + (idx / denom) * 0.7;
      written++;
    }
  } else if (view.vKind === "string") {
    const vStr = view.vStr!;
    for (let i = 0; i < view.size; i++) {
      if (vGap !== null && vGap[i] === 1) {
        xs[written] = timeMsAt(view, i) / 1000;
        ys[written] = null;
        written++;
        continue;
      }
      const v = vStr[i];
      if (v === undefined) continue;
      const idx = stateMap.get(v);
      if (idx === undefined) continue;
      xs[written] = timeMsAt(view, i) / 1000;
      ys[written] = sIdx + 0.15 + (idx / denom) * 0.7;
      written++;
    }
  }
  if (written === 0) return EMPTY_DATA;
  const lastT = xs[written - 1]!;
  const lastY = ys[written - 1]!;
  xs[written] = Math.max(nowSec, lastT) + 86400;
  ys[written] = lastY;
  xs.length = written + 1;
  ys.length = written + 1;
  return [xs, ys];
}

function joinInputs(inputs: uPlot.AlignedData[]): uPlot.AlignedData {
  if (inputs.length === 0) return [[]];
  if (inputs.length === 1) return inputs[0]!;
  return uPlot.join(inputs);
}

// Latest index with timestamp <= tSec; clamps to 0 before the first sample, -1 if empty.
function sampleIndexAtTime(view: BufferView, tSec: number): number {
  if (view.size === 0) return -1;
  let lo = 0;
  let hi = view.size - 1;
  let result = -1;
  while (lo <= hi) {
    const mid = (lo + hi) >>> 1;
    const t = timeMsAt(view, mid) / 1000;
    if (t <= tSec) {
      result = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return result < 0 ? 0 : result;
}

// Spans + ref collection only; position/text written imperatively by updateCursorDom.
function CursorLabels({
  series,
  labelRefs,
}: {
  series: PlotSeries[];
  labelRefs: React.MutableRefObject<Map<string, HTMLSpanElement | null>>;
}) {
  return (
    <>
      {series.map((s) => {
        const key = seriesLeafKey(s);
        return (
          <span
            key={key}
            ref={(el) => {
              if (el === null) labelRefs.current.delete(key);
              else labelRefs.current.set(key, el);
            }}
            style={{
              position: "absolute",
              left: 0,
              top: 0,
              display: "none",
              fontFamily: "var(--font-mono)",
              fontSize: "var(--fs-sm)",
              padding: "1px 5px",
              background: "var(--bg-base)",
              color: s.color,
              border: `1px solid ${s.color}`,
              borderRadius: "var(--radius-sm)",
              pointerEvents: "none",
              whiteSpace: "nowrap",
              zIndex: 2,
              willChange: "transform",
            }}
          />
        );
      })}
    </>
  );
}

function getCssVarOrDefault(name: string, fallback: string): string {
  if (typeof document === "undefined") return fallback;
  const v = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  return v || fallback;
}

function pad2(n: number): string {
  return n < 10 ? `0${n}` : String(n);
}
