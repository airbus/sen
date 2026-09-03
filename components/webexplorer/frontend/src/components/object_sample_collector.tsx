// === ObjectSampleCollector.tsx =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useRef } from "react";

import type { ObjectHandle } from "@sen/client";

import { propertyKeyOf, type PropertyKey } from "../core/watch_keys.js";
import { sampleLeaves } from "../state/leaf_samples.js";
import { parseServerTimestampMs } from "../core/time.js";
import {
  markDeadLeaf,
  markDeadProperty,
  reportDelivery,
} from "../state/sample_store.js";

// Per-delivery diffs the leaf path set; paths in `prev - current` get a gap sentinel so
// plots draw a discontinuity instead of holding a stale value (variant arm switched,
// optional cleared, sequence shrank). Teardown sentinels every leaf for every property.
export function ObjectSampleCollector({
  interestName,
  objectName,
  obj,
}: {
  interestName: string;
  objectName: string;
  obj: ObjectHandle | null;
}) {
  const prevPathsByPropRef = useRef<Map<PropertyKey, Set<string>> | null>(null);
  // Reconnect rotates `obj` to a fresh non-null handle; objRef lets cleanup distinguish
  // a real teardown from a transient handle swap so we don't fence every plot on a blip.
  const objRef = useRef(obj);
  objRef.current = obj;
  const mountedRef = useRef(true);

  useEffect(() => {
    if (!obj) return;
    if (prevPathsByPropRef.current === null) prevPathsByPropRef.current = new Map();
    const cancel = obj.onAnyChange((changes, info) => {
      const at = Date.now();
      const tServer = parseServerTimestampMs(info.timestamp);
      const byProp = prevPathsByPropRef.current!;
      for (const [propertyName, value] of changes) {
        const sourceKey = propertyKeyOf({
          interest: interestName,
          object: objectName,
          property: propertyName,
        });
        const leaves = sampleLeaves(value, "");
        const currentPaths = new Set<string>();
        for (let i = 0; i < leaves.length; i++) currentPaths.add(leaves[i]![0]);
        const prev = byProp.get(sourceKey);
        if (prev) {
          for (const p of prev) {
            if (!currentPaths.has(p)) markDeadLeaf(sourceKey, p, at, tServer);
          }
        }
        byProp.set(sourceKey, currentPaths);
        reportDelivery(sourceKey, leaves, at, tServer);
      }
    });
    return () => {
      const realLoss = !mountedRef.current || objRef.current === null || objRef.current === undefined;
      if (realLoss) {
        const at = Date.now();
        const byProp = prevPathsByPropRef.current;
        if (byProp) {
          for (const sourceKey of byProp.keys()) markDeadProperty(sourceKey, at);
        }
      }
      prevPathsByPropRef.current = null;
      cancel();
    };
  }, [obj, interestName, objectName]);

  useEffect(() => () => { mountedRef.current = false; }, []);

  return null;
}
