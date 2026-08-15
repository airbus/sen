// === StructInput.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Fragment, useState } from "react";

import type { Client, StructTypeFieldSpec, StructTypeSpec, Var } from "@sen/client";

import { defaultFor, isComposite } from "../../core/types.js";
import { HoverIconButton } from "../../ui/buttons.js";
import { ChevronDownIcon, ChevronRightIcon, ResetIcon } from "../../ui/icons.js";
import { defaultEquals } from "./field_writer.js";
import { ValueInput } from "./value_input.js";

export function StructInput({
  client,
  spec,
  value,
  onChange,
}: {
  client: Client | null;
  spec: StructTypeSpec;
  value: Record<string, Var>;
  onChange: (v: Record<string, Var>) => void;
}) {
  const [collapsed, setCollapsed] = useState<ReadonlySet<string>>(new Set());
  const toggleCollapsed = (name: string) =>
    setCollapsed((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  return (
    <div
      style={{
        marginLeft: 2,
        paddingLeft: 8,
        borderLeft: "1px solid var(--border-default)",
        display: "grid",
        // Tracks: name | value (shrinkable) | reset. Block fields span all columns.
        gridTemplateColumns: "auto minmax(0, 1fr) auto",
        columnGap: 6,
        rowGap: 4,
        alignItems: "baseline",
      }}
    >
      {spec.fields.map((field: StructTypeFieldSpec) => {
        const fieldValue = value[field.name] ?? defaultFor(client, field.type);
        const defaultVal = defaultFor(client, field.type);
        const isDefault = defaultEquals(fieldValue, defaultVal);
        const block = isComposite(client, field.type);
        const isCollapsed = collapsed.has(field.name);
        const resetButton = !isDefault ? (
          <HoverIconButton
            onClick={() => onChange({ ...value, [field.name]: defaultVal })}
            ariaLabel={`reset ${field.name} to default`}
            tooltip="Reset to default"
            icon={<ResetIcon />}
            size={20}
          />
        ) : null;
        if (block) {
          return (
            <Fragment key={field.name}>
              <div
                style={{
                  gridColumn: "1 / -1",
                  display: "flex",
                  alignItems: "center",
                  gap: 6,
                }}
              >
                <span
                  onClick={() => toggleCollapsed(field.name)}
                  role="button"
                  aria-expanded={!isCollapsed}
                  style={{
                    cursor: "pointer",
                    display: "inline-flex",
                    alignItems: "center",
                    gap: 6,
                  }}
                >
                  <span
                    style={{
                      width: 11,
                      height: 11,
                      display: "inline-flex",
                      alignItems: "center",
                      justifyContent: "center",
                      color: "var(--fg-subtle)",
                    }}
                  >
                    {isCollapsed ? <ChevronRightIcon /> : <ChevronDownIcon />}
                  </span>
                  <span
                    style={{
                      fontFamily: "var(--font-mono)",
                      fontSize: "var(--fs-md)",
                      color: "var(--fg-muted)",
                    }}
                  >
                    {field.name}
                  </span>
                </span>
                {resetButton && <span style={{ marginLeft: "auto" }}>{resetButton}</span>}
              </div>
              {!isCollapsed && (
                <div style={{ gridColumn: "1 / -1" }}>
                  <ValueInput
                    client={client}
                    declaredType={field.type}
                    value={fieldValue}
                    onChange={(next) => onChange({ ...value, [field.name]: next })}
                  />
                </div>
              )}
            </Fragment>
          );
        }
        return (
          <Fragment key={field.name}>
            <span
              style={{
                fontFamily: "var(--font-mono)",
                fontSize: "var(--fs-md)",
                color: "var(--fg-muted)",
                whiteSpace: "nowrap",
              }}
            >
              {field.name}
            </span>
            <span style={{ display: "flex", minWidth: 0 }}>
              <ValueInput
                client={client}
                declaredType={field.type}
                value={fieldValue}
                onChange={(next) => onChange({ ...value, [field.name]: next })}
              />
            </span>
            <span>{resetButton}</span>
          </Fragment>
        );
      })}
    </div>
  );
}
