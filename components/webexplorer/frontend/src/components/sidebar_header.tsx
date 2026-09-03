// === sidebar_header.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import logoUrl from "../assets/logo_simple.svg";
import { colorSchemeActions, useColorScheme } from "../state/color_scheme.js";
import { SettingsPopover } from "./settings_popover.js";

export function SidebarHeader() {
  return (
    <header
      style={{
        display: "flex",
        alignItems: "center",
        gap: 10,
        padding: "0 8px 0 14px",
        flex: 1,
        minWidth: 0,
      }}
    >
      <SenMark />
      <span
        style={{
          flex: 1,
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-xl)",
          fontWeight: 600,
          color: "var(--fg-base)",
          letterSpacing: "0.01em",
          whiteSpace: "nowrap",
          overflow: "hidden",
          textOverflow: "ellipsis",
        }}
      >
        Sen Explorer
      </span>
      <span
        style={{
          display: "inline-flex",
          alignItems: "center",
          gap: 4,
          flex: "none",
        }}
      >
        <ColorSchemeToggle />
        <SettingsPopover />
      </span>
    </header>
  );
}

function ColorSchemeToggle() {
  const scheme = useColorScheme();
  const isLight = scheme === "light";
  const label = isLight ? "switch to dark mode" : "switch to light mode";
  const activeColor = "var(--accent-text-wash)";
  const inactiveColor = "var(--fg-subtle)";
  return (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        color: "var(--fg-subtle)",
      }}
    >
      <span aria-hidden style={{ color: isLight ? activeColor : inactiveColor, display: "inline-flex" }}>
        <SunIcon />
      </span>
      <button
        type="button"
        role="switch"
        aria-checked={!isLight}
        aria-label={label}
        title={label}
        onClick={() => colorSchemeActions.toggle()}
        style={{
          position: "relative",
          display: "inline-block",
          width: 32,
          height: 18,
          padding: 0,
          border: "1px solid var(--border-strong)",
          borderRadius: 999,
          background: "var(--border-emphasis)",
          cursor: "pointer",
          transition: "background 120ms ease",
          verticalAlign: "middle",
        }}
      >
        <span
          aria-hidden
          style={{
            position: "absolute",
            top: 1,
            left: isLight ? 1 : 15,
            width: 14,
            height: 14,
            borderRadius: 999,
            background: "var(--bg-elevated)",
            boxShadow: "0 1px 2px rgba(0, 0, 0, 0.35)",
            transition: "left 180ms ease",
          }}
        />
      </button>
      <span aria-hidden style={{ color: isLight ? inactiveColor : activeColor, display: "inline-flex" }}>
        <MoonIcon />
      </span>
    </span>
  );
}

function SunIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" aria-hidden>
      <circle cx="7" cy="7" r="2.6" fill="none" stroke="currentColor" strokeWidth="1.4" />
      <g stroke="currentColor" strokeWidth="1.4" strokeLinecap="round">
        <line x1="7" y1="0.8" x2="7" y2="2.2" />
        <line x1="7" y1="11.8" x2="7" y2="13.2" />
        <line x1="0.8" y1="7" x2="2.2" y2="7" />
        <line x1="11.8" y1="7" x2="13.2" y2="7" />
        <line x1="2.4" y1="2.4" x2="3.4" y2="3.4" />
        <line x1="10.6" y1="10.6" x2="11.6" y2="11.6" />
        <line x1="11.6" y1="2.4" x2="10.6" y2="3.4" />
        <line x1="3.4" y1="10.6" x2="2.4" y2="11.6" />
      </g>
    </svg>
  );
}

function MoonIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 14 14" aria-hidden>
      <path
        d="M11 8.4 A 5.2 5.2 0 1 1 6.2 2.4 A 4.2 4.2 0 0 0 11 8.4 Z"
        fill="currentColor"
        stroke="none"
      />
    </svg>
  );
}

// Inlined as data URL by Vite so the single-file build stays self-contained.
function SenMark() {
  return (
    <img
      src={logoUrl}
      alt=""
      aria-hidden
      width={24}
      height={24}
      style={{ display: "block", flex: "none" }}
    />
  );
}
