// === ConnectionStatusBanner.tsx ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";

import type { ConnectionState } from "@sen/client";

// Backend shutdown otherwise looks like a silent freeze; sample/event streams just stop.
//
// Dropped notifications are the same failure wearing a healthier face: the connection stays
// open, the streams keep flowing, and the values on screen are simply older than they look.
// The server counts what it dropped under backpressure and reports it precisely so a display
// can say so; saying nothing would leave an operator trusting a stale number.
export function ConnectionStatusBanner({
  state,
  url,
  onRetry,
  droppedNotifications = 0,
}: {
  state: ConnectionState;
  url: string;
  onRetry: () => void;
  droppedNotifications?: number;
}) {
  // Acknowledged count rather than a boolean: dismissing hides the drops seen so far, and a
  // later window raises the notice again instead of staying silent for the rest of the session.
  const [acknowledged, setAcknowledged] = useState(0);

  if (state === "open") {
    const unacknowledged = droppedNotifications - acknowledged;
    if (unacknowledged <= 0) return null;
    return (
      <StaleDataNotice count={unacknowledged} onDismiss={() => setAcknowledged(droppedNotifications)} />
    );
  }

  const { tone, label } = describe(state);
  return (
    <div
      role="status"
      aria-live="polite"
      style={{
        display: "flex",
        alignItems: "center",
        gap: 10,
        padding: "5px 14px",
        background: tone.bg,
        color: tone.fg,
        borderBottom: `1px solid ${tone.border}`,
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-sm)",
      }}
    >
      <span
        aria-hidden
        style={{
          width: 8,
          height: 8,
          borderRadius: "50%",
          background: tone.dot,
          boxShadow: state === "reconnecting" ? `0 0 6px ${tone.dot}` : undefined,
          animation: state === "reconnecting" ? "webex-conn-pulse 1.2s ease-in-out infinite" : undefined,
          flex: "none",
        }}
      />
      <span style={{ flex: 1, minWidth: 0, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
        {label}
        <span style={{ marginLeft: 8, color: tone.fgSubtle, fontFamily: "var(--font-mono)" }}>
          {url}
        </span>
      </span>
      {state === "closed" && (
        <button
          type="button"
          onClick={onRetry}
          style={{
            padding: "2px 10px",
            border: `1px solid ${tone.border}`,
            borderRadius: "var(--radius-sm)",
            background: "transparent",
            color: tone.fg,
            cursor: "pointer",
            fontFamily: "var(--font-ui)",
            fontSize: "var(--fs-sm)",
          }}
        >
          Retry
        </button>
      )}
    </div>
  );
}

function StaleDataNotice({ count, onDismiss }: { count: number; onDismiss: () => void }) {
  return (
    <div
      role="status"
      aria-live="polite"
      style={{
        display: "flex",
        alignItems: "center",
        gap: 10,
        padding: "5px 14px",
        background: "rgba(80, 50, 0, 0.45)",
        color: "#ffd590",
        borderBottom: "1px solid rgba(255, 180, 70, 0.5)",
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-sm)",
      }}
    >
      <span
        aria-hidden
        style={{ width: 8, height: 8, borderRadius: "50%", background: "#ffb446", flex: "none" }}
      />
      <span style={{ flex: 1, minWidth: 0 }}>
        {count === 1
          ? "1 update was dropped while the backend was catching up."
          : `${count.toLocaleString()} updates were dropped while the backend was catching up.`}
        <span style={{ marginLeft: 8, color: "rgba(255, 213, 144, 0.7)" }}>
          Values shown may be out of date. Reopen a panel to re-read it.
        </span>
      </span>
      <button
        type="button"
        onClick={onDismiss}
        style={{
          padding: "2px 10px",
          border: "1px solid rgba(255, 180, 70, 0.5)",
          borderRadius: "var(--radius-sm)",
          background: "transparent",
          color: "#ffd590",
          cursor: "pointer",
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-sm)",
        }}
      >
        Dismiss
      </button>
    </div>
  );
}

function describe(state: ConnectionState): {
  tone: { bg: string; fg: string; fgSubtle: string; border: string; dot: string };
  label: string;
} {
  switch (state) {
    case "connecting":
      return {
        tone: {
          bg: "rgba(15, 29, 50, 0.5)",
          fg: "var(--fg-base)",
          fgSubtle: "var(--fg-subtle)",
          border: "var(--border-glass)",
          dot: "var(--fg-muted)",
        },
        label: "Connecting...",
      };
    case "reconnecting":
      return {
        tone: {
          bg: "rgba(80, 50, 0, 0.45)",
          fg: "#ffd590",
          fgSubtle: "rgba(255, 213, 144, 0.7)",
          border: "rgba(255, 180, 70, 0.5)",
          dot: "#ffb446",
        },
        label: "Reconnecting to backend...",
      };
    case "closed":
      return {
        tone: {
          bg: "rgba(80, 25, 25, 0.5)",
          fg: "#ffb3ad",
          fgSubtle: "rgba(255, 179, 173, 0.7)",
          border: "var(--err)",
          dot: "var(--err)",
        },
        label: "Disconnected from backend.",
      };
    // "open" returns null at the call site; included for switch exhaustiveness.
    case "open":
      return {
        tone: {
          bg: "transparent",
          fg: "var(--fg-base)",
          fgSubtle: "var(--fg-subtle)",
          border: "transparent",
          dot: "var(--ok)",
        },
        label: "",
      };
  }
}
