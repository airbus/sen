// === var_codec.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { TransportError } from "../errors.js";
import { Quantity, Variant, type Var } from "../values.js";
import type {
  ClassTypeSpec,
  CustomTypeSpec,
  EnumTypeSpec,
  OptionalTypeSpec,
  QuantityTypeSpec,
  SequenceTypeSpec,
  StructTypeSpec,
  VariantTypeFieldSpec,
  VariantTypeSpec,
} from "../generated/index.js";
import type { TypeCache } from "./type_cache.js";

// -- shared: built-in primitive type tags and helpers --------------------------------------------

const builtInScalarTypes = new Set<string>([
  "bool",
  "string",
  "i8",
  "i16",
  "i32",
  "i64",
  "u8",
  "u16",
  "u32",
  "u64",
  "f32",
  "f64",
  // TimeStamp arrives as an RFC-3339 ns string; Duration as a decimal-string nanosecond count.
  "TimeStamp",
  "Duration",
]);

const stringBuiltIns = new Set(["string", "TimeStamp", "Duration"]);
// `i64`/`u64` cross the wire as JSON decimal strings to preserve precision above
// `Number.MAX_SAFE_INTEGER`; consumers see `bigint`. Smaller integers + floats stay `number`.
const numberBuiltIns = new Set(["i8", "i16", "i32", "u8", "u16", "u32", "f32", "f64"]);
const bigintBuiltIns = new Set(["i64", "u64"]);

interface FieldRef {
  name: string;
  type: string;
}

// `findArm` is purely a function of the spec (no cache reads), so a global WeakMap is safe.
// `collectStructFields` reads parent specs from the cache, so its memo lives on the cache.
const variantArmsCache = new WeakMap<VariantTypeSpec, Map<string, VariantTypeFieldSpec>>();

/** Parent fields first, then child fields, so child wins on name collision. */
function collectStructFields(
  spec: StructTypeSpec,
  cache: TypeCache,
  startQualifiedName: string,
  side: "parser" | "encoder",
): FieldRef[] {
  const cached = cache.derived.get(spec) as FieldRef[] | undefined;
  if (cached) return cached;

  const chain: StructTypeSpec[] = [];
  let current: StructTypeSpec | undefined = spec;
  const seen = new Set<string>();
  seen.add(startQualifiedName);
  while (current) {
    chain.unshift(current);
    if (!current.parent) break;
    if (seen.has(current.parent)) {
      throw new TransportError(`Var ${side}: cycle in struct parent chain at "${current.parent}"`);
    }
    seen.add(current.parent);
    const parentSpec = cache.get(current.parent);
    if (!parentSpec) {
      throw new TransportError(`Var ${side}: parent struct "${current.parent}" not cached`);
    }
    if (parentSpec.data.type !== "sen.kernel.StructTypeSpec") {
      throw new TransportError(`Var ${side}: parent "${current.parent}" is not a StructTypeSpec`);
    }
    current = parentSpec.data.value;
  }
  const fields: FieldRef[] = [];
  for (const layer of chain) {
    for (const field of layer.fields) {
      fields.push({ name: field.name, type: field.type });
    }
  }
  cache.derived.set(spec, fields);
  return fields;
}

function findArm(spec: VariantTypeSpec, qualifiedName: string): VariantTypeFieldSpec | undefined {
  let armMap = variantArmsCache.get(spec);
  if (!armMap) {
    armMap = new Map<string, VariantTypeFieldSpec>();
    for (const f of spec.fields) {
      armMap.set(f.type, f);
    }
    variantArmsCache.set(spec, armMap);
  }
  return armMap.get(qualifiedName);
}

// -- parser --------------------------------------------------------------------------------------

/** Parse a JSON value into a `Var` driven by its declared type. Type spec must already be cached. */
export function parseVar(typeName: string, raw: unknown, cache: TypeCache): Var {
  if (builtInScalarTypes.has(typeName)) return parseBuiltIn(typeName, raw);
  const spec = cache.get(typeName);
  if (!spec) {
    throw new TransportError(`Var parser: no type spec cached for "${typeName}"`);
  }
  return parseCustom(spec, raw, cache);
}

function parseBuiltIn(typeName: string, raw: unknown): Var {
  if (typeName === "bool") return raw as boolean;
  if (stringBuiltIns.has(typeName)) return raw as string;
  if (numberBuiltIns.has(typeName)) return raw as number;
  if (bigintBuiltIns.has(typeName)) {
    if (typeof raw !== "string") {
      throw new TransportError(
        `Var parser: ${typeName} expected a decimal string, got ${typeof raw}`,
      );
    }
    try {
      return BigInt(raw);
    } catch (err) {
      throw new TransportError(
        `Var parser: ${typeName} value ${JSON.stringify(raw)} is not a decimal integer`,
        { cause: err },
      );
    }
  }
  throw new TransportError(`Var parser: unknown built-in type "${typeName}"`);
}

function parseCustom(spec: CustomTypeSpec, raw: unknown, cache: TypeCache): Var {
  const qn = spec.qualifiedName;
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
      return parseEnum(spec.data.value, raw, qn, cache);
    case "sen.kernel.QuantityTypeSpec":
      return parseQuantity(spec.data.value, raw, qn);
    case "sen.kernel.SequenceTypeSpec":
      return parseSequence(spec.data.value, raw, cache, qn);
    case "sen.kernel.StructTypeSpec":
      return parseStruct(spec.data.value, raw, cache, qn);
    case "sen.kernel.VariantTypeSpec":
      return parseVariant(spec.data.value, raw, cache, qn);
    case "sen.kernel.AliasTypeSpec":
      return parseVar(spec.data.value.aliasedType, raw, cache);
    case "sen.kernel.OptionalTypeSpec":
      return parseOptional(spec.data.value, raw, cache);
    case "sen.kernel.ClassTypeSpec":
      return classNotAValue(spec.data.value, qn, "parser");
  }
}

function enumNames(spec: EnumTypeSpec, cache: TypeCache): Set<string> {
  const cached = cache.derived.get(spec) as Set<string> | undefined;
  if (cached) return cached;
  const set = new Set(spec.enums.map((e) => e.name));
  cache.derived.set(spec, set);
  return set;
}

function parseEnum(spec: EnumTypeSpec, raw: unknown, qn: string, cache: TypeCache): Var {
  if (typeof raw !== "string") {
    throw new TransportError(`Var parser: enum "${qn}" expected a string, got ${typeof raw}`);
  }
  if (!enumNames(spec, cache).has(raw)) {
    throw new TransportError(`Var parser: enum "${qn}" has no enumerator "${raw}"`);
  }
  return raw;
}

function parseQuantity(spec: QuantityTypeSpec, raw: unknown, qn: string): Var {
  if (typeof raw !== "number") {
    throw new TransportError(`Var parser: quantity "${qn}" expected a number, got ${typeof raw}`);
  }
  return new Quantity(raw, spec.unit, spec.minValue, spec.maxValue);
}

function parseSequence(spec: SequenceTypeSpec, raw: unknown, cache: TypeCache, qn: string): Var {
  if (!Array.isArray(raw)) {
    throw new TransportError(`Var parser: sequence "${qn}" expected an array, got ${typeof raw}`);
  }
  return raw.map((elem) => parseVar(spec.elementType, elem, cache));
}

function parseStruct(spec: StructTypeSpec, raw: unknown, cache: TypeCache, qn: string): Var {
  const fields = collectStructFields(spec, cache, qn, "parser");
  // Empty structs (e.g. variant arms with no payload like `struct SystemSleep;`) come across
  // the wire as `null`. Accept that as an empty value.
  if (raw === null && fields.length === 0) {
    return {};
  }
  if (raw === null || typeof raw !== "object" || Array.isArray(raw)) {
    const shape = raw === null ? "null" : Array.isArray(raw) ? "array" : typeof raw;
    throw new TransportError(`Var parser: struct "${qn}" expected an object, got ${shape}`);
  }
  const rawObj = raw as Record<string, unknown>;
  const result: Record<string, Var> = {};
  for (const field of fields) {
    result[field.name] = parseVar(field.type, rawObj[field.name], cache);
  }
  return result;
}

function parseVariant(spec: VariantTypeSpec, raw: unknown, cache: TypeCache, qn: string): Var {
  if (raw === null || typeof raw !== "object" || Array.isArray(raw)) {
    const shape = raw === null ? "null" : Array.isArray(raw) ? "array" : typeof raw;
    throw new TransportError(`Var parser: variant "${qn}" expected {type, value}, got ${shape}`);
  }
  const rawObj = raw as Record<string, unknown>;
  const tag = rawObj["type"];
  if (typeof tag !== "string") {
    throw new TransportError(
      `Var parser: variant "${qn}" requires string "type" tag, got ${typeof tag}`,
    );
  }
  const arm = findArm(spec, tag);
  if (!arm) {
    throw new TransportError(`Var parser: variant "${qn}" has no arm matching tag "${tag}"`);
  }
  return new Variant(tag, parseVar(arm.type, rawObj["value"], cache));
}

function parseOptional(spec: OptionalTypeSpec, raw: unknown, cache: TypeCache): Var {
  if (raw === null) return null;
  return parseVar(spec.type, raw, cache);
}

// -- encoder -------------------------------------------------------------------------------------

/** Inverse of `parseVar`. Accepts both runtime-class instances (Quantity, Variant) and plain objects. */
export function encodeVar(typeName: string, value: unknown, cache: TypeCache): unknown {
  if (builtInScalarTypes.has(typeName)) return encodeBuiltIn(typeName, value);
  const spec = cache.get(typeName);
  if (!spec) {
    throw new TransportError(`Var encoder: no type spec cached for "${typeName}"`);
  }
  return encodeCustom(spec, value, cache);
}

/** Encode and stringify, ready for the wire. */
export function encodeVarToWire(typeName: string, value: unknown, cache: TypeCache): string {
  return JSON.stringify(encodeVar(typeName, value, cache));
}

function encodeBuiltIn(typeName: string, value: unknown): unknown {
  if (typeName === "bool") {
    if (typeof value !== "boolean") {
      throw new TransportError(`Var encoder: type "${typeName}" expected boolean, got ${typeof value}`);
    }
    return value;
  }
  if (stringBuiltIns.has(typeName)) {
    if (typeof value !== "string") {
      throw new TransportError(`Var encoder: type "${typeName}" expected string, got ${typeof value}`);
    }
    return value;
  }
  if (numberBuiltIns.has(typeName)) {
    if (typeof value !== "number") {
      throw new TransportError(`Var encoder: type "${typeName}" expected number, got ${typeof value}`);
    }
    return value;
  }
  if (bigintBuiltIns.has(typeName)) {
    let asBigInt: bigint;
    if (typeof value === "bigint") {
      asBigInt = value;
    } else if (typeof value === "number") {
      if (!Number.isInteger(value)) {
        throw new TransportError(
          `Var encoder: type "${typeName}" got non-integer number ${value}`,
        );
      }
      if (value > Number.MAX_SAFE_INTEGER || value < Number.MIN_SAFE_INTEGER) {
        throw new TransportError(
          `Var encoder: type "${typeName}" got number ${value} outside safe-integer range; pass a bigint to preserve precision`,
        );
      }
      asBigInt = BigInt(value);
    } else {
      throw new TransportError(
        `Var encoder: type "${typeName}" expected bigint or safe-integer number, got ${typeof value}`,
      );
    }
    if (typeName === "u64" && asBigInt < 0n) {
      throw new TransportError(`Var encoder: type "u64" got negative value ${asBigInt}`);
    }
    return asBigInt.toString();
  }
  throw new TransportError(`Var encoder: unknown built-in type "${typeName}"`);
}

function encodeCustom(spec: CustomTypeSpec, value: unknown, cache: TypeCache): unknown {
  const qn = spec.qualifiedName;
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
      return encodeEnum(spec.data.value, value, qn, cache);
    case "sen.kernel.QuantityTypeSpec":
      return encodeQuantity(spec.data.value, value, qn);
    case "sen.kernel.SequenceTypeSpec":
      return encodeSequence(spec.data.value, value, cache, qn);
    case "sen.kernel.StructTypeSpec":
      return encodeStruct(spec.data.value, value, cache, qn);
    case "sen.kernel.VariantTypeSpec":
      return encodeVariant(spec.data.value, value, cache, qn);
    case "sen.kernel.AliasTypeSpec":
      return encodeVar(spec.data.value.aliasedType, value, cache);
    case "sen.kernel.OptionalTypeSpec":
      return encodeOptional(spec.data.value, value, cache);
    case "sen.kernel.ClassTypeSpec":
      return classNotAValue(spec.data.value, qn, "encoder");
  }
}

function encodeEnum(spec: EnumTypeSpec, value: unknown, qn: string, cache: TypeCache): unknown {
  if (typeof value !== "string") {
    throw new TransportError(`Var encoder: enum "${qn}" expected a string, got ${typeof value}`);
  }
  if (!enumNames(spec, cache).has(value)) {
    throw new TransportError(`Var encoder: enum "${qn}" has no enumerator "${value}"`);
  }
  return value;
}

function encodeQuantity(spec: QuantityTypeSpec, value: unknown, qn: string): unknown {
  let numeric: number;
  if (value instanceof Quantity) {
    if (
      value.unit.name !== spec.unit.name ||
      value.unit.abbreviation !== spec.unit.abbreviation ||
      value.unit.category !== spec.unit.category
    ) {
      const want = spec.unit.abbreviation || spec.unit.name;
      const got = value.unit.abbreviation || value.unit.name;
      throw new TransportError(
        `Var encoder: quantity "${qn}" expects unit "${want}", got "${got}"`,
      );
    }
    numeric = value.value;
  } else if (typeof value === "number") {
    numeric = value;
  } else {
    throw new TransportError(
      `Var encoder: quantity "${qn}" expected a Quantity or number, got ${typeof value}`,
    );
  }
  if (spec.minValue !== null && numeric < spec.minValue) {
    throw new TransportError(
      `Var encoder: quantity "${qn}" value ${numeric} below minimum ${spec.minValue}`,
    );
  }
  if (spec.maxValue !== null && numeric > spec.maxValue) {
    throw new TransportError(
      `Var encoder: quantity "${qn}" value ${numeric} above maximum ${spec.maxValue}`,
    );
  }
  return numeric;
}

function encodeSequence(spec: SequenceTypeSpec, value: unknown, cache: TypeCache, qn: string): unknown {
  if (!Array.isArray(value)) {
    throw new TransportError(`Var encoder: sequence "${qn}" expected an array, got ${typeof value}`);
  }
  return value.map((elem) => encodeVar(spec.elementType, elem, cache));
}

function encodeStruct(spec: StructTypeSpec, value: unknown, cache: TypeCache, qn: string): unknown {
  const fields = collectStructFields(spec, cache, qn, "encoder");
  // Empty struct (e.g. `struct SystemSleep;`) serializes as `null` on the wire; accept the
  // editor's `{}` representation, and accept `null` round-tripped from the parser.
  if (fields.length === 0) return null;
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    const shape = value === null ? "null" : Array.isArray(value) ? "array" : typeof value;
    throw new TransportError(`Var encoder: struct "${qn}" expected an object, got ${shape}`);
  }
  const valueObj = value as Record<string, unknown>;
  const result: Record<string, unknown> = {};
  for (const field of fields) {
    result[field.name] = encodeVar(field.type, valueObj[field.name], cache);
  }
  return result;
}

function encodeVariant(spec: VariantTypeSpec, value: unknown, cache: TypeCache, qn: string): unknown {
  let tag: string;
  let inner: unknown;
  if (value instanceof Variant) {
    tag = value.type;
    inner = value.value;
  } else if (
    value !== null &&
    typeof value === "object" &&
    !Array.isArray(value) &&
    typeof (value as Record<string, unknown>)["type"] === "string"
  ) {
    const obj = value as Record<string, unknown>;
    tag = obj["type"] as string;
    inner = obj["value"];
  } else {
    throw new TransportError(
      `Var encoder: variant "${qn}" expected a Variant or {type, value} object`,
    );
  }
  const arm = findArm(spec, tag);
  if (!arm) {
    throw new TransportError(`Var encoder: variant "${qn}" has no arm matching tag "${tag}"`);
  }
  return { type: tag, value: encodeVar(arm.type, inner, cache) };
}

function encodeOptional(spec: OptionalTypeSpec, value: unknown, cache: TypeCache): unknown {
  if (value === null || value === undefined) return null;
  return encodeVar(spec.type, value, cache);
}

function classNotAValue(_spec: ClassTypeSpec, qn: string, side: "parser" | "encoder"): never {
  throw new TransportError(
    `Var ${side}: class "${qn}" is not a value type (object refs aren't embedded in Var)`,
  );
}
