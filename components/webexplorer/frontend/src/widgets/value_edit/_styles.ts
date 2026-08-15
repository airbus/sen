// === _styles.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";

export function baseInputStyle(extra: React.CSSProperties = {}): React.CSSProperties {
  return {
    padding: "3px 8px",
    fontFamily: "var(--font-mono)",
    fontSize: "var(--fs-md)",
    background: "var(--bg-input)",
    // Accent-tinted resting border is the editable-affordance cue; per-input states promote
    // to solid --accent on hover/focus.
    border: "1px solid var(--accent-border)",
    borderRadius: "var(--radius-sm)",
    color: "var(--fg-base)",
    outline: "none",
    ...extra,
  };
}

export function chipButton(variant: "accent" | "ghost"): React.CSSProperties {
  const base: React.CSSProperties = {
    padding: "2px 9px",
    fontSize: "var(--fs-sm)",
    border: "1px solid var(--border-default)",
    borderRadius: "var(--radius-sm)",
    cursor: "pointer",
    fontFamily: "var(--font-ui)",
  };
  if (variant === "accent") {
    return { ...base, background: "var(--accent-gradient)", color: "var(--accent-fg)", borderColor: "var(--accent)" };
  }
  return { ...base, background: "transparent", color: "var(--fg-muted)" };
}

export function spinnerButtonStyle(side: "left" | "right", borderColor?: string): React.CSSProperties {
  return {
    width: 22,
    padding: 0,
    fontFamily: "var(--font-mono)",
    fontSize: "var(--fs-lg)",
    background: "var(--bg-input)",
    border: `1px solid ${borderColor ?? "var(--accent-border)"}`,
    borderRadius:
      side === "left"
        ? "var(--radius-sm) 0 0 var(--radius-sm)"
        : "0 var(--radius-sm) var(--radius-sm) 0",
    color: "var(--fg-muted)",
    cursor: "pointer",
    display: "inline-flex",
    alignItems: "center",
    justifyContent: "center",
  };
}

export function withinBounds(v: number, lo: number | null, hi: number | null): boolean {
  if (lo !== null && v < lo) return false;
  if (hi !== null && v > hi) return false;
  return true;
}

export function truncate(s: string, max: number): string {
  return s.length <= max ? s : s.slice(0, max - 1) + "...";
}

export function isVoidReturn(returnType: string): boolean {
  return returnType === "" || returnType === "void";
}
