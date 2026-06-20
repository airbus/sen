// === value_row.tsx ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import {
  createContext,
  useContext,
  useEffect,
  useRef,
  type ReactNode,
  type RefObject,
} from "react";

import { cancelFlash, scheduleFlash } from "./flash_scheduler.js";

// Default OFF; surfaces that aren't in-place value updates (e.g. events log) leave it off.
export const FlashEnabledContext = createContext<boolean>(false);

// Lowercased; renderers narrow children by name, descendants clear once an ancestor matches.
export const ValueFilterContext = createContext<string>("");

// Subgrid row; parent must use the `auto minmax(0,1fr) ACTIONS_COLUMN_WIDTHpx` template.
export function ValueRow({
  flashKey,
  children,
}: {
  flashKey?: unknown;
  children: ReactNode;
}) {
  const ref = useRef<HTMLDivElement | null>(null);
  useFlash(flashKey, undefined, ref);
  return (
    <div
      ref={ref}
      className="property-row"
      style={{
        display: "grid",
        gridTemplateColumns: "subgrid",
        gridColumn: "1 / -1",
        columnGap: 6,
        alignItems: "baseline",
        // Vertical-only padding; horizontal would compound across nesting.
        padding: "2px 0",
        borderRadius: "var(--radius-sm)",
      }}
    >
      {children}
    </div>
  );
}

// Bursty surfaces should pass a longer holdMs so close updates merge into one glow.
export function useFlash(
  value: unknown,
  holdMs: number = 200,
  hostRef?: RefObject<HTMLElement | null>,
): void {
  const enabled = useContext(FlashEnabledContext);
  // Default true so call sites without a ref still flash.
  const onScreenRef = useRef(true);

  useEffect(() => {
    const el = hostRef?.current;
    if (!el) return;
    registerVisibility(el, onScreenRef);
    return () => unregisterVisibility(el);
  }, [hostRef]);

  useEffect(() => {
    if (!enabled) return;
    if (value === undefined) return;
    if (!onScreenRef.current) return;
    const el = hostRef?.current;
    if (!el) return;
    scheduleFlash(el, holdMs);
  }, [value, enabled, holdMs, hostRef]);

  useEffect(() => {
    // Capture el at mount so a mid-life ref swap can't cancel the wrong element.
    const el = hostRef?.current;
    if (!el) return;
    return () => cancelFlash(el);
  }, [hostRef]);
}

let sharedVisibilityObserver: IntersectionObserver | null = null;
const visibilityRefs = new WeakMap<Element, { current: boolean }>();

function ensureObserver(): IntersectionObserver {
  if (sharedVisibilityObserver) return sharedVisibilityObserver;
  sharedVisibilityObserver = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        const ref = visibilityRefs.get(entry.target);
        if (ref) ref.current = entry.isIntersecting;
      }
    },
    { rootMargin: "100px" },
  );
  return sharedVisibilityObserver;
}

function registerVisibility(el: Element, ref: { current: boolean }): void {
  visibilityRefs.set(el, ref);
  ensureObserver().observe(el);
}

function unregisterVisibility(el: Element): void {
  visibilityRefs.delete(el);
  sharedVisibilityObserver?.unobserve(el);
}
