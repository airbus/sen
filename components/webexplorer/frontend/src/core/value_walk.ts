// === value_walk.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, StructTypeFieldSpec, Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

import { specOf } from "./types.js";

export interface LeafInfo {
  /** Dotted/bracketed path from the root; empty for a top-level leaf. */
  path: string;
  /** Element type for sequence-wildcard leaves. */
  type: string;
}

// Sequence elements collapse to `[*]`; variants yield one placeholder for the active arm;
// aliases unwrap; classes yield nothing; uncached specs yield empty.
export function enumerateLeaves(client: Client | null, declaredType: string): LeafInfo[] {
  const out: LeafInfo[] = [];
  walkTypeTree(client, declaredType, "", out, new Set());
  return out;
}

function walkTypeTree(
  client: Client | null,
  declaredType: string,
  path: string,
  out: LeafInfo[],
  seenStructs: Set<string>,
): void {
  if (
    declaredType === "bool" ||
    declaredType === "string" ||
    declaredType === "TimeStamp" ||
    declaredType === "Duration" ||
    declaredType === "u8" || declaredType === "u16" || declaredType === "u32" || declaredType === "u64" ||
    declaredType === "i8" || declaredType === "i16" || declaredType === "i32" || declaredType === "i64" ||
    declaredType === "f32" || declaredType === "f64"
  ) {
    out.push({ path, type: declaredType });
    return;
  }
  const spec = specOf(client, declaredType);
  if (!spec) return;
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
    case "sen.kernel.QuantityTypeSpec":
      out.push({ path, type: declaredType });
      return;
    case "sen.kernel.StructTypeSpec": {
      // Cycle guard.
      if (seenStructs.has(declaredType)) return;
      seenStructs.add(declaredType);
      for (const field of spec.data.value.fields) {
        const sub = path ? `${path}.${field.name}` : field.name;
        walkTypeTree(client, field.type, sub, out, seenStructs);
      }
      seenStructs.delete(declaredType);
      return;
    }
    case "sen.kernel.SequenceTypeSpec":
      out.push({ path: `${path}[*]`, type: spec.data.value.elementType });
      return;
    case "sen.kernel.VariantTypeSpec":
      out.push({ path, type: declaredType });
      return;
    case "sen.kernel.OptionalTypeSpec":
      walkTypeTree(client, spec.data.value.type, path, out, seenStructs);
      return;
    case "sen.kernel.AliasTypeSpec":
      walkTypeTree(client, spec.data.value.aliasedType, path, out, seenStructs);
      return;
    case "sen.kernel.ClassTypeSpec":
      // Class refs aren't inline state; their surface lives in the referenced object.
      return;
  }
}

// Feeds the Properties filter: searching for "velocity" matches a parent property whose
// type transitively contains a field by that name. Doesn't descend into classes.
export function collectTypeSearchTerms(
  client: Client | null,
  declaredType: string,
  out: string[],
  visited: Set<string> = new Set(),
): void {
  if (visited.has(declaredType)) return;
  visited.add(declaredType);
  const spec = specOf(client, declaredType);
  if (!spec) return;
  switch (spec.data.type) {
    case "sen.kernel.StructTypeSpec":
      for (const field of spec.data.value.fields) {
        out.push(field.name);
        collectTypeSearchTerms(client, field.type, out, visited);
      }
      return;
    case "sen.kernel.VariantTypeSpec":
      // Arms identified by payload type name; no separate name field.
      for (const arm of spec.data.value.fields) {
        out.push(arm.type);
        collectTypeSearchTerms(client, arm.type, out, visited);
      }
      return;
    case "sen.kernel.SequenceTypeSpec":
      collectTypeSearchTerms(client, spec.data.value.elementType, out, visited);
      return;
    case "sen.kernel.OptionalTypeSpec":
      collectTypeSearchTerms(client, spec.data.value.type, out, visited);
      return;
    case "sen.kernel.AliasTypeSpec":
      collectTypeSearchTerms(client, spec.data.value.aliasedType, out, visited);
      return;
    case "sen.kernel.EnumTypeSpec":
      for (const e of spec.data.value.enums) out.push(e.name);
      return;
    case "sen.kernel.QuantityTypeSpec":
    case "sen.kernel.ClassTypeSpec":
      return;
  }
}

// Schema analogue of extractAt; aliases/optionals unwrap; sequences follow element type.
// Returns null when the spec isn't cached or the path doesn't resolve.
export function typeAtPath(
  client: Client | null,
  declaredType: string,
  path: string,
): string | null {
  let cur = unwrapAliasOptional(client, declaredType);
  let i = 0;
  while (i < path.length) {
    const spec = specOf(client, cur);
    if (!spec) return null;
    if (path[i] === "[") {
      const close = path.indexOf("]", i);
      if (close < 0) return null;
      if (spec.data.type !== "sen.kernel.SequenceTypeSpec") return null;
      cur = unwrapAliasOptional(client, spec.data.value.elementType);
      i = close + 1;
      continue;
    }
    if (path[i] === ".") {
      i++;
      continue;
    }
    let j = i;
    while (j < path.length && path[j] !== "." && path[j] !== "[") j++;
    const key = path.slice(i, j);
    if (spec.data.type !== "sen.kernel.StructTypeSpec") return null;
    const field = spec.data.value.fields.find((f: StructTypeFieldSpec) => f.name === key);
    if (!field) return null;
    cur = unwrapAliasOptional(client, field.type);
    i = j;
  }
  return cur;
}

function unwrapAliasOptional(client: Client | null, declaredType: string): string {
  let t = declaredType;
  // Bounded against pathological cycles.
  for (let depth = 0; depth < 8; depth++) {
    const spec = specOf(client, t);
    if (!spec) return t;
    if (spec.data.type === "sen.kernel.AliasTypeSpec") {
      t = spec.data.value.aliasedType;
      continue;
    }
    if (spec.data.type === "sen.kernel.OptionalTypeSpec") {
      t = spec.data.value.type;
      continue;
    }
    return t;
  }
  return t;
}

/** `position.x`, `samples[3].value`. Variants are transparent. */
export function extractAt(value: Var | undefined, path: string): Var | undefined {
  if (value === undefined) return undefined;
  if (path === "") return value;
  let cur: unknown = value;
  let i = 0;
  while (i < path.length) {
    if (cur instanceof Variant) cur = cur.value;
    if (cur === null || cur === undefined) return undefined;
    if (path[i] === "[") {
      const close = path.indexOf("]", i);
      if (close < 0) return undefined;
      const idx = parseInt(path.slice(i + 1, close), 10);
      if (!Number.isFinite(idx) || !Array.isArray(cur)) return undefined;
      cur = cur[idx];
      i = close + 1;
      continue;
    }
    if (path[i] === ".") {
      i++;
      continue;
    }
    let j = i;
    while (j < path.length && path[j] !== "." && path[j] !== "[") j++;
    const key = path.slice(i, j);
    if (typeof cur !== "object" || Array.isArray(cur) || cur instanceof Quantity) return undefined;
    cur = (cur as Record<string, unknown>)[key];
    i = j;
  }
  if (cur instanceof Variant) cur = cur.value;
  return cur as Var | undefined;
}

// Watch-pane auto-collapse: [] for childless leaves; null for sequences/variants whose
// child set is value-dependent (auto-collapse is unsafe there).
export function enumerateTopLevelPaths(client: Client | null, declaredType: string): string[] | null {
  if (
    declaredType === "bool" ||
    declaredType === "string" ||
    declaredType === "TimeStamp" ||
    declaredType === "Duration" ||
    declaredType === "u8" || declaredType === "u16" || declaredType === "u32" || declaredType === "u64" ||
    declaredType === "i8" || declaredType === "i16" || declaredType === "i32" || declaredType === "i64" ||
    declaredType === "f32" || declaredType === "f64"
  ) {
    return [];
  }
  const spec = specOf(client, declaredType);
  if (!spec) return [];
  switch (spec.data.type) {
    case "sen.kernel.EnumTypeSpec":
    case "sen.kernel.QuantityTypeSpec":
      return [];
    case "sen.kernel.StructTypeSpec":
      return spec.data.value.fields.map((f: StructTypeFieldSpec) => f.name);
    case "sen.kernel.SequenceTypeSpec":
    case "sen.kernel.VariantTypeSpec":
      return null;
    case "sen.kernel.OptionalTypeSpec":
      return enumerateTopLevelPaths(client, spec.data.value.type);
    case "sen.kernel.AliasTypeSpec":
      return enumerateTopLevelPaths(client, spec.data.value.aliasedType);
    case "sen.kernel.ClassTypeSpec":
      return [];
  }
  return [];
}

/** Quantities resolve to their `.value`. */
export function extractNumericAt(value: Var | undefined, path: string): number | null {
  const v = extractAt(value, path);
  if (typeof v === "number") return Number.isFinite(v) ? v : null;
  if (v instanceof Quantity) return Number.isFinite(v.value) ? v.value : null;
  return null;
}
