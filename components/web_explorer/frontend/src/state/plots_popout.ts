// === plots_popout.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

import { openPopout, type PopoutHandle } from "../ui/popout.js";

// Singleton: only one Plots popout ever exists. The click handler opens the popup
// synchronously (user gesture), the handle parks here, and <PopoutWindow> in app.tsx
// portals <PlotsView> into it so the React tree stays continuous.

interface PlotsPopoutState {
  readonly handle: PopoutHandle | null;
}

const store = makeStore<PlotsPopoutState>({ handle: null });

/** Must be called from a user gesture (popup blocker). */
function open(): void {
  const existing = store.getState().handle;
  if (existing) {
    try {
      existing.popup.focus();
      return;
    } catch {
      /* popup was closed out from under us; fall through and re-open */
    }
  }
  const handle = openPopout({ title: "Plots - Sen Explorer", width: 1200, height: 720 });
  if (!handle) {
    // eslint-disable-next-line no-console
    console.warn("Popup blocked: allow pop-ups for this origin to pop the Plots view out.");
    return;
  }
  store.setState({ handle });
}

/** Wired to <PopoutWindow onClose>; clears the parked handle on OS-side close. */
function clearOnUnload(): void {
  store.setState({ handle: null });
}

export const plotsPopoutActions = Object.freeze({ open, clearOnUnload });

export function usePlotsPopout(): PopoutHandle | null {
  return useSelector(store, (s) => s.handle);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    store.dispose();
  });
}
