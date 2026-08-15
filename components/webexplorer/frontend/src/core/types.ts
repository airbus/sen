// === types.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, CustomTypeSpec, Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

// Mirrors `spec.data.type` plus `"primitive"` for built-ins without a CustomTypeSpec
// and `"unknown"` for declared types whose spec hasn't been cached yet.
export type Kind =
  | "primitive"
  | "enum"
  | "quantity"
  | "struct"
  | "sequence"
  | "variant"
  | "optional"
  | "alias"
  | "class"
  | "unknown";

const INTEGER_TYPES = new Set([
  "u8", "u16", "u32", "u64",
  "i8", "i16", "i32", "i64",
]);

const FLOAT_TYPES = new Set(["f32", "f64"]);

const PRIMITIVE_TYPES = new Set<string>([
  "bool",
  "string",
  "TimeStamp",
  "Duration",
  ...INTEGER_TYPES,
  ...FLOAT_TYPES,
]);

export function isIntegerType(declaredType: string): boolean {
  return INTEGER_TYPES.has(declaredType);
}

export function specOf(client: Client | null, declaredType: string): CustomTypeSpec | null {
  if (!client) return null;
  return client.getType(declaredType) ?? null;
}

/** Returns `"unknown"` when the spec isn't in the cache yet. */
export function kindOf(client: Client | null, declaredType: string): Kind {
  if (PRIMITIVE_TYPES.has(declaredType)) return "primitive";
  const spec = specOf(client, declaredType);
  if (!spec) return "unknown";
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec": return "enum";
    case "sen.kernel.QuantityTypeSpec": return "quantity";
    case "sen.kernel.StructTypeSpec": return "struct";
    case "sen.kernel.SequenceTypeSpec": return "sequence";
    case "sen.kernel.VariantTypeSpec": return "variant";
    case "sen.kernel.OptionalTypeSpec": return "optional";
    case "sen.kernel.AliasTypeSpec": return "alias";
    case "sen.kernel.ClassTypeSpec": return "class";
  }
  return "unknown";
}

/** Renders/edits as a block (struct, sequence, variant); aliases unwrap. */
export function isComposite(client: Client | null, declaredType: string): boolean {
  const k = kindOf(client, declaredType);
  if (k === "alias") {
    const spec = specOf(client, declaredType);
    if (spec && spec.data.type === "sen.kernel.AliasTypeSpec") {
      return isComposite(client, spec.data.value.aliasedType);
    }
    return false;
  }
  return k === "struct" || k === "sequence" || k === "variant";
}

/** Seeds new sequence elements, struct fields, variant arms, etc. Returns `null` for
 *  types whose default is meaningfully "absent" (optional, class, unknown). */
export function defaultFor(client: Client | null, declaredType: string): Var {
  if (declaredType === "bool") return false;
  if (declaredType === "string" || declaredType === "Duration" || declaredType === "TimeStamp") return "";
  if (INTEGER_TYPES.has(declaredType) || FLOAT_TYPES.has(declaredType)) return 0;
  const spec = specOf(client, declaredType);
  if (!spec) return null;
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
      return spec.data.value.enums[0]?.name ?? "";
    case "sen.kernel.QuantityTypeSpec":
      return new Quantity(
        0,
        spec.data.value.unit,
        spec.data.value.minValue,
        spec.data.value.maxValue,
      );
    case "sen.kernel.AliasTypeSpec":
      return defaultFor(client, spec.data.value.aliasedType);
    case "sen.kernel.OptionalTypeSpec":
      return null;
    case "sen.kernel.SequenceTypeSpec":
      return [];
    case "sen.kernel.StructTypeSpec": {
      const out: Record<string, Var> = {};
      for (const field of spec.data.value.fields) {
        out[field.name] = defaultFor(client, field.type);
      }
      return out;
    }
    case "sen.kernel.VariantTypeSpec": {
      const first = spec.data.value.fields[0];
      if (!first) return new Variant("", null);
      return new Variant(first.type, defaultFor(client, first.type));
    }
    case "sen.kernel.ClassTypeSpec":
      return null;
  }
  return null;
}
