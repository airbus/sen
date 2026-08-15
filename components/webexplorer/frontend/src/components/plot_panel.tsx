// === PlotPanel.tsx ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";

import type { Client } from "@sen/client";

import type { PanelKind } from "../core/panels.js";
import type { PropertyKey } from "../core/watch_keys.js";
import { EMPTY_VIEW, valueAt, type BufferView } from "../state/leaf_samples.js";
import { seriesLeafKey, type PlotBoardPanel } from "../state/plot_board.js";
import { useMultipleLeafHistories } from "../state/sample_store.js";
import { GripIcon } from "../ui/icons.js";
import { LegendChip } from "./legend_chip.js";
import {
  UPlotChart,
  type AnchoredRange,
  type XRange,
} from "./u_plot_chart.js";

export type { AnchoredRange, XRange };

export interface PlotPanelProps {
  client: Client | null;
  panel: PlotBoardPanel;
  /** Live-follow window length in seconds (used when `sharedRange === null`). */
  windowSeconds: number;
  /** Anchored shared range across the board; `null` = live-follow at `windowSeconds`. */
  sharedRange: AnchoredRange | null;
  /** Freeze the visible window; data keeps flowing, only the scale stops. */
  paused: boolean;
  onUserSetRange: (range: XRange) => void;
  onUserGoLive: () => void;
  /** Mousedown on a plot pauses the board so the scale doesn't shift mid-brush. */
  onUserStartZoom: () => void;
  onDropSeries: (sourceKey: PropertyKey, leafPath: string) => void;
  onRemoveSeries: (sourceKey: PropertyKey, leafPath: string) => void;
}

export function PlotPanel({
  panel,
  windowSeconds,
  sharedRange,
  paused,
  onUserSetRange,
  onUserGoLive,
  onUserStartZoom,
  onDropSeries,
  onRemoveSeries,
}: PlotPanelProps) {
  const samplesByLeaf = useMultipleLeafHistories(panel.series);
  const [dropHover, setDropHover] = useState(false);
  return (
    <div
      onDragOver={(e) => {
        if (!e.dataTransfer.types.includes("application/x-sen-plot-leaf")) return;
        e.preventDefault();
        // Board container has a fallback dragover for gridstack's gaps; this panel owns
        // dropEffect + hover state so don't let it override.
        e.stopPropagation();
        e.dataTransfer.dropEffect = "copy";
        setDropHover(true);
      }}
      onDragLeave={() => setDropHover(false)}
      onDrop={(e) => {
        setDropHover(false);
        const raw = e.dataTransfer.getData("application/x-sen-plot-leaf");
        if (!raw) return;
        e.preventDefault();
        try {
          // PropertyKey brand reasserted at the deserialization boundary.
          const payload = JSON.parse(raw) as {
            sourceKey: PropertyKey;
            leafPath: string;
            kind?: PanelKind;
          };
          const dragKind: PanelKind = payload.kind ?? "numeric";
          if (dragKind !== panel.kind) return;
          onDropSeries(payload.sourceKey, payload.leafPath);
        } catch {
          /* ignore malformed payload */
        }
      }}
      style={{
        border: `1px solid ${dropHover ? "var(--accent)" : "var(--border-default)"}`,
        borderRadius: "var(--radius-sm)",
        background: dropHover
          ? "color-mix(in oklch, var(--accent) 22%, var(--surface-recessed))"
          : "var(--surface-recessed)",
        boxShadow: dropHover
          ? "inset 0 0 0 2px var(--accent), 0 0 0 1px color-mix(in oklch, var(--accent) 40%, transparent)"
          : undefined,
        display: "flex",
        flexDirection: "column",
        height: "100%",
        minHeight: 0,
        transition: "background 0.12s, border-color 0.12s",
      }}
    >
      <PanelHeader
        panel={panel}
        samplesByLeaf={samplesByLeaf}
        onRemoveSeries={onRemoveSeries}
      />
      <UPlotChart
        series={panel.series}
        kind={panel.kind}
        samplesByLeaf={samplesByLeaf}
        windowSeconds={windowSeconds}
        sharedRange={sharedRange}
        paused={paused}
        onUserSetRange={onUserSetRange}
        onUserGoLive={onUserGoLive}
        onUserStartZoom={onUserStartZoom}
      />
    </div>
  );
}

function PanelHeader({
  panel,
  samplesByLeaf,
  onRemoveSeries,
}: {
  panel: PlotBoardPanel;
  samplesByLeaf: ReadonlyMap<string, BufferView>;
  onRemoveSeries: (sourceKey: PropertyKey, leafPath: string) => void;
}) {
  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        flexWrap: "wrap",
        gap: 6,
        padding: "5px 10px",
        borderBottom: "1px solid var(--border-default)",
      }}
    >
      {/* GridStack's drag-handle selector is `.plot-panel-drag`. Don't preventDefault on
          pointer/mousedown -- gridstack's drag manager bails on canceled events. */}
      <span
        className="plot-panel-drag"
        role="button"
        aria-label="reorder plot"
        title="Drag to reorder"
        style={{
          display: "inline-flex",
          alignItems: "center",
          padding: "0 2px",
          color: "var(--fg-subtle)",
          cursor: "grab",
          userSelect: "none",
        }}
      >
        <GripIcon />
      </span>
      {panel.series.map((s) => {
        const view = samplesByLeaf.get(seriesLeafKey(s)) ?? EMPTY_VIEW;
        // Gap sentinel at the tail -> show the no-data placeholder, not NaN/false/"".
        const isGap = view.size > 0 && view.vGap !== null && view.vGap[view.size - 1] === 1;
        const last = view.size && !isGap ? valueAt(view, view.size - 1) : null;
        return (
          <LegendChip
            key={seriesLeafKey(s)}
            series={s}
            value={last}
            kind={panel.kind}
            onRemove={() => onRemoveSeries(s.sourceKey, s.leafPath)}
          />
        );
      })}
    </div>
  );
}
