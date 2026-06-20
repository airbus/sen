// === WatchCollectors.tsx =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef } from "react";

import type { DeliveryInfo, Var } from "@sen/client";
import { useObject } from "@sen/client/react";

import { makePropertyKey, type WatchSource } from "../core/watch_keys.js";
import { useInterestByName } from "../state/interest_registry.js";
import { sampleLeaves } from "../state/leaf_samples.js";
import { parseServerTimestampMs } from "../core/time.js";
import {
  markDeadLeaf,
  markDeadProperty,
  reportDelivery,
} from "../state/sample_store.js";

// One wire subscription per property; dedupe by propertyKey so multiple watch entries on
// the same property share a single subscription. Series in the plot board key off the same.
export function WatchCollectors({ sources }: { sources: readonly WatchSource[] }) {
  const unique = useMemo(() => {
    const map = new Map<string, WatchSource>();
    for (const s of sources) {
      const k = makePropertyKey(s);
      if (!map.has(k)) map.set(k, s);
    }
    return Array.from(map.values());
  }, [sources]);
  return (
    <>
      {unique.map((s) => (
        <WatchSourceCollector key={makePropertyKey(s)} source={s} />
      ))}
    </>
  );
}

function WatchSourceCollector({ source }: { source: WatchSource }) {
  const interest = useInterestByName(source.interestName);
  const obj = useObject(interest, source.objectName);
  const sourceKey = makePropertyKey(source);
  // Diff prev vs current leaf paths each delivery; paths in `prev - current` get a gap
  // sentinel so plots draw a discontinuity (variant arm switch, optional cleared, etc).
  const prevPathsRef = useRef<Set<string> | null>(null);
  // Reconnect rotates `obj` to a fresh non-null handle. objRef lets cleanup distinguish a
  // real teardown (push gap) from a handle swap (don't plant a fence on every connection blip).
  const objRef = useRef(obj);
  objRef.current = obj;
  // Declared AFTER the main effect so its reverse-order cleanup runs first, leaving a fresh
  // false for the main cleanup to read on true unmount.
  const mountedRef = useRef(true);

  useEffect(() => {
    if (!obj) return;
    if (prevPathsRef.current === null) prevPathsRef.current = new Set();
    const cancel = obj.onPropertyChanged(source.propertyName, (value: Var, info: DeliveryInfo) => {
      const at = Date.now();
      const tServer = parseServerTimestampMs(info.timestamp);
      const leaves = sampleLeaves(value, "");
      const currentPaths = new Set<string>();
      for (let i = 0; i < leaves.length; i++) currentPaths.add(leaves[i]![0]);
      const prev = prevPathsRef.current!;
      for (const p of prev) {
        if (!currentPaths.has(p)) markDeadLeaf(sourceKey, p, at, tServer);
      }
      prevPathsRef.current = currentPaths;
      reportDelivery(sourceKey, leaves, at, tServer);
    });
    return () => {
      // Push gap only on real teardown: unmount or obj swapped to null. A swap to another
      // non-null handle is reconnect; the next delivery continues the series.
      if (!mountedRef.current || objRef.current === null || objRef.current === undefined) {
        markDeadProperty(sourceKey, Date.now());
      }
      prevPathsRef.current = null;
      cancel();
    };
  }, [obj, source.propertyName, sourceKey]);

  useEffect(() => () => { mountedRef.current = false; }, []);

  return null;
}
