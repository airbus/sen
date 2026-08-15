// === buttons.tsx =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";
import type * as React from "react";

import { HelpIcon, ICON_BUTTON_SIZE, PauseIcon, PlayIcon } from "./icons.js";
import { Tooltip } from "./tooltip.js";

/** For toggling buttons use {@link IconToggle}. */
export function HoverIconButton({
  onClick,
  tooltip,
  ariaLabel,
  icon,
  danger,
  disabled = false,
  outline = false,
  highlighted = false,
  size = ICON_BUTTON_SIZE,
}: {
  onClick: (e: React.MouseEvent<HTMLButtonElement>) => void;
  tooltip: string;
  ariaLabel: string;
  icon: React.ReactNode;
  /** Hover uses error color for destructive actions. */
  danger?: boolean;
  disabled?: boolean;
  /** Visible border at rest so prominent action buttons read as actionable. */
  outline?: boolean;
  /** Paint the resting state with the accent chrome (border + wash bg + accent text)
   *  to call attention to the button -- e.g. "click this to get back to live" while a
   *  plot is zoomed. Hover keeps working on top. */
  highlighted?: boolean;
  /** Defaults to {@link ICON_BUTTON_SIZE} which clears WCAG 2.5.8 target-size on mouse. */
  size?: number;
}) {
  const [hover, setHover] = useState(false);
  const interactive = !disabled;
  const accentBorder = danger ? "var(--err)" : "var(--accent-border)";
  const accentWash = danger ? "var(--err-wash)" : "var(--accent-wash)";
  const accentText = danger ? "var(--err)" : "var(--accent-text-wash)";
  const showHover = hover && interactive;
  const showHighlight = highlighted && interactive;
  const restBorder = showHighlight ? accentBorder : outline ? "var(--border-default)" : "transparent";
  const restBg = showHighlight ? accentWash : "transparent";
  const restColor = showHighlight ? accentText : "var(--fg-subtle)";
  return (
    <button
      type="button"
      onClick={onClick}
      disabled={disabled}
      title={tooltip}
      aria-label={ariaLabel}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: size,
        height: size,
        padding: 0,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        border: `1px solid ${showHover ? accentBorder : restBorder}`,
        borderRadius: "var(--radius-sm)",
        background: showHover ? accentWash : restBg,
        color: showHover ? accentText : restColor,
        cursor: interactive ? "pointer" : "not-allowed",
        opacity: interactive ? 1 : 0.4,
        flex: "none",
        transition: "background 120ms ease, border-color 120ms ease, color 120ms ease",
      }}
    >
      {icon}
    </button>
  );
}

/** Per-action palette: accent (default azure), warm (gold favorites),
 *  green (live watch), violet (plot/signal). */
export type IconTogglePalette = "accent" | "warm" | "green" | "violet";

const PALETTES: Record<IconTogglePalette, {
  wash: string;
  pressedBg: string;
  text: string;
}> = {
  accent: {
    wash: "var(--accent-wash)",
    pressedBg: "var(--accent-border)",
    text: "var(--accent-text-wash)",
  },
  warm: {
    wash: "var(--palette-warm-wash)",
    pressedBg: "var(--palette-warm-pressed)",
    text: "var(--palette-warm-text)",
  },
  green: {
    wash: "var(--palette-green-wash)",
    pressedBg: "var(--palette-green-pressed)",
    text: "var(--palette-green-text)",
  },
  violet: {
    wash: "var(--palette-violet-wash)",
    pressedBg: "var(--palette-violet-pressed)",
    text: "var(--palette-violet-text)",
  },
};

/** Hover and pressed both light the icon; bg distinguishes them (wash vs pressedBg). */
export function IconToggle({
  pressed,
  onClick,
  tooltip,
  icon,
  palette = "accent",
}: {
  pressed: boolean;
  onClick: () => void;
  tooltip: string;
  icon: React.ReactNode;
  palette?: IconTogglePalette;
}) {
  const [hover, setHover] = useState(false);
  const tones = PALETTES[palette];
  const background = pressed ? tones.pressedBg : hover ? tones.wash : "transparent";
  const color = pressed || hover ? tones.text : "var(--fg-subtle)";
  return (
    <button
      type="button"
      onClick={onClick}
      title={tooltip}
      aria-pressed={pressed}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: ICON_BUTTON_SIZE,
        height: ICON_BUTTON_SIZE,
        padding: 0,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        border: "none",
        borderRadius: "var(--radius-sm)",
        background,
        color,
        cursor: "pointer",
        flex: "none",
        transition: "background 120ms ease, color 120ms ease",
      }}
    >
      {icon}
    </button>
  );
}

/** Icon shows what the *next* click does: pause when live, play when paused. */
export function LiveToggleButton({
  on,
  disabled = false,
  onToggle,
  highlighted = false,
  size = ICON_BUTTON_SIZE,
}: {
  /** True when auto-following the live tail. */
  on: boolean;
  disabled?: boolean;
  onToggle: () => void;
  /** Paint the resting state with the accent chrome to call attention to the button
   *  (matches {@link HoverIconButton}'s `highlighted`). */
  highlighted?: boolean;
  size?: number;
}) {
  const [hover, setHover] = useState(false);
  const interactive = !disabled;
  const showHover = hover && interactive;
  const showHighlight = highlighted && interactive;
  const restBorder = showHighlight ? "var(--accent-border)" : "transparent";
  const restBg = showHighlight ? "var(--accent-wash)" : "transparent";
  const restColor = showHighlight
    ? "var(--accent-text-wash)"
    : on
      ? "var(--ok)"
      : "var(--fg-subtle)";
  return (
    <button
      type="button"
      onClick={onToggle}
      disabled={disabled}
      title={on ? "Pause" : "Resume live follow"}
      aria-label={on ? "pause live follow" : "resume live follow"}
      aria-pressed={on}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: size,
        height: size,
        padding: 0,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        border: `1px solid ${showHover ? "var(--accent-border)" : restBorder}`,
        borderRadius: "var(--radius-sm)",
        background: showHover ? "var(--accent-wash)" : restBg,
        color: showHover ? "var(--accent-text-wash)" : restColor,
        cursor: interactive ? "pointer" : "not-allowed",
        opacity: interactive ? 1 : 0.5,
        flex: "none",
        transition: "background 120ms ease, border-color 120ms ease, color 120ms ease",
      }}
    >
      {on ? <PauseIcon /> : <PlayIcon />}
    </button>
  );
}

/** Outline pill for destructive clear-all actions; neutral idle, red on hover. */
export function DangerPillButton({
  onClick,
  title,
  disabled = false,
  children = "Clear all",
}: {
  onClick: () => void;
  title: string;
  disabled?: boolean;
  children?: React.ReactNode;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      disabled={disabled}
      title={title}
      style={{
        padding: "2px 8px",
        fontFamily: "var(--font-ui)",
        fontSize: "var(--fs-xs)",
        color: "var(--fg-muted)",
        background: "transparent",
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-sm)",
        cursor: disabled ? "not-allowed" : "pointer",
        opacity: disabled ? 0.5 : 1,
        letterSpacing: "0.04em",
        textTransform: "uppercase",
        whiteSpace: "nowrap",
        transition: "color 120ms ease, border-color 120ms ease",
        flex: "none",
      }}
      onMouseEnter={(e) => {
        if (disabled) return;
        e.currentTarget.style.color = "var(--err)";
        e.currentTarget.style.borderColor = "var(--err)";
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.color = "var(--fg-muted)";
        e.currentTarget.style.borderColor = "var(--border-default)";
      }}
    >
      {children}
    </button>
  );
}

/** `?` icon with docstring tooltip; renders nothing when description is empty. */
export function HelpButton({
  description,
  ariaLabel,
  size = 20,
}: {
  description: string | null | undefined;
  ariaLabel: string;
  size?: number;
}) {
  if (!description || description.trim().length === 0) return null;
  return (
    <Tooltip content={description}>
      <HoverIconButton
        onClick={() => {
          /* Reserved for Type Explorer. */
        }}
        ariaLabel={ariaLabel}
        tooltip=""
        icon={<HelpIcon />}
        size={size}
      />
    </Tooltip>
  );
}
