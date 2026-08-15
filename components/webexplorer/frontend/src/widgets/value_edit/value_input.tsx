// === ValueInput.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

import { shortName } from "../../core/format.js";
import { defaultFor } from "../../core/types.js";
import { Muted } from "../../ui/text_primitives.js";
import {
  BoolInput,
  EnumInput,
  NumberInput,
  OptionalInput,
  QuantityInput,
  StringInput,
} from "./primitive_inputs.js";
import { SequenceInput } from "./sequence_input.js";
import { StructInput } from "./struct_input.js";
import { VariantInput } from "./variant_input.js";

export interface ValueInputProps {
  client: Client | null;
  declaredType: string;
  value: Var;
  onChange: (next: Var) => void;
}

export function ValueInput({ client, declaredType, value, onChange }: ValueInputProps) {
  switch (declaredType) {
    case "bool":
      return (
        <BoolInput value={typeof value === "boolean" ? value : false} onChange={onChange} />
      );
    case "string":
      return (
        <StringInput value={typeof value === "string" ? value : ""} onChange={onChange} />
      );
    case "u8":
    case "u16":
    case "u32":
    case "u64":
    case "i8":
    case "i16":
    case "i32":
    case "i64":
      return (
        <NumberInput
          value={typeof value === "number" ? value : 0}
          integer
          onChange={onChange}
        />
      );
    case "f32":
    case "f64":
      return (
        <NumberInput
          value={typeof value === "number" ? value : 0}
          onChange={onChange}
        />
      );
    case "TimeStamp":
    case "Duration":
      return (
        <StringInput value={typeof value === "string" ? value : ""} onChange={onChange} />
      );
  }

  const spec = client?.getType(declaredType);
  if (!spec) {
    return <Muted>(unknown type {shortName(declaredType)})</Muted>;
  }

  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
      return (
        <EnumInput
          spec={spec.data.value}
          value={typeof value === "string" ? value : (spec.data.value.enums[0]?.name ?? "")}
          onChange={onChange}
        />
      );
    case "sen.kernel.QuantityTypeSpec":
      return (
        <QuantityInput
          value={
            value instanceof Quantity
              ? value
              : new Quantity(
                  typeof value === "number" ? value : 0,
                  spec.data.value.unit,
                  spec.data.value.minValue,
                  spec.data.value.maxValue,
                )
          }
          onChange={onChange}
        />
      );
    case "sen.kernel.AliasTypeSpec":
      return (
        <ValueInput
          client={client}
          declaredType={spec.data.value.aliasedType}
          value={value}
          onChange={onChange}
        />
      );
    case "sen.kernel.OptionalTypeSpec":
      return (
        <OptionalInput
          client={client}
          spec={spec.data.value}
          value={value}
          onChange={onChange}
        />
      );
    case "sen.kernel.SequenceTypeSpec":
      return (
        <SequenceInput
          client={client}
          spec={spec.data.value}
          value={Array.isArray(value) ? value : []}
          onChange={onChange}
        />
      );
    case "sen.kernel.StructTypeSpec":
      return (
        <StructInput
          client={client}
          spec={spec.data.value}
          value={
            value !== null && typeof value === "object" && !Array.isArray(value) && !(value instanceof Variant) && !(value instanceof Quantity)
              ? (value as Record<string, Var>)
              : (defaultFor(client, declaredType) as Record<string, Var>)
          }
          onChange={onChange}
        />
      );
    case "sen.kernel.VariantTypeSpec":
      return (
        <VariantInput
          client={client}
          spec={spec.data.value}
          value={value instanceof Variant ? value : (defaultFor(client, declaredType) as Variant)}
          onChange={onChange}
        />
      );
    case "sen.kernel.ClassTypeSpec":
      return <Muted>(class refs not editable)</Muted>;
  }
}
