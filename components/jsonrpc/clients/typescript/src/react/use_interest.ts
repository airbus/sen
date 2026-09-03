// === use_interest.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef, useState } from "react";

import type { Client, InterestHandle, PreSubscription } from "../index.js";

/** Inputs to {@link useInterest}; `name` must be unique for the lifetime of the client. */
export interface UseInterestArgs {
  name: string;
  query: string;
  subscribe?: PreSubscription;
}

/** What {@link useInterest} returns. `error` is set when `declareInterest` rejects. */
export interface UseInterestResult {
  handle: InterestHandle | null;
  error: Error | null;
}

/**
 * Declares an interest for the lifetime of the calling component and releases it on unmount.
 * Re-declares when `name`, `query`, or `subscribe` change. Use {@link useObjects} or
 * {@link useObject} to react to the match set; this hook is only the lifecycle wrapper.
 */
export function useInterest(client: Client | null, args: UseInterestArgs): UseInterestResult {
  const [handle, setHandle] = useState<InterestHandle | null>(null);
  const [error, setError] = useState<Error | null>(null);

  // Stabilize `subscribe` on content rather than reference so callers can pass an inline
  // object without forcing release+re-declare on every render. JSON.stringify only runs when
  // the reference itself changes; a memoized parent pays the cost once.
  const subKeyRef = useRef<{ ref: PreSubscription | undefined; key: string }>({
    ref: undefined,
    key: "null",
  });
  if (!Object.is(subKeyRef.current.ref, args.subscribe)) {
    subKeyRef.current = { ref: args.subscribe, key: JSON.stringify(args.subscribe ?? null) };
  }
  const subscribeKey = subKeyRef.current.key;
  const subscribe = useMemo(() => args.subscribe, [subscribeKey]);

  useEffect(() => {
    if (!client) return;
    let released = false;
    let local: InterestHandle | null = null;
    setError(null);
    setHandle(null);

    (async () => {
      try {
        local = await client.declareInterest({
          name: args.name,
          query: args.query,
          ...(subscribe !== undefined ? { subscribe } : {}),
        });
        if (released) {
          await local.release();
          return;
        }
        setHandle(local);
      } catch (err) {
        if (!released) setError(err instanceof Error ? err : new Error(String(err)));
      }
    })();

    return () => {
      released = true;
      setHandle(null);
      void local?.release();
    };
  }, [client, args.name, args.query, subscribe]);

  return { handle, error };
}
