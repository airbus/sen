// === ValueRenderer.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, Var } from "@sen/client";
import {
  isPrimitive,
  isQuantity,
  isSequence,
  isStruct,
  isVariant,
} from "@sen/client";

import { Mono, Muted } from "../../ui/text_primitives.js";
import { PrimitiveValue, QuantityValue } from "./primitive_values.js";
import { SequenceValue } from "./sequence_value.js";
import { StructValue } from "./struct_value.js";
import { VariantValue } from "./variant_value.js";
import type { RenderLeafActions } from "./leaf_actions.js";

export interface ValueRendererProps {
  client: Client | null;
  declaredType: string;
  value: Var | undefined;
  // Dot/bracket path from the root watched value; composites extend on recurse.
  path?: string;
  renderLeafActions?: RenderLeafActions | undefined;
}

export function ValueRenderer({
  client,
  declaredType,
  value,
  path = "",
  renderLeafActions,
}: ValueRendererProps) {
  if (value === undefined) return <Muted>--</Muted>;
  // Render null explicitly so users see "absent" instead of "0".
  if (value === null) return <Muted>none</Muted>;

  if (isQuantity(value)) {
    return <QuantityValue q={value} />;
  }
  if (isVariant(value)) {
    return (
      <VariantValue
        client={client}
        v={value}
        path={path}
        renderLeafActions={renderLeafActions}
      />
    );
  }
  if (isPrimitive(value)) {
    return (
      <PrimitiveValue
        declaredType={declaredType}
        value={value}
        client={client}
      />
    );
  }
  if (isSequence(value)) {
    return (
      <SequenceValue
        client={client}
        value={value}
        declaredType={declaredType}
        path={path}
        renderLeafActions={renderLeafActions}
      />
    );
  }
  if (isStruct(value)) {
    return (
      <StructValue
        client={client}
        value={value}
        declaredType={declaredType}
        path={path}
        renderLeafActions={renderLeafActions}
      />
    );
  }
  return <Mono>{JSON.stringify(value)}</Mono>;
}
