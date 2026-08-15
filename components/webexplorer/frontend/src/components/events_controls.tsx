// === events_controls.tsx =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";

import { DangerPillButton, LiveToggleButton } from "../ui/buttons.js";


export function EventsControls({
  live,
  onToggleLive,
  onClear,
  status,
}: {
  live: boolean;
  onToggleLive: () => void;
  onClear: () => void;
  status?: React.ReactNode;
}) {
  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        gap: 8,
        padding: "6px 12px",
        borderBottom: "1px solid var(--border-default)",
      }}
    >
      <DangerPillButton
        onClick={onClear}
        title="Clear the visible event log (other surfaces keep their copy)"
      />
      {status !== undefined && (
        <span
          style={{
            marginLeft: "auto",
            fontSize: "var(--fs-sm)",
            color: "var(--fg-subtle)",
            fontFamily: "var(--font-mono)",
          }}
        >
          {status}
        </span>
      )}
      <LiveToggleButton on={live} onToggle={onToggleLive} />
    </div>
  );
}
