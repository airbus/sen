// === narrowing.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Quantity, Variant, type Var, type VarMap } from "./values.js";

// Type guards for walking a `Var` of unknown shape. Use these instead of structural checks
// at call sites; `Quantity` and `Variant` would otherwise look like plain objects.

/** True if `v` is a {@link Quantity}. */
export function isQuantity(v: Var): v is Quantity {
  return v instanceof Quantity;
}

/** True if `v` is a {@link Variant}. */
export function isVariant(v: Var): v is Variant {
  return v instanceof Variant;
}

/** True if `v` is a plain struct (object with string keys, not a sequence, Quantity, or Variant). */
export function isStruct(v: Var): v is VarMap {
  return (
    typeof v === "object" &&
    v !== null &&
    !Array.isArray(v) &&
    !(v instanceof Quantity) &&
    !(v instanceof Variant)
  );
}

/** True if `v` is a sequence (array). */
export function isSequence(v: Var): v is Var[] {
  return Array.isArray(v);
}

/** True if `v` is a primitive: `string`, `number`, `boolean`, or `null`. */
export function isPrimitive(v: Var): v is string | number | boolean | null {
  return v === null || typeof v === "string" || typeof v === "number" || typeof v === "boolean";
}
