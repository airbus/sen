// === popup_picker.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useState } from "react";
import type * as React from "react";
import {
  FloatingPortal,
  autoUpdate,
  flip,
  offset,
  shift,
  size,
  useDismiss,
  useFloating,
  useInteractions,
  useRole,
} from "@floating-ui/react";

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

  const MAX_HEIGHT = 280;

  // Floated and portalled, like Tooltip. Positioned absolutely, the list sat inside the pane that
  // scrolls above it and was cut at that pane's edge, so the options below the cut could not be
  // reached; a z-index does not escape an ancestor's overflow. `flip` moves the list above the
  // trigger when the room below is short, and `size` caps its height to the room that is left.
  const closeAndClear = (next: boolean) => {
    setOpen(next);
    if (!next) setQuery("");
  };
  const { refs, floatingStyles, context } = useFloating({
    open,
    onOpenChange: closeAndClear,
    placement: "bottom-start",
    middleware: [
      offset(2),
      flip({ padding: 6 }),
      shift({ padding: 6 }),
      size({
        padding: 6,
        apply({ availableHeight, elements }) {
          elements.floating.style.maxHeight = `${Math.min(MAX_HEIGHT, availableHeight)}px`;
        },
      }),
    ],
    whileElementsMounted: autoUpdate,
  });
  // Replaces a hand-written mousedown listener that closed on any click outside the trigger.
  // The trigger no longer contains the list, so that listener would have read a click on an
  // option as outside and closed the popup before the click reached it. useDismiss counts the
  // floating element as part of the widget.
  const dismiss = useDismiss(context);
  const role = useRole(context, { role: "listbox" });
  const { getReferenceProps, getFloatingProps } = useInteractions([dismiss, role]);

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
    <span style={{ display: "inline-block" }}>
      <button
        ref={refs.setReference}
        type="button"
        {...getReferenceProps({
          onClick: () => closeAndClear(!open),
          onKeyDown: onTriggerKeyDown,
          onMouseEnter: () => setTriggerHover(true),
          onMouseLeave: () => setTriggerHover(false),
        })}
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
        <FloatingPortal>
          <div
            ref={refs.setFloating}
            {...getFloatingProps({ onKeyDown: onPopupKeyDown })}
            style={{
              ...floatingStyles,
              minWidth: 180,
              maxHeight: MAX_HEIGHT,
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
        </FloatingPortal>
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
