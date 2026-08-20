// === ObjectEventCollector.tsx ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo } from "react";

import type { ObjectHandle } from "@sen/client";

import { objectEventKey } from "../core/keys.js";
import { acquireCollector } from "../state/event_store.js";

// Store de-dups by (interest, object) wire-callback so multiple mounts share one subscription.
export function ObjectEventCollector({
  interestName,
  sessionName,
  busName,
  objectName,
  className,
  obj,
  eventNames,
}: {
  interestName: string;
  sessionName: string;
  busName: string;
  objectName: string;
  className: string;
  obj: ObjectHandle | null;
  eventNames: readonly string[] | null;
}) {
  // Re-run effect only when the set of names changes, not on every re-render with same names.
  const namesKey = useMemo(() => (eventNames ? eventNames.join("\0") : ""), [eventNames]);

  useEffect(() => {
    if (!obj || !eventNames || eventNames.length === 0) return;
    return acquireCollector(objectEventKey(interestName, objectName), {
      obj,
      interestName,
      sessionName,
      busName,
      objectName,
      className,
      eventNames,
      // Precomputed once so per-event `lowerSearch` is a simple concat.
      lowerSuffix: `${objectName}\0${className}\0${interestName}`.toLowerCase(),
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [obj, interestName, sessionName, busName, objectName, className, namesKey]);

  return null;
}
