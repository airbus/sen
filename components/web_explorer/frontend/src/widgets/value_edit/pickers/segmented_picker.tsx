// === SegmentedPicker.tsx =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";

import type { PickerItem } from "./adaptive_picker.js";

export function SegmentedPicker({
  items,
  value,
  onChange,
}: {
  items: readonly PickerItem[];
  value: string;
  onChange: (v: string) => void;
}) {
  return (
    <span
      style={{
        display: "inline-flex",
        border: "1px solid var(--accent-border)",
        borderRadius: "var(--radius-sm)",
        overflow: "hidden",
      }}
    >
      {items.map((item, i) => (
        <SegmentedPickerItem
          key={item.name}
          label={item.label ?? item.name}
          description={item.description}
          active={item.name === value}
          showDivider={i > 0}
          onClick={() => onChange(item.name)}
        />
      ))}
    </span>
  );
}

function SegmentedPickerItem({
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
    ? "var(--accent)"
    : hover
      ? "var(--accent-wash)"
      : "var(--bg-input)";
  const color = active
    ? "var(--accent-fg)"
    : hover
      ? "var(--accent-text-wash)"
      : "var(--fg-base)";
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={description || undefined}
      style={{
        padding: "3px 10px",
        fontSize: "var(--fs-md)",
        fontFamily: "var(--font-mono)",
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
