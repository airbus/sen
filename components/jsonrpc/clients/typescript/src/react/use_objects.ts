// === use_objects.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useSyncExternalStore } from "react";

import type { InterestHandle, ObjectHandle } from "../index.js";

/**
 * Reactive view of an interest's currently-matched object set. The returned array reference
 * is stable across renders until the interest's membership actually changes (an object is
 * added or removed); downstream `useMemo([objects, ...])` calls see a stable dep and hit
 * their memo cache. Returns the empty array (stable reference) when `interest` is null.
 *
 * Backed by `useSyncExternalStore` over the interest's `onObjectAdded` / `onObjectRemoved`
 * signals plus `InterestHandle.objects()`, which the Layer-3 handle now caches between
 * mutations. The hook does not need to allocate any state of its own.
 */
const EMPTY_OBJECTS: readonly ObjectHandle[] = Object.freeze([]);

export function useObjects(interest: InterestHandle | null): ObjectHandle[] {
  const subscribe = useCallback(
    (listener: () => void): (() => void) => {
      if (!interest) return () => undefined;
      // onObjectAdded fires synchronously for every existing match during subscribe; that's
      // fine for our purposes (useSyncExternalStore re-reads getSnapshot and skips re-render
      // when the cached array reference is unchanged).
      const cancelAdded = interest.onObjectAdded(listener);
      const cancelRemoved = interest.onObjectRemoved(listener);
      return () => {
        cancelAdded();
        cancelRemoved();
      };
    },
    [interest],
  );

  const getSnapshot = useCallback(
    (): ObjectHandle[] => (interest ? interest.objects() : (EMPTY_OBJECTS as ObjectHandle[])),
    [interest],
  );

  return useSyncExternalStore(subscribe, getSnapshot);
}
