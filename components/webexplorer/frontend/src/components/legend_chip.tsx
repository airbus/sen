// === LegendChip.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { formatValue } from "../core/format.js";
import type { PanelKind } from "../core/panels.js";
import type { PlotSeries } from "../state/plot_board.js";
import { CloseIcon } from "../ui/icons.js";
import { HoverIconButton } from "../ui/buttons.js";

export function LegendChip({
  series,
  value,
  kind,
  onRemove,
}: {
  series: PlotSeries;
  value: number | boolean | string | null;
  kind: PanelKind;
  onRemove: () => void;
}) {
  return (
    <span
      draggable
      onDragStart={(e) => {
        // "copy" shows the "+" cursor; drop targets mirror with dropEffect = "copy".
        e.dataTransfer.effectAllowed = "copy";
        e.dataTransfer.setData(
          "application/x-sen-plot-leaf",
          // kind lets the drop target refuse cross-kind moves.
          JSON.stringify({ sourceKey: series.sourceKey, leafPath: series.leafPath, kind }),
        );
      }}
      title={`${series.objectName}.${series.propertyName}${series.leafPath ? "." + series.leafPath : ""}`}
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        padding: "2px 7px",
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-sm)",
        background: "var(--bg-elevated)",
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-sm)",
        cursor: "grab",
        userSelect: "none",
      }}
    >
      <span
        style={{
          width: 8,
          height: 8,
          borderRadius: 2,
          background: series.color,
          flex: "none",
        }}
      />
      <span
        style={{
          color: "var(--fg-muted)",
          maxWidth: 180,
          overflow: "hidden",
          textOverflow: "ellipsis",
          whiteSpace: "nowrap",
        }}
      >
        {shortLabel(series)}
      </span>
      <span
        style={{
          color: value == null ? "var(--fg-subtle)" : "var(--fg-base)",
          minWidth: 36,
          textAlign: "right",
        }}
      >
        {value == null
          ? "--"
          : typeof value === "number"
            ? formatValue(value)
            : typeof value === "boolean"
              ? value
                ? "true"
                : "false"
              : value}
      </span>
      <HoverIconButton
        onClick={onRemove}
        tooltip="Remove series"
        ariaLabel="remove series"
        icon={<CloseIcon />}
        danger
        size={14}
      />
    </span>
  );
}

function shortLabel(s: PlotSeries): string {
  const base = `${s.objectName}.${s.propertyName}`;
  return s.leafPath ? `${base}.${s.leafPath}` : base;
}
