// === ModeToggle.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";

export type AuthoringMode = "guided" | "raw";

export function ModeToggle({
  value,
  onChange,
}: {
  value: AuthoringMode;
  onChange: (v: AuthoringMode) => void;
}) {
  const opts: { value: AuthoringMode; label: string; description: string }[] = [
    { value: "guided", label: "Guided", description: "Field-row builder with completion chips" },
    { value: "raw", label: "Raw SQL", description: "Single textarea - paste a full query" },
  ];
  return (
    <span
      style={{
        display: "inline-flex",
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-sm)",
        overflow: "hidden",
      }}
    >
      {opts.map((o, i) => (
        <ModeOption
          key={o.value}
          label={o.label}
          description={o.description}
          active={o.value === value}
          showDivider={i > 0}
          onClick={() => onChange(o.value)}
        />
      ))}
    </span>
  );
}

function ModeOption({
  label,
  description,
  active,
  showDivider,
  onClick,
}: {
  label: string;
  description: string;
  active: boolean;
  showDivider: boolean;
  onClick: () => void;
}) {
  const [hover, setHover] = useState(false);
  const background = active
    ? "var(--bg-elevated)"
    : hover
      ? "var(--accent-wash)"
      : "transparent";
  const color = active
    ? "var(--fg-base)"
    : hover
      ? "var(--accent-text-wash)"
      : "var(--fg-subtle)";
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={description}
      style={{
        padding: "2px 8px",
        fontSize: "var(--fs-sm)",
        fontFamily: "var(--font-ui)",
        textTransform: "uppercase",
        letterSpacing: "0.05em",
        border: "none",
        borderLeft: showDivider ? "1px solid var(--border-default)" : "none",
        background,
        color,
        cursor: "pointer",
        transition: "background 120ms ease, color 120ms ease",
      }}
    >
      {label}
    </button>
  );
}
