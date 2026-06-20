// === SelectionCollectors.tsx =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client } from "@sen/client";
import { useObject } from "@sen/client/react";

import { useInheritedEventNames } from "../state/class_members.js";
import { ObjectEventCollector } from "./object_event_collector.js";
import { useInterestByName } from "../state/interest_registry.js";
import { ObjectSampleCollector } from "./object_sample_collector.js";
import type { Selection } from "../state/selection.js";

// App-level mount so subscriptions outlive popout-window remount cycles.
export function SelectionCollectors({
  client,
  selections,
}: {
  client: Client | null;
  selections: readonly Selection[];
}) {
  if (!client || selections.length === 0) return null;
  return (
    <>
      {selections.map((sel) => (
        <Active
          key={`${sel.interestName}|${sel.objectName}`}
          client={client}
          selection={sel}
        />
      ))}
    </>
  );
}

function Active({ client, selection }: { client: Client; selection: Selection }) {
  const interest = useInterestByName(selection.interestName);
  const obj = useObject(interest, selection.objectName);
  const eventNames = useInheritedEventNames(client, selection.className);
  return (
    <>
      <ObjectSampleCollector
        interestName={selection.interestName}
        objectName={selection.objectName}
        obj={obj ?? null}
      />
      <ObjectEventCollector
        interestName={selection.interestName}
        sessionName={selection.sessionName}
        busName={selection.busName}
        objectName={selection.objectName}
        className={selection.className}
        obj={obj ?? null}
        eventNames={eventNames}
      />
    </>
  );
}
