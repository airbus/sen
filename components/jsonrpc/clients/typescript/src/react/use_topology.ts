// === use_topology.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { Client, SessionInfo } from "../index.js";

export interface UseTopologyResult {
  sessions: SessionInfo[];
  /** True until the first snapshot arrives. */
  loading: boolean;
}

/**
 * Reactive view of the connection's session/bus topology. Mounts a subscription to
 * `client.onTopologyChanged` and re-renders on every change. While `client` is `null` or
 * the initial snapshot hasn't arrived yet, `loading` is `true` and `sessions` is empty.
 */
export function useTopology(client: Client | null): UseTopologyResult {
  const [sessions, setSessions] = useState<SessionInfo[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!client) {
      setSessions([]);
      setLoading(true);
      return;
    }
    setLoading(true);
    return client.onTopologyChanged((next) => {
      setSessions(next);
      setLoading(false);
    });
  }, [client]);

  return { sessions, loading };
}
