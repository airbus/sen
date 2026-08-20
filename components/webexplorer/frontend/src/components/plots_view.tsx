// === PlotsView.tsx ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useContext, useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";

import type { Client } from "@sen/client";

import type { PropertyKey } from "../core/watch_keys.js";
import type { PlotBoardPanel } from "../state/plot_board.js";
import { RETENTION_BOUNDS, useSampleRetentionSeconds } from "../state/settings.js";
import { watchActions } from "../state/watch_plot.js";
import { DangerPillButton, HoverIconButton, LiveToggleButton } from "../ui/buttons.js";
import { EmptyPane, Mono } from "../ui/empty_state.js";
import { GridStackGrid, type GridStackItem, type LayoutEntry } from "../ui/grid_stack_grid.js";
import { ResetIcon } from "../ui/icons.js";
import { SegmentedControl } from "../ui/segmented_control.js";
import { BottomPaneHeaderSlot } from "./bottom_pane.js";
import { PlotPanel, type AnchoredRange, type XRange } from "./plot_panel.js";

import "./plot.css";

interface PanelLayout {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface PlotsViewProps {
  client: Client | null;
  panels: readonly PlotBoardPanel[];
  onRemoveSeries: (sourceKey: PropertyKey, leafPath: string) => void;
  onMoveSeriesToPanel: (panelId: string, sourceKey: PropertyKey, leafPath: string) => void;
  onMoveSeriesToNewPanel: (sourceKey: PropertyKey, leafPath: string) => void;
}

/** dataTransfer MIME for legend-chip drags. Payload: JSON `{ sourceKey, leafPath, kind }`. */
export const DRAG_MIME = "application/x-sen-plot-leaf";

const DEFAULT_PANEL_HEIGHT = 180;
const MIN_PANEL_HEIGHT = 90;
const MAX_PANEL_HEIGHT = 600;
const COLUMN_COUNT_OPTIONS = [1, 2, 3, 4] as const;
const DEFAULT_COLUMN_COUNT = 1;
const COLUMN_COUNT_KEY = "webex.plotColumnCount";
const PANEL_LAYOUT_KEY = "webex.plotPanelLayout";
const PANEL_LAYOUT_VERSION = 1;
const GRID_GAP_PX = 6;
const CELL_HEIGHT_PX = 40;

// Discrete panels need one row per series so in-band state labels survive the cull threshold.
function defaultPanelHeight(panel: PlotBoardPanel): number {
  if (panel.kind !== "discrete") return DEFAULT_PANEL_HEIGHT;
  const headerAndAxis = 80;
  const rowHeight = 40;
  return Math.min(
    MAX_PANEL_HEIGHT,
    headerAndAxis + rowHeight * Math.max(1, panel.series.length),
  );
}
function pxToCells(px: number): number {
  return Math.max(1, Math.round(px / CELL_HEIGHT_PX));
}

const DEFAULT_WINDOW_SECONDS = 30;
const MIN_WINDOW_SECONDS = 1;
const WINDOW_KEY = "webex.plotWindowSeconds";

export function PlotsView({
  client,
  panels,
  onRemoveSeries,
  onMoveSeriesToPanel,
  onMoveSeriesToNewPanel,
}: PlotsViewProps) {
  // Effective window clamps at use time so it can't exceed what the store retains.
  const maxWindowSeconds = useSampleRetentionSeconds();
  const [windowSeconds, setWindowSeconds] = useState<number>(() => {
    try {
      const raw = localStorage.getItem(WINDOW_KEY);
      if (raw) {
        const n = parseInt(raw, 10);
        if (Number.isFinite(n) && n >= MIN_WINDOW_SECONDS && n <= RETENTION_BOUNDS.max) {
          return n;
        }
      }
    } catch {
    }
    return DEFAULT_WINDOW_SECONDS;
  });
  useEffect(() => {
    try {
      localStorage.setItem(WINDOW_KEY, String(windowSeconds));
    } catch {
    }
  }, [windowSeconds]);
  const effectiveWindowSeconds = Math.min(windowSeconds, maxWindowSeconds);
  // sharedRange null = default window; non-null slides forward each tick unless paused.
  // paused = explicit Live/Pause toggle; zoom gestures don't auto-pause.
  const [sharedRange, setSharedRange] = useState<AnchoredRange | null>(null);
  const [paused, setPaused] = useState(false);
  const boardRef = useRef<HTMLDivElement | null>(null);
  // Multiple reset listeners because `dragend` is unreliable when the source unmounts.
  const [chipDragActive, setChipDragActive] = useState(false);
  useEffect(() => {
    const onStart = (e: DragEvent) => {
      if (e.dataTransfer?.types.includes(DRAG_MIME)) setChipDragActive(true);
    };
    const onEnd = () => setChipDragActive(false);
    document.addEventListener("dragstart", onStart);
    document.addEventListener("dragend", onEnd);
    document.addEventListener("drop", onEnd);
    document.addEventListener("mouseup", onEnd);
    return () => {
      document.removeEventListener("dragstart", onStart);
      document.removeEventListener("dragend", onEnd);
      document.removeEventListener("drop", onEnd);
      document.removeEventListener("mouseup", onEnd);
    };
  }, []);
  const setUserRange = useCallback((range: XRange) => {
    setSharedRange({ range, anchorAt: Date.now() });
  }, []);
  const resetZoom = useCallback(() => {
    setSharedRange(null);
    setPaused(false);
  }, []);
  /** Re-anchor zoomed range to "now" so it doesn't jump forward by the pause duration. */
  const resumeLive = useCallback(() => {
    setPaused(false);
    setSharedRange((prev) => {
      if (!prev) return prev;
      const width = prev.range.max - prev.range.min;
      const now = Date.now() / 1000;
      return { range: { min: now - width, max: now }, anchorAt: Date.now() };
    });
  }, []);
  const togglePaused = useCallback(() => {
    setPaused((wasPaused) => {
      if (wasPaused) {
        setSharedRange((prev) => {
          if (!prev) return prev;
          const width = prev.range.max - prev.range.min;
          const now = Date.now() / 1000;
          return { range: { min: now - width, max: now }, anchorAt: Date.now() };
        });
      }
      return !wasPaused;
    });
  }, []);

  const [columnCount, setColumnCount] = useState<number>(() => {
    try {
      const raw = localStorage.getItem(COLUMN_COUNT_KEY);
      const n = raw ? parseInt(raw, 10) : NaN;
      if (Number.isFinite(n) && (COLUMN_COUNT_OPTIONS as readonly number[]).includes(n)) {
        return n;
      }
    } catch {
    }
    return DEFAULT_COLUMN_COUNT;
  });
  useEffect(() => {
    try {
      localStorage.setItem(COLUMN_COUNT_KEY, String(columnCount));
    } catch {
    }
  }, [columnCount]);

  // Versioned `{version, panels}` envelope; mismatched versions get dropped on load.
  // Widths in columns (clamped to columnCount at render); heights in CELL_HEIGHT_PX rows.
  const [panelLayouts, setPanelLayouts] = useState<Record<string, PanelLayout>>(() => {
    try {
      const raw = localStorage.getItem(PANEL_LAYOUT_KEY);
      if (!raw) return {};
      const parsed: unknown = JSON.parse(raw);
      if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) return {};
      const envelope = parsed as { version?: unknown; panels?: unknown };
      if (envelope.version !== PANEL_LAYOUT_VERSION) return {};
      if (envelope.panels === null || typeof envelope.panels !== "object" || Array.isArray(envelope.panels)) {
        return {};
      }
      // Drop entries with non-finite numbers; bad values would hide a panel offscreen.
      const out: Record<string, PanelLayout> = {};
      for (const [k, v] of Object.entries(envelope.panels as Record<string, unknown>)) {
        if (v === null || typeof v !== "object") continue;
        const r = v as Record<string, unknown>;
        if (
          typeof r.x === "number" && Number.isFinite(r.x) &&
          typeof r.y === "number" && Number.isFinite(r.y) &&
          typeof r.w === "number" && Number.isFinite(r.w) && r.w > 0 &&
          typeof r.h === "number" && Number.isFinite(r.h) && r.h > 0
        ) {
          out[k] = { x: r.x, y: r.y, w: r.w, h: r.h };
        }
      }
      return out;
    } catch {
      return {};
    }
  });
  useEffect(() => {
    try {
      const payload = { version: PANEL_LAYOUT_VERSION, panels: panelLayouts };
      localStorage.setItem(PANEL_LAYOUT_KEY, JSON.stringify(payload));
    } catch {
    }
  }, [panelLayouts]);
  // handleLayoutChange only adds, so prune defunct ids here.
  useEffect(() => {
    setPanelLayouts((prev) => {
      const live = new Set(panels.map((p) => p.id));
      let touched = false;
      const next: Record<string, PanelLayout> = {};
      for (const k of Object.keys(prev)) {
        if (live.has(k)) next[k] = prev[k]!;
        else touched = true;
      }
      return touched ? next : prev;
    });
  }, [panels]);
  const handleLayoutChange = useCallback((entries: LayoutEntry<string>[]) => {
    setPanelLayouts((prev) => {
      let changed = false;
      const next: Record<string, PanelLayout> = { ...prev };
      for (const e of entries) {
        const cur = next[e.key];
        if (!cur || cur.x !== e.x || cur.y !== e.y || cur.w !== e.w || cur.h !== e.h) {
          next[e.key] = { x: e.x, y: e.y, w: e.w, h: e.h };
          changed = true;
        }
      }
      return changed ? next : prev;
    });
  }, []);

  const zoomed = sharedRange !== null;

  const headerSlot = useContext(BottomPaneHeaderSlot);
  const toolbar = (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 10,
        fontSize: "var(--fs-md)",
        color: "var(--fg-muted)",
      }}
    >
      <WindowControl
        value={effectiveWindowSeconds}
        min={MIN_WINDOW_SECONDS}
        max={maxWindowSeconds}
        disabled={panels.length === 0 || zoomed}
        onChange={setWindowSeconds}
      />
      {/* Kept rendered (disabled) so the toolbar layout doesn't shift on reset.
          Highlighted while zoomed so the user knows where to go to get back to live. */}
      <HoverIconButton
        onClick={resetZoom}
        disabled={!zoomed}
        highlighted={zoomed}
        tooltip={zoomed ? "Reset zoom - back to the default window" : "No zoom to reset"}
        ariaLabel="reset zoom"
        icon={<ResetIcon />}
        size={20}
      />
      <ToolbarDivider />
      <LiveToggleButton
        on={!paused}
        disabled={panels.length === 0}
        onToggle={togglePaused}
        highlighted={zoomed && paused}
      />
      <ToolbarDivider />
      <ColumnCountPicker
        value={columnCount}
        onChange={setColumnCount}
        disabled={panels.length === 0}
      />
      <ClearAllPanelsButton disabled={panels.length === 0} onClick={watchActions.clearAllPanels} />
    </span>
  );

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", minHeight: 0 }}>
      {/* Portal into the BottomPane title bar; fall back to an inline strip. */}
      {headerSlot ? (
        createPortal(toolbar, headerSlot)
      ) : (
        <div
          style={{
            display: "flex",
            justifyContent: "flex-end",
            padding: "6px 12px",
            borderBottom: "1px solid var(--border-default)",
          }}
        >
          {toolbar}
        </div>
      )}
      {panels.length === 0 ? (
        <EmptyPane
          heading="Nothing plotted yet."
          help={[
            <>
              Click <Mono>+plot</Mono> next to any numeric leaf - in the Object Explorer's
              Properties tab or in a Watch readout - to add a panel here.
            </>,
            <>
              In the plot board, drag horizontally to zoom; double-click resumes live. Use the
              Range slider in the toolbar to set the live time span.
            </>,
          ]}
        />
      ) : (
        <div
          ref={boardRef}
          // Blanket dragover so the cursor stays "+" across gridstack's gaps; real drop
          // targets stopPropagation to avoid double-handling.
          onDragOver={(e) => {
            if (!e.dataTransfer.types.includes(DRAG_MIME)) return;
            e.preventDefault();
            e.dataTransfer.dropEffect = "copy";
          }}
          style={{
            flex: 1,
            overflow: "auto",
            padding: "4px 8px",
            position: "relative",
          }}
        >
          <GridStackGrid<string>
            items={panels.map<GridStackItem<string>>((panel) => {
              const stored = panelLayouts[panel.id];
              const defaultHCells = pxToCells(defaultPanelHeight(panel));
              return {
                key: panel.id,
                ...(stored
                  ? { x: stored.x, y: stored.y, w: Math.min(columnCount, stored.w), h: stored.h }
                  : { w: 1, h: defaultHCells }),
                minW: 1,
                maxW: columnCount,
                minH: pxToCells(MIN_PANEL_HEIGHT),
                maxH: pxToCells(MAX_PANEL_HEIGHT),
                render: () => (
                  <PlotPanel
                    client={client}
                    panel={panel}
                    windowSeconds={effectiveWindowSeconds}
                    sharedRange={sharedRange}
                    paused={paused}
                    onUserSetRange={setUserRange}
                    onUserGoLive={resumeLive}
                    onUserStartZoom={() => setPaused(true)}
                    onDropSeries={(sourceKey, leafPath) =>
                      onMoveSeriesToPanel(panel.id, sourceKey, leafPath)
                    }
                    onRemoveSeries={onRemoveSeries}
                  />
                ),
              };
            })}
            columnCount={columnCount}
            cellHeight={CELL_HEIGHT_PX}
            gap={GRID_GAP_PX}
            dragHandleSelector=".plot-panel-drag"
            resizeHandles="e, se, s, sw, w"
            onLayoutChange={handleLayoutChange}
          />
          <NewPanelDropZone
            onDrop={onMoveSeriesToNewPanel}
            dragActive={chipDragActive}
          />
        </div>
      )}
    </div>
  );
}

function ColumnCountPicker({
  value,
  onChange,
  disabled,
}: {
  value: number;
  onChange: (n: number) => void;
  disabled: boolean;
}) {
  return (
    <span
      title="Number of columns in the plot grid"
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        opacity: disabled ? 0.5 : 1,
        pointerEvents: disabled ? "none" : undefined,
      }}
    >
      <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-subtle)" }}>cols</span>
      <SegmentedControl<string>
        ariaLabel="Number of columns"
        value={String(value)}
        onChange={(v) => onChange(parseInt(v, 10))}
        options={COLUMN_COUNT_OPTIONS.map((n) => ({ value: String(n), label: String(n) }))}
        preserveCase
      />
    </span>
  );
}

// Hairline at rest; expands to dashed accent while a chip is being dragged.
function NewPanelDropZone({
  onDrop,
  dragActive,
}: {
  onDrop: (sourceKey: PropertyKey, leafPath: string) => void;
  dragActive: boolean;
}) {
  const [hover, setHover] = useState(false);
  return (
    <div
      onDragOver={(e) => {
        if (!e.dataTransfer.types.includes(DRAG_MIME)) return;
        e.preventDefault();
        e.stopPropagation();
        e.dataTransfer.dropEffect = "copy";
        setHover(true);
      }}
      onDragLeave={() => setHover(false)}
      onDrop={(e) => {
        setHover(false);
        const raw = e.dataTransfer.getData(DRAG_MIME);
        if (!raw) return;
        e.preventDefault();
        try {
          const payload = JSON.parse(raw) as { sourceKey: PropertyKey; leafPath: string };
          onDrop(payload.sourceKey, payload.leafPath);
        } catch {
          /* ignore malformed payload */
        }
      }}
      style={{
        minHeight: dragActive ? 72 : 12,
        marginTop: dragActive ? 8 : 0,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        border: dragActive
          ? `${hover ? "2px solid" : "1px dashed"} var(--accent)`
          : "1px dashed transparent",
        background: dragActive
          ? `color-mix(in oklch, var(--accent) ${hover ? 14 : 6}%, transparent)`
          : "transparent",
        borderRadius: "var(--radius-sm)",
        color: "var(--accent-text-wash)",
        fontSize: "var(--fs-sm)",
        fontFamily: "var(--font-mono)",
        // Height NOT transitioned so the zone snaps open before the cursor reaches it.
        transition: "background 0.12s, border-color 0.12s",
        pointerEvents: dragActive ? "auto" : "none",
      }}
    >
      {dragActive && <span>Drop here for a new panel</span>}
    </div>
  );
}


function ToolbarDivider() {
  return (
    <span
      aria-hidden
      style={{
        width: 1,
        height: 16,
        background: "var(--border-default)",
        flex: "none",
      }}
    />
  );
}

function ClearAllPanelsButton({
  disabled,
  onClick,
}: {
  disabled: boolean;
  onClick: () => void;
}) {
  return (
    <DangerPillButton
      onClick={onClick}
      disabled={disabled}
      title="Remove every plot panel and its series"
    />
  );
}

// Preset chips filtered at render to drop entries above the retention bound.
const RANGE_PRESETS_SECONDS = [5, 15, 30, 60, 300, 600] as const;

// `90` -> `1m 30s`, `3660` -> `1h 1m`.
function formatSeconds(s: number): string {
  if (s < 60) return `${s}s`;
  if (s < 3600) {
    const m = Math.floor(s / 60);
    const rem = s % 60;
    return rem === 0 ? `${m}m` : `${m}m ${rem}s`;
  }
  const h = Math.floor(s / 3600);
  const remM = Math.floor((s % 3600) / 60);
  return remM === 0 ? `${h}h` : `${h}h ${remM}m`;
}

// Log mapping: linear slider over 1-600s would give only 5% of the track to 1-30s.
function logSliderToSeconds(ratio: number, min: number, max: number): number {
  const lo = Math.log(Math.max(1, min));
  const hi = Math.log(max);
  return Math.round(Math.exp(lo + ratio * (hi - lo)));
}

function secondsToLogSliderRatio(seconds: number, min: number, max: number): number {
  const lo = Math.log(Math.max(1, min));
  const hi = Math.log(max);
  const v = Math.log(Math.max(min, Math.min(max, seconds)));
  return (v - lo) / Math.max(1e-9, hi - lo);
}

// Disabled while a zoom is active; sharedRange dictates the range instead.
function WindowControl({
  value,
  min,
  max,
  disabled,
  onChange,
}: {
  value: number;
  min: number;
  max: number;
  disabled: boolean;
  onChange: (v: number) => void;
}) {
  const presets = RANGE_PRESETS_SECONDS.filter((p) => p >= min && p <= max);
  const ratio = secondsToLogSliderRatio(value, min, max);
  const fillPct = `${(ratio * 100).toFixed(1)}%`;
  return (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 10,
        opacity: disabled ? 0.5 : 1,
      }}
      title={disabled ? "Time range is locked while a zoom is active" : "Live view time range"}
    >
      <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-subtle)" }}>range</span>
      <SegmentedControl<string>
        ariaLabel="Time range preset"
        value={presets.includes(value as (typeof RANGE_PRESETS_SECONDS)[number]) ? String(value) : ""}
        onChange={(v) => onChange(parseInt(v, 10))}
        options={presets.map((p) => ({ value: String(p), label: formatSeconds(p) }))}
        preserveCase
      />
      <input
        type="range"
        className="webex-range-slider"
        min={0}
        max={1000}
        step={1}
        value={Math.round(ratio * 1000)}
        disabled={disabled}
        onChange={(e) => {
          const r = parseInt(e.currentTarget.value, 10) / 1000;
          onChange(logSliderToSeconds(r, min, max));
        }}
        style={{ ["--fill" as never]: fillPct }}
      />
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-base)",
          minWidth: 56,
          textAlign: "right",
          fontVariantNumeric: "tabular-nums",
        }}
      >
        {formatSeconds(value)}
      </span>
    </span>
  );
}
