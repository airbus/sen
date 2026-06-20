// === use_object.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { InterestHandle, ObjectHandle } from "../index.js";

/**
 * Track a single named object within an interest. Returns the current handle (or `undefined`
 * if the name isn't in the match set). Re-renders when the object enters or leaves the set.
 *
 * The lookup is read synchronously on every render (never trailed through `useState`) so
 * when `name` or `interest` change the returned handle reflects the new identity in the very
 * same render. The kept state is purely a re-render trigger for add/remove notifications;
 * shadowing the lookup in state would let a consumer see a stale handle for one render after
 * a `name` change, which causes subscriptions intended for the new object to land on the old
 * one (e.g. an event subscription with the new class's event name issued against the
 * previous object; the server then rejects it as "unknown event").
 */
export function useObject(interest: InterestHandle | null, name: string): ObjectHandle | undefined {
  const [, forceRender] = useState(0);

  useEffect(() => {
    if (!interest) return;
    const bump = () => forceRender((n) => n + 1);
    const cancelAdded = interest.onObjectAdded((o) => {
      if (o.name === name) bump();
    });
    const cancelRemoved = interest.onObjectRemoved((removedName) => {
      if (removedName === name) bump();
    });
    return () => {
      cancelAdded();
      cancelRemoved();
    };
  }, [interest, name]);

  return interest?.objectByName(name);
}
