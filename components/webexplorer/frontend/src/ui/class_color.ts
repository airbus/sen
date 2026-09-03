// === class_color.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/**
 * Deterministic className -> HSL swatch for the Overview workspace. Stability across
 * reloads/users is load-bearing; see `classHSL` salts.
 */

const PALETTE_HUES = [0, 36, 72, 108, 144, 180, 216, 252, 288, 324] as const;

export interface ClassSwatch {
  /** At rest; low alpha so cards read as siblings, not warnings. */
  border: string;
  /** On hover; higher alpha + slightly brighter. */
  borderHover: string;
  /** Mid-alpha left identity stripe; carries identity without competing with card data. */
  accent: string;
  /** Full-opacity ring for the explorer-mirrors-this selected card. */
  accentSolid: string;
}

const NEUTRAL: ClassSwatch = {
  border: "var(--border-default)",
  borderHover: "var(--border-strong)",
  accent: "var(--fg-muted)",
  accentSolid: "var(--fg-base)",
};

/** FNV-1a 32-bit; determinism + reasonable distribution, no crypto strength needed. */
function fnv1a(s: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 0x01000193);
  }
  return h >>> 0;
}

/** PERSISTED IDENTITY: the salt strings (`!H`, `!L`, `!S`) and the FNV-1a constants
 *  are load-bearing across reloads and saved cockpits. Changing any of them re-shuffles
 *  every user's class colors. `class_color.test.ts` pins one output so changes fail CI. */
function classHSL(className: string): { hue: number; L: number; S: number } {
  const baseHue = PALETTE_HUES[fnv1a(className) % PALETTE_HUES.length] ?? 0;
  const hVariant = fnv1a(`${className}!H`) % 21;
  const lVariant = fnv1a(`${className}!L`) % 31;
  const sVariant = fnv1a(`${className}!S`) % 25;
  const hDelta = hVariant - 10; // -10 .. +10 (palette stops are 36° apart)
  const lDelta = lVariant - 15; // -15 .. +15
  const sDelta = sVariant - 12; // -12 .. +12
  return {
    hue: (baseHue + hDelta + 360) % 360,
    L: 62 + lDelta,
    S: 62 + sDelta,
  };
}

/** No parent linkage; Overview groups by *position* (sort + spacer), so we maximise
 *  per-class distinguishability instead. ~7,750 hue/L/S combos; collisions rare. */
export function classSwatch(className: string): ClassSwatch {
  if (!className) return NEUTRAL;
  const { hue, L, S } = classHSL(className);
  return {
    border: `hsla(${hue}, ${S}%, ${L}%, 0.46)`,
    borderHover: `hsla(${hue}, ${S + 10}%, ${L + 6}%, 0.82)`,
    // hsla (not hsl) so the 4px left strip stays translucent like the card body; a
    // fully-opaque band reads as a hard rail painted *over* the glass.
    accent: `hsla(${hue}, ${S + 8}%, ${L + 6}%, 0.55)`,
    // Fully-opaque so the selected-state ring pops against the 0.55-alpha resting band.
    accentSolid: `hsla(${hue}, ${S + 14}%, ${L + 10}%, 1)`,
  };
}

/** Same hue/L/S as the card swatch so chip and card stay visually paired. */
export interface ClassChipStyle {
  bg: string;
  border: string;
  fg: string;
}

/** Matches the generic blue "class" entry in `KIND_STYLES`. */
const NEUTRAL_CHIP: ClassChipStyle = {
  bg: "rgba(79, 147, 255, 0.07)",
  border: "rgba(79, 147, 255, 0.26)",
  fg: "#7e9dcc",
};

export function classChipStyle(className: string): ClassChipStyle {
  if (!className) return NEUTRAL_CHIP;
  const { hue, L, S } = classHSL(className);
  return {
    bg: `hsla(${hue}, ${S}%, ${L}%, 0.10)`,
    border: `hsla(${hue}, ${S}%, ${L}%, 0.32)`,
    // Clamp lightness so darker hues stay readable on the dark gradient base.
    fg: `hsl(${hue}, ${Math.max(20, S - 6)}%, ${Math.max(52, L - 4)}%)`,
  };
}
