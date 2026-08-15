// === ConnectionStatusBanner.tsx ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ConnectionState } from "@sen/client";

// Backend shutdown otherwise looks like a silent freeze; sample/event streams just stop.
export function ConnectionStatusBanner({
  state,
  url,
  onRetry,
}: {
  state: ConnectionState;
  url: string;
  onRetry: () => void;
}) {
  if (state === "open") return null;
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
