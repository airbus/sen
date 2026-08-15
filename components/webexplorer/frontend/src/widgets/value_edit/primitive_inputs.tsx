// === primitive_inputs.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import RcInputNumber from "rc-input-number";
import { useEffect, useRef, useState } from "react";
import type * as React from "react";

import type { Client, EnumTypeSpec, OptionalTypeSpec, Var } from "@sen/client";
import { Quantity } from "@sen/client";

import { defaultFor } from "../../core/types.js";
import { formatUnitAbbreviation } from "../../state/unit_format.js";
import { Muted } from "../../ui/text_primitives.js";
import { baseInputStyle, spinnerButtonStyle, withinBounds } from "./_styles.js";
import { AdaptivePicker } from "./pickers/adaptive_picker.js";
import { ValueInput } from "./value_input.js";

export function BoolInput({ value, onChange }: { value: boolean; onChange: (b: boolean) => void }) {
  const TRACK_W = 32;
  const TRACK_H = 18;
  const KNOB_D = 14;
  return (
    <button
      type="button"
      role="switch"
      aria-checked={value}
      onClick={() => onChange(!value)}
      style={{
        width: TRACK_W,
        height: TRACK_H,
        padding: 0,
        borderRadius: TRACK_H / 2,
        border: `1px solid ${value ? "var(--accent)" : "var(--border-strong)"}`,
        background: value ? "var(--accent-gradient)" : "var(--border-emphasis)",
        cursor: "pointer",
        position: "relative",
        display: "inline-block",
        transition: "background 120ms ease, border-color 120ms ease",
        verticalAlign: "middle",
        flex: "none",
      }}
    >
      <span
        style={{
          width: KNOB_D,
          height: KNOB_D,
          borderRadius: "50%",
          background: "var(--bg-elevated)",
          display: "block",
          position: "absolute",
          top: (TRACK_H - KNOB_D) / 2 - 1,
          left: value ? TRACK_W - KNOB_D - 2 : 1,
          transition: "left 140ms ease",
          boxShadow: "0 1px 2px rgba(0, 0, 0, 0.35)",
        }}
      />
    </button>
  );
}

export function EnumInput({
  spec,
  value,
  onChange,
}: {
  spec: EnumTypeSpec;
  value: string;
  onChange: (v: string) => void;
}) {
  return <AdaptivePicker items={spec.enums} value={value} onChange={onChange} />;
}

// Delegates the text input to rc-input-number for intermediate-state typing ("0.").
export function NumberInput({
  value,
  integer,
  invalid,
  onChange,
}: {
  value: number;
  integer?: boolean;
  invalid?: boolean;
  onChange: (n: number) => void;
}) {
  const STEP = 1;
  const apply = (delta: number) => {
    const base = Number.isFinite(value) ? value : 0;
    const next = base + delta;
    onChange(integer ? Math.trunc(next) : next);
  };
  const borderColor = invalid ? "var(--warn)" : undefined;
  // Allow control keys, digits, one leading -, one decimal point, e/E/+ for scientific.
  const filterKeystroke = (e: React.KeyboardEvent<HTMLSpanElement>) => {
    const target = e.target;
    if (!(target instanceof HTMLInputElement)) return;
    const { key } = e;
    if (key.length > 1) return;
    if (e.ctrlKey || e.metaKey) return;
    const start = target.selectionStart ?? 0;
    const end = target.selectionEnd ?? 0;
    const remaining = target.value.slice(0, start) + target.value.slice(end);
    const blocked = (() => {
      if (key === "-") return start !== 0 || remaining.includes("-");
      if (key === ".") return integer || remaining.includes(".");
      if (key === "e" || key === "E") return integer || remaining.toLowerCase().includes("e");
      if (key === "+") {
        if (integer) return true;
        const ePos = remaining.toLowerCase().indexOf("e");
        return ePos === -1 || start !== ePos + 1;
      }
      return !/[0-9]/.test(key);
    })();
    if (blocked) e.preventDefault();
  };
  return (
    <span
      style={{ display: "inline-flex", alignItems: "stretch", gap: 0 }}
      onKeyDown={filterKeystroke}
    >
      <button
        type="button"
        onClick={(e) => apply(-STEP * (e.shiftKey ? 10 : 1))}
        title="Decrement (Shift = x10)"
        style={spinnerButtonStyle("left", borderColor)}
      >
        -
      </button>
      <RcInputNumber
        value={Number.isFinite(value) ? value : null}
        onChange={(v) => {
          if (typeof v === "number") onChange(integer ? Math.trunc(v) : v);
          else onChange(NaN);
        }}
        controls={false}
        // Float: omit `precision` so input isn't rounded; conditional spread satisfies
        // exactOptionalPropertyTypes.
        {...(integer ? { precision: 0 } : {})}
        className={`sen-number-input${invalid ? " sen-number-input--invalid" : ""}`}
        inputMode={integer ? "numeric" : "decimal"}
      />
      <button
        type="button"
        onClick={(e) => apply(STEP * (e.shiftKey ? 10 : 1))}
        title="Increment (Shift = x10)"
        style={spinnerButtonStyle("right", borderColor)}
      >
        +
      </button>
    </span>
  );
}

export function StringInput({ value, onChange }: { value: string; onChange: (s: string) => void }) {
  // Sticky one-way upgrade to textarea so the widget can't bounce mid-edit.
  const [isMultiline, setMultiline] = useState(
    () => value.includes("\n") || value.length > 60,
  );
  useEffect(() => {
    if (!isMultiline && (value.includes("\n") || value.length > 60)) {
      setMultiline(true);
    }
  }, [value, isMultiline]);
  if (isMultiline) {
    return <StringTextarea value={value} onChange={onChange} />;
  }
  return <StringSingleLine value={value} onChange={onChange} />;
}

function StringSingleLine({ value, onChange }: { value: string; onChange: (s: string) => void }) {
  return (
    <input
      type="text"
      value={value}
      onChange={(e) => onChange(e.target.value)}
      style={baseInputStyle({ width: 220 })}
    />
  );
}

function StringTextarea({ value, onChange }: { value: string; onChange: (s: string) => void }) {
  const ref = useRef<HTMLTextAreaElement | null>(null);
  // Cap at 240px so a huge paste can't blow out the row.
  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    el.style.height = "auto";
    el.style.height = `${Math.min(el.scrollHeight, 240)}px`;
  }, [value]);
  // Plain Enter is a newline; Cmd/Ctrl+Enter is the Apply gesture.
  const onKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key !== "Enter") return;
    if (e.metaKey || e.ctrlKey) {
      e.preventDefault();
      return;
    }
    e.stopPropagation();
  };
  return (
    <textarea
      ref={ref}
      value={value}
      onChange={(e) => onChange(e.target.value)}
      onKeyDown={onKeyDown}
      title="Cmd/Ctrl+Enter to apply"
      rows={1}
      style={baseInputStyle({
        width: 320,
        minHeight: 28,
        resize: "vertical",
        overflow: "auto",
        lineHeight: 1.4,
      })}
    />
  );
}

export function QuantityInput({
  value,
  onChange,
}: {
  value: Quantity;
  onChange: (q: Quantity) => void;
}) {
  const inBounds = withinBounds(value.value, value.minValue, value.maxValue);
  return (
    <span style={{ display: "inline-flex", alignItems: "center", gap: 6 }}>
      <NumberInput
        value={value.value}
        invalid={!inBounds}
        onChange={(n) =>
          onChange(new Quantity(n, value.unit, value.minValue, value.maxValue))
        }
      />
      <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-subtle)", fontFamily: "var(--font-mono)" }}>
        {formatUnitAbbreviation(value.unit.abbreviation) || value.unit.name || ""}
      </span>
    </span>
  );
}

export function OptionalInput({
  client,
  spec,
  value,
  onChange,
}: {
  client: Client | null;
  spec: OptionalTypeSpec;
  value: Var;
  onChange: (v: Var) => void;
}) {
  const present = value !== null && value !== undefined;
  return (
    <span style={{ display: "inline-flex", alignItems: "center", gap: 8, flexWrap: "wrap" }}>
      <BoolInput
        value={present}
        onChange={(next) => {
          if (next) onChange(defaultFor(client, spec.type));
          else onChange(null);
        }}
      />
      {present ? (
        <ValueInput client={client} declaredType={spec.type} value={value} onChange={onChange} />
      ) : (
        <Muted>none</Muted>
      )}
    </span>
  );
}
