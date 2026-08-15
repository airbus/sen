// === icons.tsx =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState, type ReactNode } from "react";

import type { OrderBy } from "./bus_queries.js";

export function Caret({ open }: { open: boolean }) {
  return (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        width: 14,
        height: 14,
        color: "var(--fg-muted)",
        transform: open ? "rotate(90deg)" : "none",
        transition: "transform 0.1s",
        flex: "none",
      }}
    >
      <svg
        width="10"
        height="10"
        viewBox="0 0 16 16"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <polyline points="6,3 11,8 6,13" />
      </svg>
    </span>
  );
}

export function TypeTag({ children }: { children: ReactNode }) {
  return (
    <span
      style={{
        fontSize: "var(--fs-2xs)",
        fontFamily: "var(--font-ui)",
        textTransform: "uppercase",
        letterSpacing: "0.10em",
        color: "var(--fg-subtle)",
        minWidth: 0,
        overflow: "hidden",
        flexShrink: 2,
      }}
    >
      {children}
    </span>
  );
}

export function SessionName({ children }: { children: ReactNode }) {
  return (
    <span
      style={{
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-lg)",
        fontWeight: 600,
        color: "var(--fg-base)",
        flex: "none",
      }}
    >
      {children}
    </span>
  );
}

export function BusName({ children }: { children: ReactNode }) {
  return (
    <span
      style={{
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-lg)",
        fontWeight: 600,
        letterSpacing: "0.02em",
        color: "var(--fg-base)",
        flex: "none",
      }}
    >
      {children}
    </span>
  );
}

export function AddInterestButton({ onClick, active }: { onClick: () => void; active: boolean }) {
  const [hover, setHover] = useState(false);
  const on = hover || active;
  return (
    <button
      type="button"
      onClick={(e) => {
        e.stopPropagation();
        onClick();
      }}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      aria-label="add query"
      title="Add query"
      style={{
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        width: 28,
        height: 24,
        padding: 0,
        background: on ? "rgba(214, 168, 100, 0.18)" : "rgba(214, 168, 100, 0.06)",
        color: on ? "rgb(232, 184, 110)" : "rgb(200, 158, 92)",
        border: `1px solid ${on ? "rgba(214, 168, 100, 0.45)" : "rgba(214, 168, 100, 0.28)"}`,
        borderRadius: "var(--radius-sm)",
        cursor: "pointer",
        transition: "background 0.12s, color 0.12s, border-color 0.12s",
        flex: "none",
      }}
    >
      <svg width="14" height="14" viewBox="0 0 14 14" fill="none" aria-hidden="true">
        <path
          d="M1 3 L8 3 L5.75 6 L5.75 9 L3.25 9 L3.25 6 Z"
          stroke="currentColor"
          strokeWidth="1"
          strokeLinejoin="round"
        />
        <line x1="11" y1="2.5" x2="11" y2="7.5" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" />
        <line x1="8.5" y1="5" x2="13.5" y2="5" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" />
      </svg>
    </button>
  );
}

export function Hint({
  children,
  indent = 0,
  color,
}: {
  children: ReactNode;
  indent?: number;
  color?: string;
}) {
  return (
    <div
      style={{
        padding: `4px 12px 4px ${12 + indent * 12}px`,
        color: color ?? "var(--fg-muted)",
        fontSize: "var(--fs-md)",
        fontFamily: color === "var(--err)" ? "var(--font-mono)" : undefined,
      }}
    >
      {children}
    </div>
  );
}

const searchInputStyle = {
  width: "100%",
  background: "rgba(6, 12, 22, 0.6)",
  border: "1px solid var(--border-default)",
  borderRadius: "var(--radius-sm)",
  color: "var(--fg-base)",
  fontFamily: "var(--font-mono)",
  fontSize: "var(--fs-md)",
  outline: "none",
  boxSizing: "border-box" as const,
};

export function SearchInput({
  value,
  onChange,
  placeholder = "Filter objects...",
}: {
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
}) {
  return (
    <div
      style={{
        padding: "8px 10px",
        borderBottom: "1px solid var(--border-default)",
        position: "relative",
      }}
    >
      <span
        aria-hidden="true"
        style={{
          position: "absolute",
          left: 18,
          top: "50%",
          transform: "translateY(-50%)",
          color: "var(--fg-subtle)",
          fontSize: "var(--fs-md)",
          pointerEvents: "none",
        }}
      >
        🔍
      </span>
      <input
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        spellCheck={false}
        autoComplete="off"
        style={{
          ...searchInputStyle,
          padding: "4px 6px 4px 24px",
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
            right: 12,
            top: "50%",
            transform: "translateY(-50%)",
            width: 24,
            height: 24,
            display: "grid",
            placeItems: "center",
            padding: 0,
            background: "transparent",
            border: "none",
            borderRadius: "var(--radius-sm)",
            cursor: "pointer",
            color: "var(--fg-subtle)",
            fontSize: "var(--fs-md)",
          }}
        >
          ✕
        </button>
      )}
    </div>
  );
}

export function SortToggle({ value, onChange }: { value: OrderBy; onChange: (v: OrderBy) => void }) {
  const opts: { value: OrderBy; label: string }[] = [
    { value: "name", label: "name" },
    { value: "class", label: "class" },
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
      {opts.map((o) => {
        const active = o.value === value;
        return (
          <button
            key={o.value}
            type="button"
            onClick={() => onChange(o.value)}
            style={{
              padding: "1px 6px",
              fontSize: "var(--fs-xs)",
              fontFamily: "var(--font-mono)",
              background: active ? "var(--bg-elevated)" : "transparent",
              color: active ? "var(--fg-base)" : "var(--fg-subtle)",
              border: "none",
              cursor: "pointer",
            }}
          >
            {o.label}
          </button>
        );
      })}
    </span>
  );
}
