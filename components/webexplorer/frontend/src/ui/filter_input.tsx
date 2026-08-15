// === filter_input.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";

export function FilterInput({
  value,
  onChange,
  placeholder = "Filter...",
  ariaLabel = "filter",
  fill = false,
}: {
  value: string;
  onChange: (next: string) => void;
  placeholder?: string;
  ariaLabel?: string;
  /** Grow to fill flex parent instead of capping at 320px / 240px basis. */
  fill?: boolean;
}) {
  const [focused, setFocused] = useState(false);
  // Keep the accent border while populated so the user remembers a filter is active.
  const active = focused || value !== "";
  return (
    <span
      style={{
        position: "relative",
        flex: fill ? 1 : "1 1 240px",
        maxWidth: fill ? undefined : 320,
        minWidth: fill ? 0 : undefined,
        display: "inline-flex",
        alignItems: "center",
      }}
    >
      <span
        aria-hidden="true"
        style={{
          position: "absolute",
          left: 8,
          top: "50%",
          transform: "translateY(-50%)",
          color: active ? "var(--accent)" : "var(--fg-subtle)",
          fontSize: "var(--fs-md)",
          pointerEvents: "none",
          transition: "color 120ms ease",
        }}
      >
        🔍
      </span>
      <input
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        onFocus={() => setFocused(true)}
        onBlur={() => setFocused(false)}
        placeholder={placeholder}
        aria-label={ariaLabel}
        spellCheck={false}
        autoComplete="off"
        style={{
          width: "100%",
          padding: "4px 26px 4px 26px",
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-md)",
          background: "var(--bg-input)",
          color: "var(--fg-base)",
          border: `1px solid ${active ? "var(--accent)" : "var(--border-default)"}`,
          borderRadius: "var(--radius-sm)",
          outline: "none",
          boxSizing: "border-box",
          boxShadow: focused ? "0 0 0 2px var(--accent-wash)" : "none",
          transition: "border-color 120ms ease, box-shadow 120ms ease",
        }}
      />
      {value && (
        <button
          type="button"
          onClick={() => onChange("")}
          aria-label="clear filter"
          title="Clear"
          style={{
            position: "absolute",
            right: 4,
            top: "50%",
            transform: "translateY(-50%)",
            width: 18,
            height: 18,
            display: "grid",
            placeItems: "center",
            padding: 0,
            background: "transparent",
            border: "none",
            color: "var(--fg-muted)",
            cursor: "pointer",
            borderRadius: 4,
          }}
        >
          <svg width="10" height="10" viewBox="0 0 10 10" aria-hidden>
            <path
              d="M1.5 1.5 L8.5 8.5 M8.5 1.5 L1.5 8.5"
              stroke="currentColor"
              strokeWidth="1.4"
              strokeLinecap="round"
            />
          </svg>
        </button>
      )}
    </span>
  );
}
