// === popouts.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";
import { openPopout, type PopoutHandle } from "../ui/popout.js";

function createPopout(title: string, width: number, height: number) {
  const store = makeStore<{ handle: PopoutHandle | null }>({ handle: null });
  const actions = Object.freeze({
    open: () => {
      const existing = store.getState().handle;
      // A closed window does not throw on focus(), so test `closed` instead of catching.
      // focus() can still be refused, which is no reason to open a second window.
      if (existing && !existing.popup.closed) {
        try {
          existing.popup.focus();
        } catch {
          /* refused; the window is still there */
        }
        return;
      }
      const handle = openPopout({ title, width, height });
      if (!handle) {
        // eslint-disable-next-line no-console
        console.warn(`Popup blocked: allow pop-ups for this origin to pop the ${title} view out.`);
        return;
      }
      store.setState({ handle });
    },
    clearOnUnload: () => {
      store.setState({ handle: null });
    },
  });
  const usePopout = () => useSelector(store, (s) => s.handle);
  return { store, actions, usePopout };
}

const plots = createPopout("Plots - Sen Explorer", 1200, 720);
export const plotsPopoutActions = plots.actions;
export const usePlotsPopout = plots.usePopout;

const events = createPopout("Events - Sen Explorer", 1000, 720);
export const eventsPopoutActions = events.actions;
export const useEventsPopout = events.usePopout;

const watch = createPopout("Watch - Sen Explorer", 420, 720);
export const watchPopoutActions = watch.actions;
export const useWatchPopout = watch.usePopout;

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    plots.store.dispose();
    events.store.dispose();
    watch.store.dispose();
  });
}
