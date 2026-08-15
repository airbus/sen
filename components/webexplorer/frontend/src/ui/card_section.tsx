// === card_section.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";

import { Collapsible } from "./layout.js";
import { HoverIconButton } from "./buttons.js";
import { ChevronDownIcon } from "./icons.js";

export interface CollapsibleSectionProps {
  collapsed: boolean;
  onToggleCollapsed: () => void;
  /** Render whatever chip/text marks this section's identity. */
  label: React.ReactNode;
  count: number;
  /** Rendered to the right of the hairline. */
  trailing?: React.ReactNode;
  /** Used for the toggle's aria-label (`expand <ariaLabel>` / `collapse <ariaLabel>`). */
  ariaLabel: string;
  children: React.ReactNode;
}

export function CollapsibleSection({
  collapsed,
  onToggleCollapsed,
  label,
  count,
  trailing,
  ariaLabel,
  children,
}: CollapsibleSectionProps) {
  return (
    <section style={{ display: "flex", flexDirection: "column", gap: 6 }}>
      <SectionHeader
        collapsed={collapsed}
        onToggleCollapsed={onToggleCollapsed}
        label={label}
        count={count}
        trailing={trailing}
        ariaLabel={ariaLabel}
      />
      <Collapsible open={!collapsed}>{children}</Collapsible>
    </section>
  );
}

function SectionHeader({
  collapsed,
  onToggleCollapsed,
  label,
  count,
  trailing,
  ariaLabel,
}: Omit<CollapsibleSectionProps, "children">) {
  return (
    <header
      style={{
        display: "flex",
        alignItems: "center",
        gap: 8,
        // Negative horizontal margins overhang the workspace body's 2px padding so
        // chevron + label + hairline sit flush with the central-pane edges.
        margin: "0 -2px",
        padding: "2px 0",
        color: "var(--fg-subtle)",
        position: "relative",
      }}
    >
      <HoverIconButton
        size={18}
        ariaLabel={collapsed ? `expand ${ariaLabel}` : `collapse ${ariaLabel}`}
        tooltip={collapsed ? "Show" : "Hide"}
        onClick={onToggleCollapsed}
        icon={
          <span
            aria-hidden
            style={{
              display: "inline-flex",
              transform: collapsed ? "rotate(-90deg)" : "rotate(0deg)",
              transition: "transform 200ms ease-out",
            }}
          >
            <ChevronDownIcon />
          </span>
        }
      />
      {label}
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-subtle)",
        }}
      >
        {count}
      </span>
      {/* Hairline doubles as the inter-section divider. */}
      <span
        aria-hidden
        style={{ flex: 1, height: 1, background: "var(--border-default)" }}
      />
      {trailing}
    </header>
  );
}
