// === interest_owner.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Single owner of every backend interest declaration. Mounted once at the App level so
// declarations don't get torn down when a view swap unmounts the QueryRow that would
// otherwise own them. Every other consumer reads handles out of interest_registry.

import { useMemo } from "react";

import type { Client } from "@sen/client";
import { useInterest } from "@sen/client/react";

import { parseBusKey } from "../core/keys.js";
import { interestNameFor } from "../widgets/interests/bus_queries.js";
import { useBusQueries } from "../widgets/interests/bus_queries_store.js";
import { useRegisterInterest, useRegisterInterestError } from "./interest_registry.js";

interface Declaration {
  /** Full interest name `session.bus.queryName` - also the slot's React key. */
  name: string;
  /** SQL body to declare. */
  query: string;
}

export function InterestOwner({ client }: { client: Client | null }) {
  const busQueries = useBusQueries();
  const declarations = useMemo<Declaration[]>(() => {
    const out: Declaration[] = [];
    for (const [bKey, queries] of Object.entries(busQueries)) {
      const parsed = parseBusKey(bKey);
      if (!parsed) continue;
      for (const q of queries) {
        out.push({
          name: interestNameFor(parsed.sessionName, parsed.busName, q.name),
          query: q.query,
        });
      }
    }
    return out;
  }, [busQueries]);
  // Skipping the slot mounts while disconnected avoids registry churn during the gap.
  if (!client) return null;
  return (
    <>
      {declarations.map((d) => (
        <InterestSlot key={d.name} client={client} name={d.name} query={d.query} />
      ))}
    </>
  );
}

// Headless: declares one interest and mirrors handle + error into the registry. Lives
// as a child of InterestOwner so React reconciles add/remove/update of the list.
function InterestSlot({
  client,
  name,
  query,
}: {
  client: Client;
  name: string;
  query: string;
}) {
  const args = useMemo(() => ({ name, query }), [name, query]);
  const { handle, error } = useInterest(client, args);
  useRegisterInterest(name, handle);
  useRegisterInterestError(name, error);
  return null;
}
