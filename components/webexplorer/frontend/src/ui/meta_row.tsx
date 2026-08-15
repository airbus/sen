// === MetaRow.tsx =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";

import type { Client } from "@sen/client";

import { formatTimestamp } from "../core/format.js";
import { TypeChip } from "./chips.js";

/** Timestamp + type chip + writable/static badges; `wrap` allows breaking to a second line. */
export function MetaRow({
  client,
  declaredType,
  writable,
  isStatic,
  timestamp,
  wrap = true,
}: {
  client: Client | null;
  declaredType: string;
  writable: boolean;
  isStatic: boolean;
  timestamp: string | null;
  wrap?: boolean;
}) {
  return (
    <span
      style={{
        display: "inline-flex",
        gap: 6,
        alignItems: "center",
        flexWrap: wrap ? "wrap" : "nowrap",
      }}
    >
      {timestamp && (
        <span
          style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-xs)", color: "var(--fg-subtle)" }}
          title={timestamp + " UTC (server)"}
        >
          {formatTimestamp(timestamp)}
        </span>
      )}
      <TypeChip client={client} type={declaredType} />
      {writable && <MetaBadge color="var(--accent)">writable</MetaBadge>}
      {isStatic && <MetaBadge color="var(--fg-subtle)">static</MetaBadge>}
    </span>
  );
}

function MetaBadge({
  children,
  color,
}: {
  children: React.ReactNode;
  color: string;
}) {
  return (
    <span
      style={{
        fontSize: "var(--fs-xs)",
        color,
        border: `1px solid ${color}`,
        padding: "0 5px",
        borderRadius: "var(--radius-sm)",
        opacity: 0.7,
      }}
    >
      {children}
    </span>
  );
}
