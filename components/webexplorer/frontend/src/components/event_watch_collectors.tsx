// === EventWatchCollectors.tsx ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo } from "react";

import type { Client } from "@sen/client";
import { useObject } from "@sen/client/react";

import { useInheritedEventNames } from "../state/class_members.js";
import { useEventWatches } from "../state/event_watches.js";
import { useInterestByName } from "../state/interest_registry.js";
import { ObjectEventCollector } from "./object_event_collector.js";

// Subscribes the full per-class event set per object so EventWatchCards fill in without
// requiring the user to open the Events drawer first. event_store ref-counts by
// (interest, object) so this co-exists cheaply with EventsWorkspace + SelectionCollectors.
export function EventWatchCollectors({ client }: { client: Client | null }) {
  const watches = useEventWatches();
  // Collapse duplicate watches; (className, sessionName, busName) isn't part of subscription
  // identity, so picking any one tuple per (interest, object) is fine.
  const perObject = useMemo(() => {
    type Spec = {
      interestName: string;
      objectName: string;
      className: string;
      sessionName: string;
      busName: string;
    };
    const map = new Map<string, Spec>();
    for (const w of watches) {
      const key = `${w.interestName}\0${w.objectName}`;
      if (map.has(key)) continue;
      map.set(key, {
        interestName: w.interestName,
        objectName: w.objectName,
        className: w.className,
        sessionName: w.sessionName,
        busName: w.busName,
      });
    }
    return Array.from(map.values());
  }, [watches]);

  if (!client || perObject.length === 0) return null;
  return (
    <>
      {perObject.map((s) => (
        <Active key={`${s.interestName}|${s.objectName}`} client={client} spec={s} />
      ))}
    </>
  );
}

function Active({
  client,
  spec,
}: {
  client: Client;
  spec: {
    interestName: string;
    objectName: string;
    className: string;
    sessionName: string;
    busName: string;
  };
}) {
  const interest = useInterestByName(spec.interestName);
  const obj = useObject(interest, spec.objectName);
  const eventNames = useInheritedEventNames(client, spec.className);
  return (
    <ObjectEventCollector
      interestName={spec.interestName}
      sessionName={spec.sessionName}
      busName={spec.busName}
      objectName={spec.objectName}
      className={spec.className}
      obj={obj ?? null}
      eventNames={eventNames}
    />
  );
}
