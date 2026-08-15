// === VariantInput.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useState } from "react";

import type { Client, Var, VariantTypeFieldSpec, VariantTypeSpec } from "@sen/client";
import { Variant } from "@sen/client";

import { shortName } from "../../core/format.js";
import { defaultFor, isComposite } from "../../core/types.js";
import { AdaptivePicker, type PickerItem } from "./pickers/adaptive_picker.js";
import { ValueInput } from "./value_input.js";

export function VariantInput({
  client,
  spec,
  value,
  onChange,
}: {
  client: Client | null;
  spec: VariantTypeSpec;
  value: Variant;
  onChange: (v: Variant) => void;
}) {
  // Cache per-arm payloads so flipping back restores prior input instead of resetting.
  const [armCache, setArmCache] = useState<Record<string, Var>>({});
  const items: PickerItem[] = useMemo(
    () =>
      spec.fields.map((f: VariantTypeFieldSpec) => ({
        name: f.type,
        label: shortName(f.type),
        description: f.type,
      })),
    [spec.fields],
  );
  const onArmChange = (next: string) => {
    if (next === value.type) return;
    setArmCache((prev) => ({ ...prev, [value.type]: value.value as Var }));
    const restored =
      armCache[next] !== undefined ? armCache[next]! : defaultFor(client, next);
    onChange(new Variant(next, restored));
  };
  const payloadIsBlock = isComposite(client, value.type);
  const innerEditor = (
    <ValueInput
      client={client}
      declaredType={value.type}
      value={value.value as Var}
      onChange={(next) => onChange(new Variant(value.type, next))}
    />
  );
  if (payloadIsBlock) {
    return (
      <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
        <span style={{ display: "inline-flex" }}>
          <AdaptivePicker items={items} value={value.type} onChange={onArmChange} />
        </span>
        <div>{innerEditor}</div>
      </div>
    );
  }
  return (
    <span style={{ display: "inline-flex", alignItems: "flex-start", gap: 8, flexWrap: "wrap" }}>
      <AdaptivePicker items={items} value={value.type} onChange={onArmChange} />
      {innerEditor}
    </span>
  );
}
