// === SqlTooltip.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { createPortal } from "react-dom";

export function SqlTooltip({ sql, top, left }: { sql: string; top: number; left: number }) {
  // Portalled to <body> so backdrop-filter on the nav pane can't reanchor position:fixed.
  if (typeof document === "undefined") return null;
  return createPortal(
    <span
      role="tooltip"
      style={{
        position: "fixed",
        top,
        left,
        zIndex: 100,
        maxWidth: 420,
        minWidth: 200,
        padding: "3px 6px",
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-sm)",
        lineHeight: 1.45,
        color: "var(--fg-base)",
        background: "var(--bg-elevated)",
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-sm)",
        boxShadow: "0 4px 12px rgba(0, 0, 0, 0.35)",
        whiteSpace: "pre-wrap",
        wordBreak: "break-word",
        pointerEvents: "none",
      }}
    >
      {sql}
    </span>,
    document.body,
  );
}
