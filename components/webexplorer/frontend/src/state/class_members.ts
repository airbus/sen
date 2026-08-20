// === class_members.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useState } from "react";

import type {
  Client,
  EventSpec,
  MethodSpec,
  PropertyCategorySpec,
  PropertySpec,
} from "@sen/client";

/** Leaf at index 0, parents BFS; overridden members are dropped from the parent's group. */
export interface ClassMemberGroup {
  className: string;
  properties: PropertySpec[];
  methods: MethodSpec[];
  events: EventSpec[];
}

/** Returns `null` if any class in the chain isn't cached yet. */
export function classMemberGroups(
  client: Client,
  className: string,
): ClassMemberGroup[] | null {
  const groups: ClassMemberGroup[] = [];
  const propNames = new Set<string>();
  const methodNames = new Set<string>();
  const eventNames = new Set<string>();
  const visited = new Set<string>();

  let queue: string[] = [className];
  while (queue.length > 0) {
    const next: string[] = [];
    for (const name of queue) {
      if (visited.has(name)) continue;
      visited.add(name);
      const spec = client.getType(name);
      if (!spec) return null;
      if (spec.data.type !== "sen.kernel.ClassTypeSpec") return null;
      const cls = spec.data.value;
      const group: ClassMemberGroup = {
        className: name,
        properties: [],
        methods: [],
        events: [],
      };
      for (const p of cls.properties) {
        if (propNames.has(p.name)) continue;
        propNames.add(p.name);
        group.properties.push(p);
      }
      for (const m of cls.methods) {
        if (methodNames.has(m.name)) continue;
        methodNames.add(m.name);
        group.methods.push(m);
      }
      for (const e of cls.events) {
        if (eventNames.has(e.name)) continue;
        eventNames.add(e.name);
        group.events.push(e);
      }
      groups.push(group);
      next.push(...cls.parents);
    }
    queue = next;
  }
  return groups;
}

export function totalProperties(groups: ClassMemberGroup[]): number {
  let n = 0;
  for (const g of groups) n += g.properties.length;
  return n;
}

export function totalMethods(groups: ClassMemberGroup[]): number {
  let n = 0;
  for (const g of groups) n += g.methods.length;
  return n;
}

export function totalEvents(groups: ClassMemberGroup[]): number {
  let n = 0;
  for (const g of groups) n += g.events.length;
  return n;
}

export function flatEventNames(groups: ClassMemberGroup[]): string[] {
  const out: string[] = [];
  for (const g of groups) for (const e of g.events) out.push(e.name);
  return out;
}

// Subscribes to onTypeAdded while still null and drops the subscription on resolution.
export function useClassMemberGroups(
  client: Client,
  className: string,
): ClassMemberGroup[] | null {
  const [tick, setTick] = useState(0);
  const groups = useMemo(
    () => classMemberGroups(client, className),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [client, className, tick],
  );
  useEffect(() => {
    if (groups) return;
    return client.onTypeAdded(() => setTick((x) => x + 1));
  }, [groups, client]);
  return groups;
}

/** `null` when client is offline, spec isn't cached, or property isn't declared. */
export function propertyCategoryOf(
  client: Client | null,
  className: string,
  propertyName: string,
): PropertyCategorySpec | null {
  if (!client) return null;
  const spec = client.getType(className);
  if (!spec || spec.data.type !== "sen.kernel.ClassTypeSpec") return null;
  const prop = spec.data.value.properties.find((p: PropertySpec) => p.name === propertyName);
  return prop?.category ?? null;
}

export function isDynamicCategory(c: PropertyCategorySpec | null): boolean {
  return c === "dynamicRO" || c === "dynamicRW";
}

export function isWritableCategory(c: PropertyCategorySpec | null): boolean {
  return c === "staticRW" || c === "dynamicRW";
}

export function useInheritedEventNames(
  client: Client,
  className: string,
): string[] | null {
  const groups = useClassMemberGroups(client, className);
  return useMemo(() => (groups ? flatEventNames(groups) : null), [groups]);
}

/** Empty while the chain resolves. */
export function useClassEvents(client: Client, className: string): EventSpec[] {
  const groups = useClassMemberGroups(client, className);
  return useMemo(() => {
    if (!groups) return [];
    const out: EventSpec[] = [];
    for (const g of groups) for (const e of g.events) out.push(e);
    return out;
  }, [groups]);
}

// Root-to-leaf; Sen has single inheritance so follow parents[0]. Returns [className]
// (best-effort) if the spec isn't cached.
export function inheritanceChain(client: Client | null, className: string): string[] {
  if (!client || !className) return className ? [className] : [];
  const chain: string[] = [];
  const visited = new Set<string>();
  let current: string | undefined = className;
  while (current && !visited.has(current)) {
    visited.add(current);
    chain.unshift(current);
    const spec = client.getType(current);
    if (!spec || spec.data.type !== "sen.kernel.ClassTypeSpec") break;
    const parents = spec.data.value.parents;
    if (!parents || parents.length === 0) break;
    current = parents[0];
  }
  return chain;
}

/** Pass as a useMemo dep to re-evaluate `client.getType(...)` results. */
export function useTypeCacheTick(client: Client | null): number {
  const [tick, setTick] = useState(0);
  useEffect(() => {
    if (!client) return;
    return client.onTypeAdded(() => setTick((n) => n + 1));
  }, [client]);
  return tick;
}

// Returns false when the concrete chain isn't cached yet; pair with useTypeCacheTick
// to re-evaluate. Per-Client cache cleared on onTypeAdded.
const isInClassCacheByClient = new WeakMap<Client, Map<string, Map<string, boolean>>>();
const isInClassWired = new WeakSet<Client>();

export function isInClassChain(client: Client, base: string, concrete: string): boolean {
  if (concrete === base) return true;
  let perClient = isInClassCacheByClient.get(client);
  if (!perClient) {
    perClient = new Map();
    isInClassCacheByClient.set(client, perClient);
  }
  if (!isInClassWired.has(client)) {
    isInClassWired.add(client);
    // Client-lifetime subscription; disposer dies with the Client.
    client.onTypeAdded(() => {
      isInClassCacheByClient.get(client)?.clear();
    });
  }
  let perBase = perClient.get(base);
  if (!perBase) {
    perBase = new Map();
    perClient.set(base, perBase);
  }
  const cached = perBase.get(concrete);
  if (cached !== undefined) return cached;
  const groups = classMemberGroups(client, concrete);
  if (!groups) {
    // Don't memoize the negative; the chain may land later.
    return false;
  }
  let result = false;
  for (const g of groups) {
    if (g.className === base) {
      result = true;
      break;
    }
  }
  perBase.set(concrete, result);
  return result;
}
