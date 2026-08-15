// === visibility.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Tab-visibility tracker. The WebSocket keeps delivering while backgrounded, but rAF is
// throttled to ~1 Hz so flushes pile up; on resume they drain as a catch-up burst of
// flashes/arrivals. Animations check inResumeGrace() and suppress themselves briefly.
// Module-scope side effect: subscribes to `document.visibilitychange` on import.

/** Long enough to absorb the resume burst, short enough that real post-resume activity animates. */
export const RESUME_GRACE_MS = 350;

let lastResumeAt = 0;

const onVisibilityChange = (): void => {
  if (document.visibilityState === "visible") {
    lastResumeAt = Date.now();
  }
};

if (typeof document !== "undefined") {
  document.addEventListener("visibilitychange", onVisibilityChange);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    if (typeof document !== "undefined") {
      document.removeEventListener("visibilitychange", onVisibilityChange);
    }
  });
}

/** 0 if the page has never been hidden since load (caller's `delivery.at > 0` then passes). */
export function getLastResumeAt(): number {
  return lastResumeAt;
}

export function inResumeGrace(): boolean {
  return Date.now() - lastResumeAt < RESUME_GRACE_MS;
}
