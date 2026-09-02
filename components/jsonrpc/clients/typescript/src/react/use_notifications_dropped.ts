// === use_notifications_dropped.ts ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { Client } from "../index.js";

/**
 * Running total of notifications the server has reported dropping on this connection.
 *
 * Non-zero means the displayed state is incomplete: property changes and events were dropped
 * under backpressure and there is no record of which. The count never decreases, so a component
 * that shows it needs its own dismissal if the notice should be clearable.
 *
 * Seeds from `client.droppedNotifications` rather than from zero, so a component mounted after
 * the drop happened still learns about it -- the server reports each window once and never
 * repeats it.
 */
export function useNotificationsDropped(client: Client | null): number {
  const [count, setCount] = useState<number>(() => client?.droppedNotifications ?? 0);

  useEffect(() => {
    if (!client) return;
    setCount(client.droppedNotifications);
    return client.onNotificationsDropped(() => {
      setCount(client.droppedNotifications);
    });
  }, [client]);

  return count;
}
