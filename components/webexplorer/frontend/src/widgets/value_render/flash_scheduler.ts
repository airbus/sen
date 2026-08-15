// === flash_scheduler.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// One rAF loop + Map of expiries drives data-flash on every flashing cell; CSS
// reacts to the attribute, React never sees it.

import { inResumeGrace } from "../../state/visibility.js";

const expiryByEl = new Map<Element, number>();
let rafId: number | null = null;

function tick(): void {
  const now = performance.now();
  // Two-pass to avoid mutating the Map during iteration.
  let expired: Element[] | null = null;
  for (const [el, expireAt] of expiryByEl) {
    if (expireAt <= now) {
      if (expired === null) expired = [];
      expired.push(el);
    }
  }
  if (expired) {
    for (const el of expired) {
      el.removeAttribute("data-flash");
      expiryByEl.delete(el);
    }
  }
  if (expiryByEl.size === 0) {
    rafId = null;
    return;
  }
  rafId = requestAnimationFrame(tick);
}

// Repeated calls extend the visual; trailing-edge behavior.
export function scheduleFlash(el: Element, holdMs: number): void {
  // Suppress mass-flash on tab resume; the underlying motion already happened off-screen.
  if (inResumeGrace()) return;
  el.setAttribute("data-flash", "1");
  expiryByEl.set(el, performance.now() + holdMs);
  if (rafId === null) rafId = requestAnimationFrame(tick);
}

export function cancelFlash(el: Element): void {
  if (!expiryByEl.has(el)) return;
  expiryByEl.delete(el);
  el.removeAttribute("data-flash");
}
