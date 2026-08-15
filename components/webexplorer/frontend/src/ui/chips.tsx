// === chips.tsx =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, CustomTypeSpec } from "@sen/client";

import { useTypeCacheTick } from "../state/class_members.js";
import { classChipStyle } from "./class_color.js";
import { LeftTruncated } from "./text_primitives.js";

export type ChipSize = "sm" | "md";

/** Mirrors the Sen type-spec discriminator plus primitive buckets without spec entries.
 *  `unknown` is the fallback before the spec is cached. */
export type TypeChipKind =
  | "class"
  | "variant"
  | "struct"
  | "enum"
  | "sequence"
  | "quantity"
  | "optional"
  | "alias"
  | "numeric"
  | "boolean"
  | "string"
  | "time"
  | "unknown";

const BUILTIN_NUMERIC = new Set([
  "u8", "u16", "u32", "u64",
  "i8", "i16", "i32", "i64",
  "f32", "f64",
]);

/** Primitives short-circuit so a cache miss for `f64` / `bool` doesn't matter. */
export function typeChipKind(client: Client | null, typeName: string): TypeChipKind {
  if (BUILTIN_NUMERIC.has(typeName)) return "numeric";
  if (typeName === "bool") return "boolean";
  if (typeName === "string") return "string";
  if (typeName === "TimeStamp" || typeName === "Duration") return "time";
  const spec = client?.getType(typeName) as CustomTypeSpec | undefined;
  if (!spec) return "unknown";
  switch (spec.data.type) {
    case "sen.kernel.ClassTypeSpec": return "class";
    case "sen.kernel.VariantTypeSpec": return "variant";
    case "sen.kernel.StructTypeSpec": return "struct";
    case "sen.kernel.EnumTypeSpec": return "enum";
    case "sen.kernel.SequenceTypeSpec": return "sequence";
    case "sen.kernel.QuantityTypeSpec": return "quantity";
    case "sen.kernel.OptionalTypeSpec": return "optional";
    case "sen.kernel.AliasTypeSpec": return "alias";
  }
  return "unknown";
}

interface KindStyle {
  bg: string;
  border: string;
  fg: string;
}

/** Tinted backgrounds + low-weight borders + desaturated fg so chips don't compete with
 *  surrounding text. Hues group kinds: objects blue, sum types purple, products mint,
 *  enums amber, sequences green, neutrals grey. */
const KIND_STYLES: Record<TypeChipKind, KindStyle> = {
  class:     { bg: "rgba(79, 147, 255, 0.07)",  border: "rgba(79, 147, 255, 0.26)",  fg: "#7e9dcc" },
  variant:   { bg: "rgba(198, 120, 221, 0.07)", border: "rgba(198, 120, 221, 0.26)", fg: "#b289c0" },
  struct:    { bg: "rgba(127, 214, 199, 0.06)", border: "rgba(127, 214, 199, 0.24)", fg: "#88b8ad" },
  enum:      { bg: "rgba(214, 168, 94, 0.07)",  border: "rgba(214, 168, 94, 0.26)",  fg: "#b89c70" },
  sequence:  { bg: "rgba(140, 210, 120, 0.06)", border: "rgba(140, 210, 120, 0.24)", fg: "#94b585" },
  quantity:  { bg: "rgba(207, 234, 217, 0.05)", border: "rgba(207, 234, 217, 0.22)", fg: "#a8bdb1" },
  numeric:   { bg: "rgba(207, 234, 217, 0.05)", border: "rgba(207, 234, 217, 0.22)", fg: "#a8bdb1" },
  boolean:   { bg: "rgba(127, 214, 199, 0.06)", border: "rgba(127, 214, 199, 0.24)", fg: "#88b8ad" },
  string:    { bg: "rgba(150, 165, 195, 0.05)", border: "rgba(150, 165, 195, 0.22)", fg: "#9aa3b3" },
  time:      { bg: "rgba(150, 165, 195, 0.05)", border: "rgba(150, 165, 195, 0.22)", fg: "#9aa3b3" },
  optional:  { bg: "rgba(150, 165, 195, 0.05)", border: "rgba(150, 165, 195, 0.22)", fg: "#9aa3b3" },
  alias:     { bg: "rgba(150, 165, 195, 0.05)", border: "rgba(150, 165, 195, 0.22)", fg: "#9aa3b3" },
  unknown:   { bg: "transparent",               border: "var(--border-default)",     fg: "var(--fg-subtle)" },
};

/** Class kinds get a per-class palette from the same HSL as the card border swatch. */
export function TypeChip({
  client,
  type,
  title,
  leftTruncate = false,
  size = "sm",
}: {
  client: Client | null;
  type: string;
  /** Override the auto-generated `"<kind> <type>"` tooltip. */
  title?: string;
  /** Truncate left so the rightmost segment stays visible when the chip shrinks. */
  leftTruncate?: boolean;
  /** `"sm"` (in-card) or `"md"` (section heading). */
  size?: ChipSize;
}) {
  // Re-render when the type cache learns new specs; otherwise a chip mounted before its
  // spec arrived stays "unknown" forever.
  useTypeCacheTick(client);
  const kind = typeChipKind(client, type);
  const style = kind === "class" ? classChipStyle(type) : KIND_STYLES[kind];
  const isMd = size === "md";
  return (
    <span
      title={title ?? `${kind} ${type}`}
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: isMd ? "var(--fs-md)" : "var(--fs-xs)",
        color: style.fg,
        background: style.bg,
        border: `1px solid ${style.border}`,
        padding: isMd ? "2px 9px" : "0 5px",
        borderRadius: "var(--radius-sm)",
        whiteSpace: "nowrap",
        display: "inline-block",
        lineHeight: 1.55,
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
        // Left-truncation mode yields space to siblings rather than wrapping.
        ...(leftTruncate
          ? { minWidth: 0, flex: "0 1 auto", overflow: "hidden" }
          : {}),
      }}
    >
      {leftTruncate ? (
        <LeftTruncated text={type} style={{ color: style.fg }} />
      ) : (
        type
      )}
    </span>
  );
}

/** `session.bus` chip; neutral palette since the bus isn't class-coded. */
export function BusChip({
  sessionName,
  busName,
  size = "sm",
}: {
  sessionName: string;
  busName: string;
  size?: ChipSize;
}) {
  const isMd = size === "md";
  return (
    <span
      title={`bus ${sessionName}.${busName}`}
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: isMd ? "var(--fs-md)" : "var(--fs-xs)",
        color: "var(--fg-subtle)",
        background: "rgba(150, 165, 195, 0.05)",
        border: "1px solid rgba(150, 165, 195, 0.22)",
        padding: isMd ? "2px 9px" : "0 5px",
        borderRadius: "var(--radius-sm)",
        whiteSpace: "nowrap",
        display: "inline-block",
        lineHeight: 1.55,
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
      }}
    >
      {sessionName}.{busName}
    </span>
  );
}
