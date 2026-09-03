// === class_spec_lookup.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { TransportError } from "../errors.js";
import type {
  ClassTypeSpec,
  EventSpec,
  MethodSpec,
  PropertySpec,
} from "../generated/index.js";
import type { TypeCache } from "./type_cache.js";

/** Throws if missing or not a class. */
export function getClassSpec(qualifiedClassName: string, cache: TypeCache): ClassTypeSpec {
  const spec = cache.get(qualifiedClassName);
  if (!spec) throw new TransportError(`class spec "${qualifiedClassName}" not cached`);
  if (spec.data.type !== "sen.kernel.ClassTypeSpec") {
    throw new TransportError(`type "${qualifiedClassName}" is not a class`);
  }
  return spec.data.value;
}

export function findPropertyInClass(
  classSpec: ClassTypeSpec,
  propertyName: string,
  cache: TypeCache,
): PropertySpec | undefined {
  return findClassMember(classSpec, propertyName, cache, "p", (cs) => cs.properties);
}

export function findMethodInClass(
  classSpec: ClassTypeSpec,
  methodName: string,
  cache: TypeCache,
): MethodSpec | undefined {
  return findClassMember(classSpec, methodName, cache, "m", (cs) => cs.methods);
}

export function findEventInClass(
  classSpec: ClassTypeSpec,
  eventName: string,
  cache: TypeCache,
): EventSpec | undefined {
  return findClassMember(classSpec, eventName, cache, "e", (cs) => cs.events);
}

// Wire-rate consumers (subscription_registry.dispatch*) hit this per delivery, so the
// parent-chain walk is memoized on TypeCache.derived. Cache lifetime is tied to the spec
// object reference; if the cache replaces a spec, its old entries die with it.
function findClassMember<T extends { name: string }>(
  classSpec: ClassTypeSpec,
  memberName: string,
  cache: TypeCache,
  kind: "p" | "m" | "e",
  pick: (cs: ClassTypeSpec) => readonly T[],
): T | undefined {
  const cacheKey = `${kind}:${memberName}`;
  let memo = cache.derived.get(classSpec) as Map<string, unknown> | undefined;
  if (memo && memo.has(cacheKey)) return memo.get(cacheKey) as T | undefined;

  let result: T | undefined;
  for (const member of pick(classSpec)) {
    if (member.name === memberName) {
      result = member;
      break;
    }
  }
  if (!result) {
    const seen = new Set<string>();
    for (const parent of classSpec.parents) {
      const found = searchParents(parent, memberName, cache, pick, seen);
      if (found) {
        result = found;
        break;
      }
    }
  }

  if (!memo) {
    memo = new Map();
    cache.derived.set(classSpec, memo);
  }
  memo.set(cacheKey, result);
  return result;
}

function searchParents<T extends { name: string }>(
  parentName: string,
  memberName: string,
  cache: TypeCache,
  pick: (cs: ClassTypeSpec) => readonly T[],
  seen: Set<string>,
): T | undefined {
  if (seen.has(parentName)) return undefined;
  seen.add(parentName);
  const parentSpec = getClassSpec(parentName, cache);
  for (const member of pick(parentSpec)) {
    if (member.name === memberName) return member;
  }
  for (const grandparent of parentSpec.parents) {
    const found = searchParents(grandparent, memberName, cache, pick, seen);
    if (found) return found;
  }
  return undefined;
}
