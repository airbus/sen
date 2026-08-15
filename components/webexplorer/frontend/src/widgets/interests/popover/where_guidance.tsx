// === WhereGuidance.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useState, type ReactNode } from "react";

import { SQL_COMPARE_OPS, SQL_LOGICAL_OPS, SQL_PSEUDO_PROPS } from "./sql_helpers.js";

const MAX_PROP_CHIPS = 12;

export interface WhereMatch {
  token: string;
  kind: "pseudo" | "prop" | "op" | "logic";
}

// Flat ordered list (row 1 first, row 2 second) so arrow-key nav matches the visual order.
export function useWhereMatches(
  properties: string[] | null,
  currentWord: string,
): { items: WhereMatch[]; rowOneCount: number; truncated: number } {
  const q = currentWord.toLowerCase();
  const qU = currentWord.toUpperCase();

  const matchedPseudos = useMemo(
    () => (q === "" ? [...SQL_PSEUDO_PROPS] : SQL_PSEUDO_PROPS.filter((p) => p.startsWith(q))),
    [q],
  );
  const matchedProps = useMemo(() => {
    if (!properties) return [] as string[];
    const filtered = q === "" ? properties : properties.filter((p) => p.toLowerCase().startsWith(q));
    return filtered.slice(0, MAX_PROP_CHIPS);
  }, [properties, q]);
  const matchedKeywords = useMemo(() => {
    const kws = [...SQL_LOGICAL_OPS, "BETWEEN", "IN"];
    if (qU === "") return kws;
    return kws.filter((k) => k.startsWith(qU));
  }, [qU]);

  return useMemo(() => {
    const items: WhereMatch[] = [
      ...matchedPseudos.map((t) => ({ token: t, kind: "pseudo" as const })),
      ...matchedProps.map((t) => ({ token: t, kind: "prop" as const })),
      ...SQL_COMPARE_OPS.map((t) => ({ token: t, kind: "op" as const })),
      ...matchedKeywords.map((t) => ({
        token: t,
        kind: (t === "AND" || t === "OR" || t === "NOT" ? "logic" : "op") as "op" | "logic",
      })),
    ];
    const rowOneCount = matchedPseudos.length + matchedProps.length;
    const truncated = q === "" ? Math.max(0, (properties?.length ?? 0) - MAX_PROP_CHIPS) : 0;
    return { items, rowOneCount, truncated };
  }, [matchedPseudos, matchedProps, matchedKeywords, properties, q]);
}

export function WhereGuidance({
  className,
  properties,
  currentWord,
  matches,
  rowOneCount,
  truncated,
  focusedIdx,
  onPick,
}: {
  className: string;
  properties: string[] | null;
  currentWord: string;
  matches: WhereMatch[];
  rowOneCount: number;
  truncated: number;
  focusedIdx: number;
  onPick: (token: string) => void;
}) {
  const propsCached = properties !== null && properties.length > 0;
  const propsRowEmpty = rowOneCount === 0;
  const rowOne = matches.slice(0, rowOneCount);
  const rowTwo = matches.slice(rowOneCount);

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4, marginTop: 2 }}>
      <ChipRow label={propsCached ? `${className} props` : "props"}>
        {rowOne.map((m, i) =>
          m.kind === "pseudo" ? (
            <Chip
              key={m.token}
              kind="pseudo"
              onClick={() => onPick(m.token)}
              title={`Pseudo-property ${m.token}`}
              focused={i === focusedIdx}
            >
              {m.token}
            </Chip>
          ) : (
            <Chip
              key={m.token}
              onClick={() => onPick(m.token)}
              title={`Property ${m.token}`}
              focused={i === focusedIdx}
            >
              {m.token}
            </Chip>
          ),
        )}
        {truncated > 0 && (
          <Hintette>
            (+{truncated} more - keep typing to filter)
          </Hintette>
        )}
        {propsRowEmpty && !propsCached && (
          <Hintette>
            {className === "*" || className.trim() === ""
              ? "(pick a class to see its properties)"
              : `(spec for ${className} not cached yet)`}
          </Hintette>
        )}
        {propsRowEmpty && propsCached && (
          <Hintette>(no property matches "{currentWord}")</Hintette>
        )}
      </ChipRow>
      <ChipRow label="ops">
        {rowTwo.map((m, i) => (
          <Chip
            key={m.token}
            kind={m.kind === "logic" ? "logic" : "op"}
            onClick={() => onPick(m.token)}
            focused={rowOneCount + i === focusedIdx}
          >
            {m.token}
          </Chip>
        ))}
      </ChipRow>
    </div>
  );
}

export function ChipRow({ label, children }: { label?: string; children: ReactNode }) {
  return (
    <div style={{ display: "flex", flexWrap: "wrap", alignItems: "center", gap: 4 }}>
      {label && (
        <span
          style={{
            fontSize: "var(--fs-2xs)",
            textTransform: "uppercase",
            letterSpacing: "0.05em",
            color: "var(--fg-subtle)",
            marginRight: 2,
            flex: "none",
          }}
        >
          {label}
        </span>
      )}
      {children}
    </div>
  );
}

export function Chip({
  children,
  onClick,
  title,
  kind,
  focused = false,
}: {
  children: ReactNode;
  onClick: () => void;
  title?: string;
  kind?: "op" | "logic" | "pseudo";
  focused?: boolean;
}) {
  const [hover, setHover] = useState(false);
  const lit = hover || focused;
  // Kinds differ only in foreground weight so the chips read as one family.
  const fgRest = (() => {
    if (kind === "op") return "var(--fg-muted)";
    if (kind === "logic") return "var(--fg-base)";
    if (kind === "pseudo") return "var(--fg-subtle)";
    return "var(--fg-base)";
  })();
  const palette = {
    bg: lit ? "var(--accent-wash)" : "var(--bg-elevated)",
    fg: lit ? "var(--accent-text-wash)" : fgRest,
  };
  return (
    <button
      type="button"
      onMouseDown={(e) => {
        // mousedown (not click) so the input keeps focus across chained inserts.
        e.preventDefault();
        onClick();
      }}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={title}
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-sm)",
        padding: "0 6px",
        height: 18,
        lineHeight: "16px",
        background: palette.bg,
        color: palette.fg,
        border: `1px solid ${focused ? "var(--accent-border)" : "var(--border-default)"}`,
        borderRadius: "var(--radius-sm)",
        cursor: "pointer",
        transition: "background 0.12s, color 0.12s, border-color 0.12s",
        whiteSpace: "nowrap",
      }}
    >
      {children}
    </button>
  );
}

export function Hintette({ children }: { children: ReactNode }) {
  return (
    <span style={{ fontSize: "var(--fs-xs)", color: "var(--fg-subtle)", fontStyle: "italic" }}>{children}</span>
  );
}
