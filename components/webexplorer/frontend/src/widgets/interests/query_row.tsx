// === QueryRow.tsx ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef, useState, type ReactNode } from "react";
import type * as React from "react";

import { useObjects } from "@sen/client/react";

import {
  useInterestByName,
  useInterestErrorByName,
} from "../../state/interest_registry.js";
import { QueryIcon } from "../../ui/icons.js";
import { Hint } from "./icons.js";
import { queryKey } from "../../core/keys.js";
import { interestNameFor } from "./bus_queries.js";
import { SqlTooltip } from "./popover/sql_tooltip.js";

interface QueryRowProps {
  sessionName: string;
  busName: string;
  queryName: string;
  queryString: string;
  onRemove: () => void;
  filterText: string;
  reportFilteredCount: (key: string, count: number) => void;
  // Drop the entry on unmount so the counts map doesn't accrue stale keys.
  clearFilteredCount: (key: string) => void;
  focused: boolean;
  onFocusConsumed: () => void;
  // Driven from parent state (not :nth-child) so zebra survives wrapper changes.
  rowIndex: number;
}

export function QueryRow({
  sessionName,
  busName,
  queryName,
  queryString,
  onRemove,
  filterText,
  reportFilteredCount,
  clearFilteredCount,
  focused,
  onFocusConsumed,
  rowIndex,
}: QueryRowProps) {
  const [hover, setHover] = useState(false);
  const interestName = interestNameFor(sessionName, busName, queryName);
  // Row is purely presentational; declareInterest lives in InterestOwner at App root,
  // so mount/unmount/display:none here doesn't touch backend state.
  const handle = useInterestByName(interestName);
  const error = useInterestErrorByName(interestName);
  const objects = useObjects(handle);

  const filtered = useMemo(() => {
    if (!filterText) return objects;
    const needle = filterText.toLowerCase();
    return objects.filter(
      (o) =>
        o.name.toLowerCase().includes(needle) ||
        o.className.toLowerCase().includes(needle),
    );
  }, [objects, filterText]);
  const totalCount = objects.length;
  const filteredCount = filtered.length;
  const filtering = filterText.length > 0 && totalCount !== filteredCount;
  const ownKey = queryKey(sessionName, busName, queryName);

  useEffect(() => {
    reportFilteredCount(ownKey, filteredCount);
  }, [ownKey, filteredCount, reportFilteredCount]);
  useEffect(() => {
    return () => clearFilteredCount(ownKey);
  }, [ownKey, clearFilteredCount]);

  const rowRef = useRef<HTMLDivElement | null>(null);
  useEffect(() => {
    if (!focused) return;
    rowRef.current?.scrollIntoView({ block: "nearest", behavior: "smooth" });
    const t = window.setTimeout(onFocusConsumed, 1500);
    return () => window.clearTimeout(t);
  }, [focused, onFocusConsumed]);

  // CSS-hide (not unmount) so the row keeps reporting counts when the filter changes.
  const hiddenByFilter = filterText.length > 0 && filteredCount === 0;

  const isStripe = rowIndex % 2 === 1;
  const rowBg = hover
    ? "var(--surface-row-hover)"
    : isStripe
      ? "var(--surface-zebra-strong)"
      : "var(--surface-zebra)";
  return (
    <div
      ref={rowRef}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        display: hiddenByFilter ? "none" : "block",
        background: rowBg,
        transition: "background 200ms ease-out",
      }}
    >
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 6,
          padding: "3px 4px 3px 0",
        }}
      >
        <QueryIcon color="rgb(214, 168, 100)" />
        <QueryLabel queryName={queryName} queryString={queryString} focused={focused} />
        <CountSpan total={totalCount} filtered={filteredCount} filtering={filtering} />
        <span style={{ flex: 1 }} />
        <MiniBtn
          danger
          onClick={(e) => {
            e.stopPropagation();
            onRemove();
          }}
          ariaLabel={`remove query ${queryName}`}
          tooltip="Remove query"
        >
          ✕
        </MiniBtn>
      </div>
      {error && (
        <Hint indent={1} color="var(--err)">
          {error.message}
        </Hint>
      )}
      {!error && !handle && <Hint indent={1}>opening...</Hint>}
    </div>
  );
}

function CountSpan({
  total,
  filtered,
  filtering,
}: {
  total: number;
  filtered: number;
  filtering: boolean;
}) {
  return (
    <span
      style={{
        fontSize: "var(--fs-sm)",
        color: filtering ? "var(--accent-text-wash)" : "var(--fg-subtle)",
        fontFamily: "var(--font-mono)",
        minWidth: 0,
        overflow: "hidden",
        flexShrink: 2,
      }}
      title={
        filtering
          ? `${filtered} match the search; ${total} match the interest`
          : `${total} objects match the interest`
      }
    >
      {filtering ? `(${filtered}/${total})` : `(${total})`}
    </span>
  );
}

function QueryLabel({
  queryName,
  queryString,
  focused,
}: {
  queryName: string;
  queryString: string;
  focused: boolean;
}) {
  // position:fixed + cursor-tracked coords; ellipsis + scroll ancestors would clip absolute.
  const [coords, setCoords] = useState<{ top: number; left: number } | null>(null);

  const trackCursor = (e: React.MouseEvent) => {
    const left = Math.max(8, Math.min(e.clientX + 12, window.innerWidth - 432));
    const estimatedHeight = 80;
    const wouldOverflow = e.clientY + 2 + estimatedHeight > window.innerHeight;
    const top = wouldOverflow ? Math.max(8, e.clientY - estimatedHeight - 4) : e.clientY + 2;
    setCoords({ top, left });
  };

  return (
    <span
      onMouseEnter={trackCursor}
      onMouseMove={trackCursor}
      onMouseLeave={() => setCoords(null)}
      style={{
        display: "inline-flex",
        alignItems: "baseline",
        gap: 0,
        fontFamily: "var(--font-mono)",
        whiteSpace: "nowrap",
        overflow: "hidden",
        textOverflow: "ellipsis",
        minWidth: 0,
        flexShrink: 0,
      }}
    >
      <span style={{ fontSize: "var(--fs-md)", color: "var(--fg-subtle)", marginRight: 2 }}>@</span>
      <span
        style={{
          fontSize: "var(--fs-lg)",
          color: "var(--accent-text-wash)",
          fontWeight: focused ? 600 : 500,
          background: focused ? "var(--accent-wash)" : "transparent",
          borderRadius: 2,
          padding: focused ? "0 3px" : 0,
          transition: "background 0.3s ease, padding 0.3s ease",
        }}
      >
        {queryName}
      </span>
      {coords && <SqlTooltip sql={queryString} top={coords.top} left={coords.left} />}
    </span>
  );
}

function MiniBtn({
  children,
  onClick,
  ariaLabel,
  tooltip,
  danger,
}: {
  children: ReactNode;
  onClick: (e: React.MouseEvent) => void;
  ariaLabel: string;
  tooltip?: string;
  danger?: boolean;
}) {
  const [hover, setHover] = useState(false);
  const accent = danger ? "var(--err)" : "var(--accent-text-wash)";
  const wash = danger ? "var(--err-wash, rgba(229,103,95,0.16))" : "var(--accent-wash)";
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      aria-label={ariaLabel}
      title={tooltip}
      style={{
        width: 24,
        height: 24,
        display: "grid",
        placeItems: "center",
        padding: 0,
        marginLeft: 2,
        background: hover ? wash : "transparent",
        border: "none",
        borderRadius: "var(--radius-sm)",
        cursor: "pointer",
        color: hover ? accent : "var(--fg-subtle)",
        transition: "background 0.12s ease, color 0.12s ease",
        flex: "none",
        fontSize: "var(--fs-lg)",
        lineHeight: 1,
        fontFamily: "var(--font-mono)",
      }}
    >
      {children}
    </button>
  );
}
