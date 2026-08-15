// === CenterPane.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";
import type * as React from "react";

import type { Client } from "@sen/client";

import type { Selection } from "../state/selection.js";
import { ListIcon } from "../ui/icons.js";
import { CockpitWorkspace } from "./cockpit_workspace.js";
import { ObjectsWorkspace } from "./objects_workspace.js";

type CenterTab = "objects" | "cockpit";

const ACTIVE_TAB_KEY = "webex.center.activeTab";

function loadActiveTab(): CenterTab {
  try {
    const raw = localStorage.getItem(ACTIVE_TAB_KEY);
    return raw === "cockpit" ? "cockpit" : "objects";
  } catch {
    return "objects";
  }
}

export interface CenterPaneProps {
  client: Client | null;
  onOpenInPaneExplorer: (sel: Selection) => void;
  banner?: React.ReactNode;
}

export function CenterPane({ client, onOpenInPaneExplorer, banner }: CenterPaneProps) {
  const [active, setActive] = useState<CenterTab>(loadActiveTab);
  useEffect(() => {
    try {
      localStorage.setItem(ACTIVE_TAB_KEY, active);
    } catch {
    }
  }, [active]);
  return (
    <div
      style={{
        display: "flex",
        flexDirection: "column",
        height: "100%",
        gap: "var(--pane-gutter-top)",
      }}
    >
      {banner}
      <TabStrip active={active} onChange={setActive} />
      <div
        style={{
          flex: 1,
          minHeight: 0,
          overflow: "hidden",
          // Inset matches `--radius-xl` so the bottom drawer's rounded corners don't clip cards.
          paddingLeft: "var(--radius-xl)",
        }}
      >
        {active === "objects" ? (
          <ObjectsWorkspace client={client} onOpenInPaneExplorer={onOpenInPaneExplorer} />
        ) : (
          <div style={{ height: "100%", overflow: "auto" }}>
            <CockpitWorkspace client={client} onOpenInPaneExplorer={onOpenInPaneExplorer} />
          </div>
        )}
      </div>
    </div>
  );
}

function TabStrip({
  active,
  onChange,
}: {
  active: CenterTab;
  onChange: (tab: CenterTab) => void;
}) {
  return (
    <div
      role="tablist"
      style={{
        display: "flex",
        alignItems: "stretch",
        gap: 0,
        // Height + chrome mirror the sidebar identity card so the two top regions align.
        height: "var(--topbar-height)",
        padding: "0 8px",
        flex: "none",
        borderRadius: "var(--radius-xl)",
        border: "1px solid var(--border-glass)",
        background: "var(--surface-pane-nav)",
        boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
        overflow: "hidden",
      }}
    >
      <Tab
        label="Objects"
        icon={<ListIcon />}
        active={active === "objects"}
        onClick={() => onChange("objects")}
      />
      <Tab
        label="Cockpit"
        icon={<CockpitTabIcon />}
        active={active === "cockpit"}
        onClick={() => onChange("cockpit")}
      />
    </div>
  );
}

function Tab({
  label,
  icon,
  active,
  onClick,
}: {
  label: string;
  icon: React.ReactNode;
  active: boolean;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      role="tab"
      aria-selected={active}
      onClick={onClick}
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        background: "transparent",
        border: "none",
        borderBottom: active ? "2px solid var(--accent)" : "2px solid transparent",
        margin: "0 4px",
        padding: "8px 12px",
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-lg)",
        fontWeight: active ? 600 : 500,
        color: active ? "var(--fg-base)" : "var(--fg-muted)",
        cursor: "pointer",
        letterSpacing: "0.02em",
        transition: "color 120ms ease, border-color 120ms ease",
      }}
    >
      <span
        aria-hidden
        style={{
          display: "inline-flex",
          color: active ? "var(--accent)" : "var(--fg-subtle)",
          transition: "color 120ms ease",
        }}
      >
        {icon}
      </span>
      {label}
    </button>
  );
}

function CockpitTabIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" aria-hidden>
      <rect x="1.5" y="1.5" width="4.5" height="4.5" rx="1" fill="none" stroke="currentColor" strokeWidth="1.4" />
      <rect x="8" y="1.5" width="4.5" height="4.5" rx="1" fill="none" stroke="currentColor" strokeWidth="1.4" />
      <rect x="1.5" y="8" width="4.5" height="4.5" rx="1" fill="none" stroke="currentColor" strokeWidth="1.4" />
      <rect x="8" y="8" width="4.5" height="4.5" rx="1" fill="currentColor" stroke="currentColor" strokeWidth="1.4" />
    </svg>
  );
}
