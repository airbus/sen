// === object_card_style.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";

import type { ClassSwatch } from "./class_color.js";

export type ObjectCardDensity = "tile" | "cockpit";

export function objectCardStyle(
  swatch: ClassSwatch,
  hover: boolean,
  isSelected: boolean,
  density: ObjectCardDensity,
): React.CSSProperties {
  const tile = density === "tile";
  const surface = isSelected
    ? "var(--card-surface-selected)"
    : hover
      ? "var(--card-surface-hover)"
      : "var(--card-surface-rest)";
  const border = isSelected
    ? swatch.accentSolid
    : hover
      ? "var(--card-border-hover)"
      : "var(--card-border-rest)";
  const accentBand = isSelected
    ? `inset 4px 0 0 0 ${swatch.accentSolid}`
    : `inset 2px 0 0 0 ${swatch.accent}`;
  return {
    position: "relative",
    background: `var(--card-top-sheen), ${surface}`,
    border: `1px solid ${border}`,
    borderRadius: "var(--radius-lg)",
    padding: tile ? "6px 8px 7px 11px" : "10px 12px 11px 14px",
    display: "flex",
    flexDirection: "column",
    gap: tile ? 3 : 6,
    height: "100%",
    cursor: "pointer",
    boxShadow: `${accentBand}, var(--card-top-highlight-inset), var(--card-drop-shadow)`,
    transition: "background 0.12s ease, border-color 0.12s ease, box-shadow 0.12s ease",
  };
}
