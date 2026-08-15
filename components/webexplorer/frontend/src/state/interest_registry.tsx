// === interest_registry.tsx ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo } from "react";

import { makeStore, useSelector } from "@sen/client/react";
import type { InterestHandle } from "@sen/client";

// Connection-scoped registry of live InterestHandles; the InterestOwner writes, everyone
// else reads. Module-scoped so popouts sharing the React tree see the same set.

interface InterestRegistryState {
  readonly handles: ReadonlyMap<string, InterestHandle>;
  /** Errors mirrored from useInterest so non-owner consumers (e.g. QueryRow) can render
   *  failure states without owning the declaration. */
  readonly errors: ReadonlyMap<string, Error>;
  // Bus-bucketed index keyed on `${session}.${bus}`. Inner Map replaced only when THAT
  // bus's membership changes, so per-bus consumers (Overview cards) don't re-render on
  // every cross-bus interest open/close.
  readonly byBus: ReadonlyMap<string, ReadonlyMap<string, InterestHandle>>;
}

// Interest names are `${session}.${bus}.${query}`; returns null defensively if shorter.
function busPrefixOf(interestName: string): string | null {
  const first = interestName.indexOf(".");
  if (first <= 0) return null;
  const second = interestName.indexOf(".", first + 1);
  if (second <= first + 1) return null;
  return interestName.slice(0, second);
}

const interestStore = makeStore<InterestRegistryState>({
  handles: new Map(),
  errors: new Map(),
  byBus: new Map(),
});

export function registerInterest(name: string, handle: InterestHandle): void {
  interestStore.setState((prev) => {
    if (prev.handles.get(name) === handle) return prev;
    const nextHandles = new Map(prev.handles);
    nextHandles.set(name, handle);
    const busKey = busPrefixOf(name);
    if (busKey === null) return { ...prev, handles: nextHandles };
    const nextByBus = new Map(prev.byBus);
    const innerPrev = nextByBus.get(busKey);
    const innerNext = innerPrev ? new Map(innerPrev) : new Map<string, InterestHandle>();
    innerNext.set(name, handle);
    nextByBus.set(busKey, innerNext);
    return { ...prev, handles: nextHandles, byBus: nextByBus };
  });
}

export function unregisterInterest(name: string): void {
  interestStore.setState((prev) => {
    if (!prev.handles.has(name)) return prev;
    const nextHandles = new Map(prev.handles);
    nextHandles.delete(name);
    const busKey = busPrefixOf(name);
    if (busKey === null) return { ...prev, handles: nextHandles };
    const innerPrev = prev.byBus.get(busKey);
    if (!innerPrev || !innerPrev.has(name)) {
      return { ...prev, handles: nextHandles };
    }
    const nextByBus = new Map(prev.byBus);
    if (innerPrev.size === 1) {
      nextByBus.delete(busKey);
    } else {
      const innerNext = new Map(innerPrev);
      innerNext.delete(name);
      nextByBus.set(busKey, innerNext);
    }
    return { ...prev, handles: nextHandles, byBus: nextByBus };
  });
}

export function registerInterestError(name: string, error: Error): void {
  interestStore.setState((prev) => {
    if (prev.errors.get(name) === error) return prev;
    const next = new Map(prev.errors);
    next.set(name, error);
    return { ...prev, errors: next };
  });
}

export function unregisterInterestError(name: string): void {
  interestStore.setState((prev) => {
    if (!prev.errors.has(name)) return prev;
    const next = new Map(prev.errors);
    next.delete(name);
    return { ...prev, errors: next };
  });
}

/** No-op while `handle` is null. */
export function useRegisterInterest(name: string, handle: InterestHandle | null): void {
  useEffect(() => {
    if (!handle) return;
    registerInterest(name, handle);
    return () => unregisterInterest(name);
  }, [name, handle]);
}

/** No-op while `error` is null. */
export function useRegisterInterestError(name: string, error: Error | null): void {
  useEffect(() => {
    if (!error) return;
    registerInterestError(name, error);
    return () => unregisterInterestError(name);
  }, [name, error]);
}

export function useInterestErrorByName(name: string | null): Error | null {
  return useSelector(interestStore, (s) => (name ? s.errors.get(name) ?? null : null));
}

export function useInterestByName(name: string | null): InterestHandle | null {
  return useSelector(interestStore, (s) => (name ? s.handles.get(name) ?? null : null));
}

// Fallback for click-through from durable surfaces (watches, plot legends) whose source
// interest may not currently match the object. Re-evaluates on registry-membership flips
// only, not on per-interest match churn.
export function useInterestContaining(objectName: string | null): {
  interest: InterestHandle;
  name: string;
} | null {
  const handles = useSelector(interestStore, (s) => s.handles);
  return useMemo(() => {
    if (objectName === null) return null;
    for (const [name, handle] of handles) {
      if (handle.objectByName(objectName) !== undefined) return { interest: handle, name };
    }
    return null;
  }, [handles, objectName]);
}

export function useAllInterests(): Array<[string, InterestHandle]> {
  const handles = useSelector(interestStore, (s) => s.handles);
  return useMemo(() => Array.from(handles.entries()), [handles]);
}

// Any open query on the bus that surfaces `objectName` is enough; the Overview card
// uses this so a pinned object stays live without requiring a specific named query.
// Subscribes per-bus so churn on unrelated buses doesn't re-render every pinned card.
export function useInterestContainingOnBus(
  sessionName: string,
  busName: string,
  objectName: string,
): { interest: InterestHandle; name: string } | null {
  const busKey = `${sessionName}.${busName}`;
  const onBus = useSelector(interestStore, (s) => s.byBus.get(busKey) ?? null);
  return useMemo(() => {
    if (!onBus) return null;
    for (const [name, handle] of onBus) {
      if (handle.objectByName(objectName) !== undefined) return { interest: handle, name };
    }
    return null;
  }, [onBus, objectName]);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    interestStore.dispose();
  });
}
