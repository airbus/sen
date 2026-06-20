// === use_events.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { ObjectHandle, Var } from "../index.js";

/** One delivery of an event. */
export interface EventDelivery {
  args: Var[];
  /** Server-side emission timestamp (kernel `EventInfo.creationTime`); wire string format. */
  timestamp: string;
  /** Local-wall-clock ms at which the React handler saw the delivery. Use for relative
   *  ordering when wire timestamps are missing or unreliable. */
  at: number;
}

/** Bounded ring of recent deliveries. */
export interface UseEventsOptions {
  /** Cap on the returned array; older entries fall off the front. Default 200. */
  max?: number;
  /** Optional predicate evaluated on the wire-side handler; deliveries returning `false`
   *  are dropped before the React state update, so they don't trigger a re-render or count
   *  against `max`. Useful for narrow consumers (e.g. only events whose first arg matches
   *  a target). */
  filter?: (delivery: EventDelivery) => boolean;
}

/**
 * Subscribe to one named event on an object and keep a bounded ring of recent deliveries.
 * Returns the newest-last array; `obj === null` returns an empty array. The full retention
 * model from the brief (large virtualized buffer) lives in the explorer's events view,
 * not in this leaf hook.
 *
 * Every accepted delivery triggers a React state update + array reallocation; fine for
 * dashboards/logs at human-readable rates. For high-rate streams (sensors, chart sources)
 * use `makeBufferStore` + a narrow `useView` selector; see `react/store/README.md`.
 */
export function useEvents(
  obj: ObjectHandle | null,
  name: string,
  opts: UseEventsOptions = {},
): EventDelivery[] {
  const [log, setLog] = useState<EventDelivery[]>([]);
  const max = opts.max ?? 200;
  const filter = opts.filter;

  useEffect(() => {
    if (!obj) {
      setLog([]);
      return;
    }
    setLog([]);
    return obj.onEventTriggered(name, (args, info) => {
      const delivery: EventDelivery = { args, timestamp: info.timestamp, at: Date.now() };
      if (filter && !filter(delivery)) return;
      setLog((prev) => {
        const next = prev.length >= max ? prev.slice(prev.length - max + 1) : prev.slice();
        next.push(delivery);
        return next;
      });
    });
  }, [obj, name, max, filter]);

  return log;
}
