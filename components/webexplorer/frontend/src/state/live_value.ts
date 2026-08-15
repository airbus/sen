// === live_value.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// useSyncExternalStore-backed per-property subscription. A tick on property X invalidates
// only the row that asked about X. LiveValue fields are undefined until the first sample.

import { useCallback, useRef, useSyncExternalStore } from "react";

import type { Client, ObjectHandle, PropertySpec, Var } from "@sen/client";

export interface LiveValue {
  value: Var | undefined;
  timestamp: string | undefined;
}

const EMPTY_LIVE: LiveValue = Object.freeze({
  value: undefined,
  timestamp: undefined,
});

// Outer LiveValue identity flips on every delivery via an alternating-buffer ref pair
// (no per-push allocation). Don't stash the reference across renders to compare later -
// tear off `live.value` / `live.timestamp` instead. useFlash-style consumers should depend
// on `live.value`, not the wrapping LiveValue.
export function useLiveProperty(
  obj: ObjectHandle | null,
  propertyName: string,
): LiveValue {
  const snapRef = useRef<LiveValue>(EMPTY_LIVE);
  // Alternating mutable pair so each delivery flips snapshot identity without allocating.
  const scratchRef = useRef<{ a: LiveValue; b: LiveValue; usingA: boolean } | null>(null);
  if (scratchRef.current === null) {
    scratchRef.current = {
      a: { value: undefined, timestamp: undefined },
      b: { value: undefined, timestamp: undefined },
      usingA: false,
    };
  }
  // Reset before next getSnapshot call so a swap doesn't flash the prior property's value.
  const keyRef = useRef("");
  const key = obj ? `${obj.name}\0${propertyName}` : "";
  if (keyRef.current !== key) {
    snapRef.current = EMPTY_LIVE;
    keyRef.current = key;
  }

  const subscribe = useCallback(
    (listener: () => void) => {
      if (!obj) return () => undefined;
      return obj.onPropertyChanged(propertyName, (value, info) => {
        const scratch = scratchRef.current!;
        const next = scratch.usingA ? scratch.b : scratch.a;
        next.value = value;
        next.timestamp = info.timestamp;
        scratch.usingA = !scratch.usingA;
        snapRef.current = next;
        listener();
      });
    },
    [obj, propertyName],
  );

  const getSnapshot = useCallback(() => snapRef.current, []);
  return useSyncExternalStore(subscribe, getSnapshot, getSnapshot);
}

/** `null` while the spec / property isn't cached yet; self-corrects once it lands. */
export function findClassProperty(
  client: Client | null,
  className: string,
  propertyName: string,
): PropertySpec | null {
  if (!client) return null;
  const seen = new Set<string>();
  let queue: string[] = [className];
  while (queue.length > 0) {
    const next: string[] = [];
    for (const name of queue) {
      if (seen.has(name)) continue;
      seen.add(name);
      const spec = client.getType(name);
      if (!spec || spec.data.type !== "sen.kernel.ClassTypeSpec") continue;
      const cls = spec.data.value;
      const prop = cls.properties.find((p: PropertySpec) => p.name === propertyName);
      if (prop) return prop;
      next.push(...cls.parents);
    }
    queue = next;
  }
  return null;
}

export function isPropertyStatic(
  client: Client | null,
  className: string,
  propertyName: string,
): boolean {
  const cat = findClassProperty(client, className, propertyName)?.category;
  return cat === "staticRW" || cat === "staticRO";
}

export function isPropertyWritable(
  client: Client | null,
  className: string,
  propertyName: string,
): boolean {
  return findClassProperty(client, className, propertyName)?.category === "dynamicRW";
}
