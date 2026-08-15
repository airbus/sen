// === primitive_values.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, Quantity } from "@sen/client";

import { formatNumber, formatTimestamp } from "../../core/format.js";
import { isIntegerType } from "../../core/types.js";
import { formatUnitAbbreviation } from "../../state/unit_format.js";
import { Muted } from "../../ui/text_primitives.js";

export function NumberCell({
  value,
  integer = false,
}: {
  value: number;
  integer?: boolean | undefined;
}) {
  const fmt = formatNumber(value, { integer });
  return (
    <span
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-lg)",
        color: "var(--val-num)",
        display: "inline-block",
        minWidth: 36,
        textAlign: "right",
        fontVariantNumeric: "tabular-nums",
      }}
    >
      {fmt}
    </span>
  );
}

// Quote only when declaredType is exactly "string" so identifier-like strings stay bare.
export function PlainStringValue({
  value,
  declaredType,
}: {
  value: string;
  declaredType: string;
}) {
  const isPlainString = declaredType === "string";
  return (
    <span style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--val-str)" }}>
      {isPlainString ? `"${value}"` : value}
    </span>
  );
}

// Display drops the date; full UTC string in the title.
export function TimeStampValue({ value }: { value: string }) {
  return (
    <span
      style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--val-str)" }}
      title={value + " UTC"}
    >
      {formatTimestamp(value)}
    </span>
  );
}

export function EnumValue({ value }: { value: string }) {
  return (
    <span
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-md)",
        color: "var(--val-enum)",
        background: "var(--bg-elevated)",
        border: "1px solid var(--border-default)",
        padding: "1px 7px",
        borderRadius: "var(--radius-sm)",
      }}
    >
      {value}
    </span>
  );
}

export function QuantityValue({ q }: { q: Quantity }) {
  const unit = formatUnitAbbreviation(q.unit.abbreviation) || q.unit.name || "";
  return (
    <span style={{ display: "flex", alignItems: "center", gap: 6 }}>
      <NumberCell value={q.value} />
      {unit && (
        <span
          style={{
            fontSize: "var(--fs-sm)",
            color: "var(--fg-subtle)",
            fontFamily: "var(--font-mono)",
            flex: "none",
          }}
        >
          {unit}
        </span>
      )}
    </span>
  );
}

export function PrimitiveValue({
  declaredType,
  value,
  client,
}: {
  declaredType: string;
  value: string | number | boolean | null;
  client: Client | null;
}) {
  if (typeof value === "boolean") {
    return (
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-md)",
          color: value ? "var(--val-bool)" : "var(--fg-muted)",
        }}
      >
        {value ? "true" : "false"}
      </span>
    );
  }
  if (typeof value === "number") {
    return <NumberCell value={value} integer={isIntegerType(declaredType)} />;
  }
  if (typeof value === "string") {
    const spec = client?.getType(declaredType);
    const isEnum = spec?.data.type === "sen.kernel.EnumTypeSpec";
    const isTimeStamp = declaredType === "TimeStamp" || declaredType === "sen.TimeStamp";
    if (isEnum) return <EnumValue value={value} />;
    if (isTimeStamp) return <TimeStampValue value={value} />;
    return <PlainStringValue value={value} declaredType={declaredType} />;
  }
  return <Muted>none</Muted>;
}
