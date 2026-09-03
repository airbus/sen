// === PopupPicker.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";

import { baseInputStyle } from "../_styles.js";
import type { PickerItem } from "./adaptive_picker.js";

export function PopupPicker({
  items,
  value,
  onChange,
  searchable,
}: {
  items: readonly PickerItem[];
  value: string;
  onChange: (v: string) => void;
  searchable: boolean;
}) {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState("");
  const wrapRef = useRef<HTMLSpanElement | null>(null);

  useEffect(() => {
    if (!open) return;
    const onClick = (e: MouseEvent) => {
      if (wrapRef.current && !wrapRef.current.contains(e.target as Node)) {
        setOpen(false);
        setQuery("");
      }
    };
    window.addEventListener("mousedown", onClick);
    return () => window.removeEventListener("mousedown", onClick);
  }, [open]);

  const filtered: readonly PickerItem[] = useMemo(() => {
    if (!searchable || !query) return items;
    const q = query.toLowerCase();
    return items.filter(
      (it) =>
        it.name.toLowerCase().includes(q) ||
        (it.label ?? "").toLowerCase().includes(q),
    );
  }, [items, query, searchable]);

  const activeLabel = useMemo(
    () => items.find((it) => it.name === value)?.label ?? value,
    [items, value],
  );

  // Stop Esc bubble (FieldWriter would cancel) and suppress Enter (reserved for Apply).
  const onTriggerKeyDown = (e: React.KeyboardEvent<HTMLButtonElement>) => {
    if (e.key === "Enter") {
      e.preventDefault();
    } else if (e.key === " " || e.key === "ArrowDown" || e.key === "ArrowUp") {
      e.preventDefault();
      setOpen(true);
    } else if (e.key === "Escape" && open) {
      e.preventDefault();
      e.stopPropagation();
      setOpen(false);
      setQuery("");
    }
  };
  const onPopupKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    if (e.key === "Escape") {
      e.preventDefault();
      e.stopPropagation();
      setOpen(false);
      setQuery("");
    }
  };

  const [triggerHover, setTriggerHover] = useState(false);
  const triggerBorder = open || triggerHover ? "var(--accent)" : "var(--accent-border)";
  return (
    <span ref={wrapRef} style={{ position: "relative", display: "inline-block" }}>
      <button
        type="button"
        onClick={() => setOpen((o) => !o)}
        onKeyDown={onTriggerKeyDown}
        onMouseEnter={() => setTriggerHover(true)}
        onMouseLeave={() => setTriggerHover(false)}
        style={baseInputStyle({
          minWidth: 140,
          display: "inline-flex",
          alignItems: "center",
          gap: 6,
          cursor: "pointer",
          textAlign: "left",
          borderColor: triggerBorder,
          transition: "border-color 120ms ease",
        })}
      >
        <span style={{ flex: "1 1 auto" }}>
          {activeLabel || <em style={{ color: "var(--fg-subtle)" }}>-</em>}
        </span>
        <span style={{ color: "var(--fg-subtle)", fontSize: "var(--fs-xs)" }}>▾</span>
      </button>
      {open && (
        <div
          onKeyDown={onPopupKeyDown}
          style={{
            position: "absolute",
            top: "100%",
            left: 0,
            marginTop: 2,
            minWidth: 180,
            maxHeight: 280,
            overflow: "auto",
            background: "var(--bg-elevated)",
            border: "1px solid var(--border-default)",
            borderRadius: "var(--radius-sm)",
            boxShadow: "0 4px 12px rgba(0, 0, 0, 0.4)",
            zIndex: 100,
          }}
        >
          {searchable && (
            <input
              type="text"
              autoFocus
              placeholder="Search..."
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              style={baseInputStyle({
                width: "100%",
                padding: "4px 8px",
                borderTop: "none",
                borderLeft: "none",
                borderRight: "none",
                borderRadius: 0,
                background: "var(--bg-input)",
              })}
            />
          )}
          {filtered.map((item) => (
            <PopupPickerItem
              key={item.name}
              label={item.label ?? item.name}
              description={item.description}
              active={item.name === value}
              onClick={() => {
                onChange(item.name);
                setOpen(false);
                setQuery("");
              }}
            />
          ))}
          {filtered.length === 0 && (
            <div style={{ padding: "6px 10px", color: "var(--fg-muted)", fontSize: "var(--fs-sm)" }}>
              no matches
            </div>
          )}
        </div>
      )}
    </span>
  );
}

function PopupPickerItem({
  label,
  description,
  active,
  onClick,
}: {
  label: string;
  description: string;
  active: boolean;
  onClick: () => void;
}) {
  const [hover, setHover] = useState(false);
  const background = active
    ? "var(--accent-wash)"
    : hover
      ? "var(--bg-input)"
      : "transparent";
  const color = active ? "var(--accent-text-wash)" : "var(--fg-base)";
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={description || undefined}
      style={{
        width: "100%",
        textAlign: "left",
        padding: "4px 10px",
        fontSize: "var(--fs-md)",
        fontFamily: "var(--font-mono)",
        border: "none",
        background,
        color,
        cursor: "pointer",
        display: "block",
        transition: "background 100ms ease",
      }}
    >
      {label}
    </button>
  );
}
