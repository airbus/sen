// === empty_state.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ReactNode } from "react";

/** Single-line muted hint for dense empty surfaces (tab bodies, no-match filters). */
export function EmptyHint({ children }: { children: ReactNode }) {
  return (
    <div style={{ padding: 12, color: "var(--fg-muted)", fontSize: "var(--fs-md)" }}>{children}</div>
  );
}

/** Heading + help paragraphs for workspace-sized empty panes. */
export function EmptyPane({
  heading,
  help,
  icon,
}: {
  heading: ReactNode;
  help?: ReactNode | ReactNode[];
  /** Switches the layout to a centered column with the icon above the heading. */
  icon?: ReactNode;
}) {
  const helpItems = Array.isArray(help) ? help : help !== undefined ? [help] : [];
  if (icon) {
    return (
      <div
        style={{
          padding: "32px 24px",
          color: "var(--fg-muted)",
          fontSize: "var(--fs-lg)",
          lineHeight: 1.6,
          display: "flex",
          flexDirection: "column",
          alignItems: "center",
          textAlign: "center",
          maxWidth: 520,
          margin: "0 auto",
        }}
      >
        <div
          aria-hidden="true"
          style={{
            color: "var(--fg-subtle)",
            display: "inline-flex",
            transform: "scale(2.4)",
            marginBottom: 18,
            opacity: 0.7,
          }}
        >
          {icon}
        </div>
        {heading}
        {helpItems.map((h, i) => (
          <div
            key={i}
            style={{ marginTop: i === 0 ? 6 : 8, fontSize: "var(--fs-md)", color: "var(--fg-subtle)" }}
          >
            {h}
          </div>
        ))}
      </div>
    );
  }
  return (
    <div style={{ padding: 16, color: "var(--fg-muted)", fontSize: "var(--fs-lg)", lineHeight: 1.6 }}>
      {heading}
      {helpItems.map((h, i) => (
        <div
          key={i}
          style={{ marginTop: i === 0 ? 4 : 8, fontSize: "var(--fs-md)", color: "var(--fg-subtle)" }}
        >
          {h}
        </div>
      ))}
    </div>
  );
}

/** Inline mono affordance name for use inside `EmptyPane` help text. */
export function Mono({ children }: { children: ReactNode }) {
  return <span style={{ fontFamily: "var(--font-mono)" }}>{children}</span>;
}
