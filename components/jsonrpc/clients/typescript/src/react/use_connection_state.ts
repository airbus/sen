// === use_connection_state.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { Client, ConnectionState } from "../index.js";

/**
 * Reactive view of `client.connectionState`. Returns the current state and re-renders on every
 * transition. When `client` is `null`, returns `"connecting"` (the initial state of a freshly
 * created client) so consumers can render a placeholder while `connect()` is in flight.
 */
export function useConnectionState(client: Client | null): ConnectionState {
  const [state, setState] = useState<ConnectionState>(() => client?.connectionState ?? "connecting");

  useEffect(() => {
    if (!client) return;
    setState(client.connectionState);
    return client.onConnectionStateChange(setState);
  }, [client]);

  return state;
}
